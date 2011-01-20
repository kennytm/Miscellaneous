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
from tempfile import mkdtemp
from zipfile import ZipFile
from contextlib import closing
import shutil
import os
import os.path
from plistlib import readPlist
import lxml.html
import re


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
    parser.add_option('-x', '--xpwn', help='location where the "xpwn" binary is installed.')
    parser.add_option('-d', '--vfdecrypt', help='location where "vfdecrypt" binary is installed.')
    parser.add_option('-o', '--output', help='the directory where the extracted files are placed to.')
    (options, args) = parser.parse_args()
    
    if not args and not os.path.isdir(options.output):
        parser.error('Please supply the path to the IPSW file or an existing output directory that contains the extracted firmware.')
    
    parser.destroy()
    
    return (options, args)


_products = {
    'AppleTV2,1': 'Apple TV 2G',
    'iPad1,1': 'iPad',
    'iPhone1,1': 'iPhone',
    'iPhone1,2': 'iPhone 3G',
    'iPhone2,1': 'iPhone 3GS',
    'iPhone3,1': 'iPhone 4',
    'iPod1,1': 'iPod touch 1G',
    'iPod2,1': 'iPod touch 2G',
    'iPod3,1': 'iPod touch 3G',
    'iPod4,1': 'iPod touch 4G',
}

_parenthesis_sub = re.compile('\s|\([^)]+\)|\..+$').sub
_key_matcher = re.compile('\s*([\w ]+):\s*([a-fA-F\d]+)').search

def extract_zipfile(ipsw_path):
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


def decrypted_filename(path):
    (root, ext) = os.path.splitext(path)
    return root + '.decrypted' + ext


def build_file_decryption_map(plist_obj, key_map):
    for identity in plist_obj['BuildIdentities']:
        behavior = identity['Info']['RestoreBehavior']
        for key, content in identity['Manifest'].items():
            key_lower = key.lower()
            if key_lower.startswith('restore'):
                if key_lower == 'restoreramdisk' and behavior == 'Update':
                    key_lower = 'updateramdisk'
                else:
                    continue
                
            path = os.path.join(output_dir, content['Info']['Path'])
            dec_path = decrypted_filename(path)

            skip_reason = None
            level = 'Notice'
            if key_lower not in key_map:
                skip_reason = 'No decryption key'
            elif not os.path.exists(path):
                if os.path.exists(dec_path):
                    skip_reason = 'Already decrypted'
                    level = 'Info'
                else:
                    skip_reason = 'File does not exist'
                    
            if skip_reason:
                print("<{3}> Skipping {0} at '{1}': {2}".format(key, content['Info']['Path'], skip_reason, level))
            else:
                file_key_map[path] = {'dec_path': dec_path, 'keys': key_map[key_lower]}

    return file_key_map



def main():
    (options, args) = parse_options()
        
    output_dir = options.output
    should_extract = True
    
    if os.path.isdir(output_dir):
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
        extract_zipfile(args[0])

    build_manifest_file = os.path.join(output_dir, 'BuildManifest.plist')
    plist_obj = readPlist(build_manifest_file)

    key_map = get_decryption_info(plist_obj, output_dir, options.url)
    file_key_map = build_file_decryption_map(plist_obj, key_map)
    
    
    
    
    
    
if __name__ == '__main__':
    main()
    