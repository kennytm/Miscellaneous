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
macho.features.enable('vmaddr', 'symbol', 'strings')

from argparse import ArgumentParser
from collections import defaultdict

from macho.loader import MachOLoader
from macho.utilities import peekStructs
from cpu.arm.thread import Thread
from cpu.pointers import isParameter, Return, Parameter, StackPointer
from cpu.arm.instructions.core import isBLInstruction, CMPInstruction
from sym import SYMTYPE_CFSTRING

def parse_options():
    parser = ArgumentParser()
    parser.add_argument('--version', action='version', version='dump_stuff 0.1')
    parser.add_argument('-u', '--arch', default="armv7",
        help='the CPU architecture. Default to "armv7".')
    parser.add_argument('-y', '--sdk', default="/",
        help='the SDK root. Default to "/"')
    parser.add_argument('-c', '--cache',
        help='the dyld shared cache file to base on. Provide "-c=" to use the '
             'default path.')
    
    subparsers = parser.add_subparsers(help='commands')
    
    ccatom_parser = subparsers.add_parser('caatom',
        help='Dump CAAtom symbols from QuartzCore')
    ccatom_parser.add_argument('-p', '--format', default='table', choices=["table", "enum"],
        help='print format. Either "table" (default) or "enum".')
    ccatom_parser.add_argument('filename', nargs='?', default='QuartzCore',
        help='Path to QuartzCore file.')
    ccatom_parser.set_defaults(func=caatom_main)

    uisound_parser = subparsers.add_parser('uisound', help='Dump UISound filenames from AudioToolbox')
    uisound_parser.add_argument('--testvalue', default=1000, metavar='ID', type=int,
        help='first value of Sound ID.')
    uisound_parser.add_argument('audioToolbox', nargs='?', default='AudioToolbox',
        help='Path to AudioToolbox file.')
    uisound_parser.add_argument('coreMedia', nargs='?', default='CoreMedia',
        help='Path to CoreMedia file.')
    uisound_parser.set_defaults(func=uisound_main)

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
    with MachOLoader(inputfn, arch=opts.arch, sdk=opts.sdk, cache=opts.cache) as (mo,):
        if mo is None:
            print("Error: {0} is not found.".format(inputfn))
            return
    
        try:
            spc_sym = mo.symbols.any1('name', '_stringpool_contents')
            wl_sym = mo.symbols.any1('name', '_wordlist')
        except KeyError as e:
            print("Error: Symbol '{0}' not found.".format(e.args[0]))
            return
            
        count = (spc_sym.addr - wl_sym.addr) // 4
        if count <= 0:
            print("Error: Word list count '{0}' is invalid.".format(count))
            return
            
        mo.seek(mo.fromVM(wl_sym.addr))
        for strindex, atom in peekStructs(mo.file, mo.makeStruct('2H'), count):
            if atom:
                yield (atom, mo.derefString(strindex + spc_sym.addr))

#--------- UISound -------------------------------------------------------------

class UISoundOnBranchHolder(object):
    def __init__(self, mo):
        self.msa = mo.symbols.any
        self.msa1 = mo.symbols.any1
    
    def __call__(self, prevLoc, instr, thread_):
        funcsym = self.msa('addr', thread_.pcRaw)
        if funcsym is not None:
            fname = funcsym.name
            if fname == '_sprintf':
                formatPtr = thread_.r[1]
                filename = self.msa1('addr', formatPtr).name
                thread_.memory.set(thread_.r[0], filename.replace("%s/", ""))
            thread_.forceReturn()

class UISoundCoreMediaOnBranch(object):
    def __init__(self, mo):
        self.msa = mo.symbols.any
        self.msall = mo.symbols.all
    
    def __call__(self, prevLoc, instr, thread):
        msa = self.msa
        funcsym = msa('addr', thread.pcRaw)
        if funcsym is not None:
            fname = funcsym.name
            if fname == '_CFDictionaryCreate':
                keysPtr = thread.r[1]
                retval = 0
                if isinstance(keysPtr, int):
                    keysSym = msa('addr', keysPtr)
                    if keysSym is not None and 'ssids' in keysSym.name:
                        tmg = thread.memory.get
                        msall = self.msall
                        
                        valuesPtr = thread.r[2]
                        count = thread.r[3]                
                        
                        keyValueList = ( (tmg(keysPtr + 4*i), tmg(valuesPtr + 4*i)) for i in range(count))
                        theDict = {}
                        for key, valuePtr in keyValueList:
                            for sym in msall('addr', valuePtr):
                                if sym.symtype == SYMTYPE_CFSTRING:
                                    theDict[key] = sym.name
                                    break
                                    
                            
                        retval = thread.memory.alloc(theDict)
                thread.r[0] = retval
            
            elif fname == '_notify_register_mach_port':
                thread.r[0] = 1
            elif fname in ('_CelestialCFCreatePropertyList', '_lockdown_connect'):
                thread.r[0] = 0
            
            thread.forceReturn() 


def uisound_get_name_for_input(mo, thread, startAddr, value, testValues):
    mem = thread.memory
    
    retstr = mem.alloc("-")
    retbool = mem.alloc(0)
    
    thread.pc = startAddr
    thread.r[0] = Parameter("input", value)
    thread.r[1] = retstr
    thread.r[2] = retbool
    thread.lr = Return
    thread.sp = StackPointer(0)
    
    while thread.pcRaw != Return:
        instr = thread.execute()
        if isinstance(instr, CMPInstruction):
            (lhs, rhs) = (op.get(thread) for op in instr.operands)
            if isParameter(lhs, "input"):
                testValues.update((rhs-1, rhs, rhs+1))
            elif isParameter(rhs, "input"):
                testValues.update((lhs-1, lhs, lhs+1))
    
    filename = mem.get(retstr)
    hasSound = mem.get(retbool)
            
    mem.free(retstr)
    mem.free(retbool)
    
    return (filename, hasSound)
    
    
    

def uisound_print(fnmaps):
    for value, (phoneSound, podSound, category) in sorted(fnmaps.items()):
        print("|-\n| {0} || {1} || {2} || {3} || ".format(value, phoneSound, podSound, category))


def uisound_get_filenames(mo, f_sym, iphone_sound_sym, testValue):
    testValues = {testValue}
    exhaustedValues = set()
    startAddr = f_sym.addr

    thread = Thread(mo)
    thread.instructionSet = f_sym.isThumb
    thread.onBranch = UISoundOnBranchHolder(mo)
    
    fnmaps = {}
    
    while True:
        valueLeft = testValues - exhaustedValues
        if not valueLeft:
            break
        anyValue = valueLeft.pop()
        exhaustedValues.add(anyValue)

        thread.memory.set(iphone_sound_sym.addr, 1)
        (phoneSound, hasPhoneSound) = uisound_get_name_for_input(mo, thread, startAddr, anyValue, testValues)

        if hasPhoneSound:            
            thread.memory.set(iphone_sound_sym.addr, 0)
            podSound = uisound_get_name_for_input(mo, thread, startAddr, anyValue, testValues)[0]
        else:
            podSound = '-'
            
        if hasPhoneSound:
            fnmaps[anyValue] = [phoneSound, podSound, '-']
    
    return fnmaps



def uisound_fill_categories(mo, cat_addr, init_sym):
    thread = Thread(mo)
    thread.instructionSet = init_sym.isThumb
    thread.onBranch = UISoundCoreMediaOnBranch(mo)
    
    msa = mo.symbols.any
    mem = thread.memory
    tmg = mem.get
    tms = mem.set
    
    novol_sym = msa('name', '_gSystemSoundsWithNoVolumeAdjustment')
    if novol_sym:
        tms(novol_sym.addr, 1)
    tms(cat_addr, 0)

    thread.pc = init_sym.addr
    
    while thread.pcRaw != Return:
        thread.execute()
        catdict = tmg(cat_addr)
        if catdict:
            return tmg(catdict)
    
    return {}


def uisound_main(opts):
    with MachOLoader(opts.audioToolbox, opts.coreMedia, arch=opts.arch, sdk=opts.sdk, cache=opts.cache) as (mo, coreMediaMo):
        msa1 = mo.symbols.any1
        cmsa = coreMediaMo.symbols.any
        cmsa1 = coreMediaMo.symbols.any1
        try:
            f_sym = msa1('name', '__Z24GetFileNameForThisActionmPcRb')
            iphone_sound_sym = msa1('name', '__ZL12isPhoneSound')
            cat_sym = cmsa('name', '_gSystemSoundIDToCategory') or cmsa1('name', '__ZL24gSystemSoundIDToCategory')
            init_sym = cmsa('name', '_initializeCMSessionMgr') or cmsa1('name', '__ZL34cmsmInitializeSSIDCategoryMappingsv')
        except KeyError as e:
            print("Error: Symbol '{0}' not found.".format(e.args[0]))
            return
        
        fmaps = uisound_get_filenames(mo, f_sym, iphone_sound_sym, opts.testvalue)
        catdict = uisound_fill_categories(coreMediaMo, cat_sym.addr, init_sym)
        for ssid, cat in catdict.items():
            if cat != 'UserAlert':
                fmaps[ssid][-1] = cat

        uisound_print(fmaps)
        

#--------- etc -----------------------------------------------------------------

def main():
    opts = parse_options()
    opts.func(opts)


if __name__ == '__main__':
    main()
