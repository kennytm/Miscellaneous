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
    
    if not args:
        parser.error('Please supply the path to the IPSW file.')
        options = None
    
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


def main():
    (options, args) = parse_options()
    
    ipsw_path = args[0]
    
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
            
    build_manifest_file = os.path.join(output_dir, 'BuildManifest.plist')
    plist_obj = readPlist(build_manifest_file)
    
    product_type = plist_obj['SupportedProductTypes'][0]
    product_name = _products.get(product_type, product_type)
    version = plist_obj['ProductVersion']
    
    build_identity = plist_obj['BuildIdentities'][0]
    build_info = build_identity['Info']
    build_train = build_info['BuildTrain']
    build_number = build_info['BuildNumber']
    device_class = build_info['DeviceClass']
    
    print("<Info> {0} ({1}), class {2}".format(product_name, product_type, device_class))
    print("<Info> iOS version {0}, build {1} {2}".format(version, build_train, build_number))
    
    

    
if __name__ == '__main__':
    main()
    