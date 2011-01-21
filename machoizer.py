#!/usr/bin/env python3.1
#
#    machoizer.py ... Make the entire file Mach-O __TEXT,__text section
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

import sys
sys.path.append('./EcaFretni/')

from optparse import OptionParser
from macho.arch import Arch
from macho.loadcommands.loadcommand import LC_SEGMENT, LC_SEGMENT_64
import os
from struct import Struct
from shutil import copyfileobj

def parse_options():
    parser = OptionParser(usage='usage: %prog [options] <input>', version='%prog 0.0')
    parser.add_option('-y', '--arch', help='the CPU architecture. Default to "armv7".', default="armv7")
    parser.add_option('-o', '--output', help='output file. If not supplied, the result will be written to "<input>.o".', metavar='FILE')
    parser.add_option('-a', '--address', help='VM address this file should map to. Default to 0', default='0', metavar='VMADDR')
    (options, args) = parser.parse_args()
    
    if not args:
        parser.error('Please supply the input filename.')
    
    inputfn = args[0]
    outputfn = options.output or args[0] + ".o"
    vmaddr = int(options.address, 16)
    
    parser.destroy()
    
    return (options.arch, inputfn, outputfn, vmaddr)





def main():
    (archstr, inputfn, outputfn, vmaddr) = parse_options()
    arch = Arch(archstr)
    
    with open(inputfn, 'rb') as fin, open(outputfn, 'wb') as fout:
        fin.seek(0, os.SEEK_END)
        filesize = fin.tell()
        fin.seek(0, os.SEEK_SET)
        excess = 16 - (filesize & 16)
        filesize += excess
        
        endian = arch.endian
        is64bit = arch.is64bit
        
        # prepare mach_header(_64)
        cputype = arch.cputype
        cpusubtype = arch.cpusubtypePacked
        if is64bit:
            magic = 0xfeedfacf
            mach_header = Struct(endian + '7I4x')
        else:
            magic = 0xfeedface
            mach_header = Struct(endian + '7I')


        # prepare segment_command(_64)
        if is64bit:
            segment_command = Struct(endian + '2I16s4Q4I')
            cmd = LC_SEGMENT_64
        else:
            segment_command = Struct(endian + '2I16s8I')
            cmd = LC_SEGMENT
        cmdsize = segment_command.size
        protlevel = 5
        
        
        # prepare section(_64)
        if is64bit:
            section_stru = Struct(endian + '16s16s2Q7I4x')
        else:
            section_stru = Struct(endian + '16s16s9I')
        cmdsize += section_stru.size
        fileoff = cmdsize + mach_header.size
        
            
        header_bytes = mach_header.pack(magic, cputype, cpusubtype, 1, 1, cmdsize, 1)
        fout.write(header_bytes)
        segment_bytes = segment_command.pack(cmd, cmdsize, '__TEXT', vmaddr, filesize, fileoff, filesize, 5, 5, 1, 0)
        fout.write(segment_bytes)
        section_bytes = section_stru.pack('__text', '__TEXT', vmaddr, filesize, fileoff, 4, 0, 0, 0x80000400, 0, 0)
        fout.write(section_bytes)
        
        copyfileobj(fin, fout)
        fout.write(b'\0' * excess)


if __name__ == '__main__':
    main()