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

def main():
    (options, args) = parse_options()
    
    ipsw_path = args[0]


    
if __name__ == '__main__':
    main()
    