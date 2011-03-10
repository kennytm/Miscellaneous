#!/usr/bin/env python3.2
#
#    dump_caatom.py ... Print the list of predefined CAAtom from QuartzCore
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
import macho.features
macho.features.enable('vmaddr', 'symbol')

from argparse import ArgumentParser

from macho.macho import MachO
from macho.utilities import peekStructs

def parse_options():
    parser = ArgumentParser()
    parser.add_argument('--version', action='version', version='dump_stuff 0.1')
    parser.add_argument('-y', '--arch', help='the CPU architecture. Default to "armv7".', default="armv7")
    
    subparsers = parser.add_subparsers(help='commands')
    ccatom_parser = subparsers.add_parser('caatom', help='Dump CAAtom symbols from QuartzCore')
    ccatom_parser.add_argument('-s', '--stringpool', help='the symbol that contains the string pool.', default='_stringpool_contents', metavar='SYM')
    ccatom_parser.add_argument('-w', '--wordlist', help='the symbol that contains the word list.', default='_wordlist', metavar='SYM')
    ccatom_parser.add_argument('-n', '--count', help='the number of words in the word list. By default, it will continue until hitting the _stringpool_contents symbol.')
    ccatom_parser.add_argument('-p', '--format', help='print format. Either "table" (default) or "enum".', default='table', choices=["table", "enum"])
    ccatom_parser.set_defaults(func=caatom_main)

    parser.add_argument('filename', help='Path to file to dump')
    return parser.parse_args()


#--------- CAAtom --------------------------------------------------------------

def caatom_main(opts):
    atoms = sorted(get_atoms(opts))

    if opts.format == 'enum':
        print("enum CAInternalAtom {")
        for atom, string in atoms:
            print("\tkCAInternalAtom_{0} = {1},".format(string, atom))
        print("\tkCAInternalAtomCount = {0}\n}};\n".format(max(a[0] for a in atoms)+1));
    else:
        print('--dec  --hex   string')
        for atom, string in atoms:
            print("{1:5}  {1:5x}   {0}".format(string, atom))


def get_atoms(opts):
    inputfn = opts.filename
    with MachO(inputfn, opts.arch) as mo:
        try:
            spc_sym = mo.symbols.any1('name', opts.stringpool)
            wl_sym = mo.symbols.any1('name', opts.wordlist)
        except KeyError as e:
            print("Error: Symbol '{0}' not found.".format(e.args[0]))
            return
            
        count = opts.count
        if count is None:
            count = (spc_sym.addr - wl_sym.addr) // 4
        if count <= 0:
            print("Error: Word list count '{0}' is invalid. Please rerun with a corrected the --count option.".format(count))
            return
            
        mo.seek(mo.fromVM(wl_sym.addr))
        for strindex, atom in peekStructs(mo.file, mo.makeStruct('2H'), count):
            if atom:
                yield (atom, mo.derefString(strindex + spc_sym.addr))
        

#--------- etc -----------------------------------------------------------------

def main():
    opts = parse_options()
    opts.func(opts)


if __name__ == '__main__':
    main()
