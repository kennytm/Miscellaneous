#!/usr/bin/env python3.1
#    
#    ipsw_decrypt.py ... Extract and decrypt all objects in an IPSW file.
#    Copyright (C) 2011  KennyTM~ <kennytm@gmail.com>
#    
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#    
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#    
#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

from optparse import OptionParser
from tempfile import mkdtemp, NamedTemporaryFile
from zipfile import ZipFile
from contextlib import closing
import shutil
import os
import os.path
from plistlib import readPlist
import lxml.html
import re
from struct import Struct
import subprocess
from lzss import decompressor


class TemporaryDirectory(object):
    def __init__(self):
        self._should_del = True
        pass
        
    def __enter__(self):
        self._tempdir = mkdtemp()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        if self._should_del:
            shutil.rmtree(self._tempdir)
            
    def move(self, target_dir):
        os.rename(self._tempdir, target_dir)
        self._should_del = False
        self._tempdir = target_dir
        
    @property
    def directory(self):
        return self._tempdir


def parse_options():
    parser = OptionParser(usage='usage: %prog [options] path/to/ipsw', version='%prog 0.0')
    parser.add_option('-u', '--url', help='the URL to download the decryption keys.')
    parser.add_option('-d', '--vfdecrypt', help='location where "vfdecrypt" binary is installed.', default='vfdecrypt')
    parser.add_option('-o', '--output', help='the directory where the extracted files are placed to.')
    (options, args) = parser.parse_args()
    
    if not args and not os.path.isdir(options.output):
        parser.error('Please supply the path to the IPSW file or an existing output directory that contains the extracted firmware.')
    
    parser.destroy()
    
    return (options, args)


_products = {
    'AppleTV2,1': 'Apple TV 2G',
    'iPad1,1': 'iPad',
    'iPad2,1': 'iPad 2 Wi-Fi',
    'iPad2,2': 'iPad 2 GSM',
    'iPad2,3': 'iPad 2 CDMA',
    'iPhone1,1': 'iPhone',
    'iPhone1,2': 'iPhone 3G',
    'iPhone2,1': 'iPhone 3GS',
    'iPhone3,1': 'iPhone 4',
    'iPhone3,3': 'iPhone 4 CDMA',
    'iPod1,1': 'iPod touch 1G',
    'iPod2,1': 'iPod touch 2G',
    'iPod3,1': 'iPod touch 3G',
    'iPod4,1': 'iPod touch 4G',
}

_parenthesis_sub = re.compile('\s|\([^)]+\)|\..+$').sub
_key_matcher = re.compile('\s*([\w ]+):\s*([a-fA-F\d]+)').search

def extract_zipfile(ipsw_path, output_dir):
    with TemporaryDirectory() as td:
        print("<Info> Extracting content from {0}, it may take a minute...".format(ipsw_path))
        with closing(ZipFile(ipsw_path)) as zipfile:
            zipfile.extractall(td.directory)

        if output_dir is None:
            build_manifest_file = os.path.join(td.directory, 'BuildManifest.plist')
            plist_obj = readPlist(build_manifest_file)
            product_type = plist_obj['SupportedProductTypes'][0]
            product_name = _products.get(product_type, product_type)
            version = plist_obj['ProductVersion']
            build = plist_obj['ProductBuildVersion']
            
            output_dir = '{0}, {1} ({2})'.format(product_name, version, build)
        
        td.move(output_dir)
        print("<Info> Extracted firmware to '{0}'. You may use the '-o \"{0}\"' switch in the future to skip this step.".format(output_dir))
    
    return output_dir


_header_replacement_get = {
    'mainfilesystem': 'os',
    'rootfilesystem': 'os',
    'glyphcharging': 'batterycharging',
    'glyphplugin': 'batteryplugin',
}.get


def get_decryption_info(plist_obj, output_dir, url=None):
    product_type = plist_obj['SupportedProductTypes'][0]
    product_name = _products.get(product_type, product_type)
    version = plist_obj['ProductVersion']
    
    build_info = plist_obj['BuildIdentities'][0]['Info']
    build_train = build_info['BuildTrain']
    build_number = build_info['BuildNumber']
    device_class = build_info['DeviceClass']
    
    print("<Info> {0} ({1}), class {2}".format(product_name, product_type, device_class))
    print("<Info> iOS version {0}, build {1} {2}".format(version, build_train, build_number))

    if url is None:
        url = 'http://theiphonewiki.com/wiki/index.php?title={0}_{1}_({2})'.format(build_train, build_number, product_name.translate({0x20:'_'}))

    print("<Info> Downloading decryption keys from '{0}'...".format(url))

    try:
        htmldoc = lxml.html.parse(url)
    except IOError as e:
        print("<Error> {1}".format(url, e))
        return None
        
    headers = htmldoc.iterfind('//h3/span[@class="mw-headline"]')
    key_map = {}
    for tag in headers:
        header_name = _parenthesis_sub('', tag.text_content()).strip().lower()
        header_name = _header_replacement_get(header_name, header_name)
        ul = tag.getparent().getnext()
        keys = {}
        for li in ul.iterchildren('li'):
            m = _key_matcher(li.text_content())
            if m:
                (key_type, key_value) = m.groups()
                keys[key_type] = key_value
        key_map[header_name] = keys
    
    print("<Info> Retrieved {0} keys.".format(len(key_map)))
    return key_map


tag_unpack = Struct('<4s2I').unpack
kbag_unpack = Struct('<2I16s').unpack



def decrypt_img3(filename, outputfn, keystring, ivstring, openssl='openssl'):
    basename = os.path.split(filename)[1]

    with open(filename, 'rb') as f:
        magic = f.read(4)
        if magic != b'3gmI':
            print("<Warning> '{0}' is not a valid IMG3 file. Skipping.".format(basename))
            return
        f.seek(16, os.SEEK_CUR)
        
        while True:
            tag = f.read(12)
            if not tag:
                break
            (tag_type, total_len, data_len) = tag_unpack(tag)
            data_len &= ~15
            
            if tag_type == b'ATAD':
                print("<Info> Decrypting '{0}'... ".format(basename))
                aes_len = str(len(keystring)*4)
                # OUCH!
                # Perhaps we an OpenSSL wrapper for Python 3.1
                # (although it is actually quite fast now)
                p = subprocess.Popen([openssl, 'aes-'+aes_len+'-cbc', '-d', '-nopad', '-K', keystring, '-iv', ivstring, '-out', outputfn], stdin=subprocess.PIPE)
                bufsize = 16384
                buf = bytearray(bufsize)
                while data_len:
                    bytes_to_read = min(data_len, bufsize)
                    data_len -= bytes_to_read
                    if bytes_to_read < bufsize:
                        del buf[bytes_to_read:]
                    f.readinto(buf)
                    p.stdin.write(buf)
                p.stdin.close()
                if p.wait() != 0 or not os.path.exists(outputfn):
                    print("<Error> Decryption failed!")
                return
                
            else:
                f.seek(total_len - 12, os.SEEK_CUR)
                
        print("<Warning> Nothing was decrypted from '{0}'".format(basename))



def vfdecrypt(filename, outputfn, keystring, bin='vfdecrypt'):
    basename = os.path.split(filename)[1]
    print("<Info> Decrypting '{0}', it may take a minute...".format(basename))
    try:
        retcode = subprocess.call([bin, '-i', filename, '-k', keystring, '-o', outputfn])
    except OSError as e:
        print("<Error> Received exception '{0}' when trying to run '{1}'.".format(e, bin))
    else:
        if retcode:
            print("<Error> VFDecrypt of '{1}' failed with error code {0}.".format(retcode, basename))



def decrypted_filename(path):
    (root, ext) = os.path.splitext(path)
    return root + '.decrypted' + ext


def build_file_decryption_map(plist_obj, key_map, output_dir):
    file_key_map = {}
    for identity in plist_obj['BuildIdentities']:
        behavior = identity['Info']['RestoreBehavior']
        for key, content in identity['Manifest'].items():
            key_lower = key.lower()
            if behavior == 'Update':
                if key_lower == 'restoreramdisk':
                    key_lower = 'updateramdisk'
                else:
                    continue
            elif key_lower.startswith('restore') and key_lower != 'restoreramdisk':
                continue
                
            path = os.path.join(output_dir, content['Info']['Path'])
            dec_path = decrypted_filename(path)

            skip_reason = None
            level = 'Notice'
            if os.path.exists(dec_path):
                skip_reason = 'Already decrypted'
                level = 'Info'
            elif key_lower not in key_map:
                skip_reason = 'No decryption key'
            elif not os.path.exists(path):
                skip_reason = 'File does not exist'
                    
            if skip_reason:
                print("<{3}> Skipping {0} ({1}): {2}".format(key, os.path.split(content['Info']['Path'])[1], skip_reason, level))
            else:
                file_key_map[path] = {'dec_path': dec_path, 'keys': key_map[key_lower]}

    return file_key_map



def main():
    (options, args) = parse_options()
        
    output_dir = options.output
    should_extract = True
    
    if output_dir and os.path.isdir(output_dir):
        build_manifest_file = os.path.join(output_dir, 'BuildManifest.plist')
        if os.path.exists(build_manifest_file):
            print("<Notice> Output directory '{0}' already exists. Assuming the IPSW has been extracted to this directory.".format(output_dir))
            should_extract = False
        else:
            print("<Warning> Output directory '{0}' already exists.".format(output_dir))
        
        
    if should_extract:
        if not args:
            print("<Error> Please supply the path to the IPSW file.")
            return
        output_dir = extract_zipfile(args[0], output_dir)

    build_manifest_file = os.path.join(output_dir, 'BuildManifest.plist')
    plist_obj = readPlist(build_manifest_file)

    key_map = get_decryption_info(plist_obj, output_dir, options.url)
    file_key_map = build_file_decryption_map(plist_obj, key_map, output_dir)
    
    for filename, info in file_key_map.items():
        keys = info['keys']
        dec_path = info['dec_path']

        if 'Key' in keys and 'IV' in keys:
            decrypt_img3(filename, info['dec_path'], keys['Key'], keys['IV'])
        elif 'VFDecrypt Key' in keys:
            vfdecrypt(filename, info['dec_path'], keys['VFDecrypt Key'], options.vfdecrypt)
    
        if os.path.exists(dec_path):
            try:
                fin = open(dec_path, 'rb')     
                decomp_func = decompressor(fin)
                if decomp_func is not None:
                    with NamedTemporaryFile() as fout:
                        decomp_func(fin, fout, report_progress=True)
                        fin.close()
                        fin = None
                        os.rename(fout.name, dec_path)
                        with open(fout.name, 'wb'):
                            pass
            finally:
                if fin:
                    fin.close()

    
if __name__ == '__main__':
    main()
    