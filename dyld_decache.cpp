/*
    dyld_decache.cpp ... Extract dylib files from shared cache.
    Copyright (C) 2011  KennyTM~ <kennytm@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
   With reference to DHowett's dyldcache.cc, with the following condition:

    "if you find it useful, do whatever you want with it. just don't forget that
     somebody helped."

   see http://blog.howett.net/?p=75 for detail.
*/

/*
    Part of code is referenced from Apple's dyld project, with the following li-
    cense:
*/

    /* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
        *
        * Copyright (c) 2006-2008 Apple Inc. All rights reserved.
        *
        * @APPLE_LICENSE_HEADER_START@
        *
        * This file contains Original Code and/or Modifications of Original Code
        * as defined in and that are subject to the Apple Public Source License
        * Version 2.0 (the 'License'). You may not use this file except in
        * compliance with the License. Please obtain a copy of the License at
        * http://www.opensource.apple.com/apsl/ and read it before using this
        * file.
        *
        * The Original Code and all software distributed under the License are
        * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
        * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
        * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
        * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
        * Please see the License for the specific language governing rights and
        * limitations under the License.
        *
        * @APPLE_LICENSE_HEADER_END@
        */

//------------------------------------------------------------------------------
// END LEGALESE
//------------------------------------------------------------------------------

// g++ -o dyld_decache -O3 -Wall -Wextra -std=c++98 /usr/local/lib/libboost_filesystem-mt.a /usr/local/lib/libboost_system-mt.a dyld_decache.cpp DataFile.cpp

#include <unistd.h>
#include <cstdio>
#include <stdint.h>
#include <getopt.h>
#include "DataFile.h"
#include <string>
#include <vector>
#define BOOST_FILESYSTEM_VERSION 3
#include <boost/filesystem.hpp>
#include <utility>
#include <boost/unordered_map.hpp>
#include <boost/foreach.hpp>

struct dyld_cache_header {
	char		magic[16];
	uint32_t	mappingOffset;
	uint32_t	mappingCount;
	uint32_t	imagesOffset;
	uint32_t	imagesCount;
	uint64_t	dyldBaseAddress;
};

typedef uint64_t		mach_vm_address_t;
typedef uint64_t		mach_vm_offset_t;
typedef uint64_t		mach_vm_size_t;
typedef int32_t vm_prot_t;

struct shared_file_mapping_np {
	mach_vm_address_t	sfm_address;
	mach_vm_size_t		sfm_size;
	mach_vm_offset_t	sfm_file_offset;
	vm_prot_t		sfm_max_prot;
	vm_prot_t		sfm_init_prot;
};

struct dyld_cache_image_info {
	uint64_t	address;
	uint64_t	modTime;
	uint64_t	inode;
	uint32_t	pathFileOffset;
	uint32_t	pad;
};

typedef int32_t		integer_t;
typedef integer_t	cpu_type_t;
typedef integer_t	cpu_subtype_t;

struct mach_header {
	uint32_t	magic;
	cpu_type_t	cputype;
	cpu_subtype_t	cpusubtype;
	uint32_t	filetype;
	uint32_t	ncmds;
	uint32_t	sizeofcmds;
	uint32_t	flags;
};

struct load_command {
	uint32_t cmd;
	uint32_t cmdsize;
};

#define LC_REQ_DYLD 0x80000000

#define	LC_SEGMENT	0x1
#define	LC_SYMTAB	0x2
#define	LC_SYMSEG	0x3
#define	LC_THREAD	0x4
#define	LC_UNIXTHREAD	0x5
#define	LC_LOADFVMLIB	0x6
#define	LC_IDFVMLIB	0x7
#define	LC_IDENT	0x8
#define LC_FVMFILE	0x9
#define LC_PREPAGE      0xa
#define	LC_DYSYMTAB	0xb
#define	LC_LOAD_DYLIB	0xc
#define	LC_ID_DYLIB	0xd
#define LC_LOAD_DYLINKER 0xe
#define LC_ID_DYLINKER	0xf
#define	LC_PREBOUND_DYLIB 0x10
#define	LC_ROUTINES	0x11
#define	LC_SUB_FRAMEWORK 0x12
#define	LC_SUB_UMBRELLA 0x13
#define	LC_SUB_CLIENT	0x14
#define	LC_SUB_LIBRARY  0x15
#define	LC_TWOLEVEL_HINTS 0x16
#define	LC_PREBIND_CKSUM  0x17
#define	LC_LOAD_WEAK_DYLIB (0x18 | LC_REQ_DYLD)
#define	LC_SEGMENT_64	0x19
#define	LC_ROUTINES_64	0x1a
#define LC_UUID		0x1b
#define LC_RPATH       (0x1c | LC_REQ_DYLD)
#define LC_CODE_SIGNATURE 0x1d
#define LC_SEGMENT_SPLIT_INFO 0x1e
#define LC_REEXPORT_DYLIB (0x1f | LC_REQ_DYLD)
#define	LC_LAZY_LOAD_DYLIB 0x20
#define	LC_ENCRYPTION_INFO 0x21
#define	LC_DYLD_INFO 	0x22
#define	LC_DYLD_INFO_ONLY (0x22|LC_REQ_DYLD)
#define LC_LOAD_UPWARD_DYLIB (0x23|LC_REQ_DYLD)
#define LC_VERSION_MIN_MACOSX 0x24
#define LC_VERSION_MIN_IPHONEOS 0x25
#define LC_FUNCTION_STARTS 0x26
#define LC_DYLD_ENVIRONMENT 0x27

struct segment_command : public load_command {
	char		segname[16];
	uint32_t	vmaddr;
	uint32_t	vmsize;
	uint32_t	fileoff;
	uint32_t	filesize;
	vm_prot_t	maxprot;
	vm_prot_t	initprot;
	uint32_t	nsects;
	uint32_t	flags;
};

struct section {
	char		sectname[16];
	char		segname[16];
	uint32_t	addr;
	uint32_t	size;
	uint32_t	offset;
	uint32_t	align;
	uint32_t	reloff;
	uint32_t	nreloc;
	uint32_t	flags;
	uint32_t	reserved1;
	uint32_t	reserved2;
};

struct symtab_command : public load_command {
	uint32_t	symoff;
	uint32_t	nsyms;
	uint32_t	stroff;
	uint32_t	strsize;
};

struct symseg_command : public load_command {
	uint32_t	offset;
	uint32_t	size;
};

struct dysymtab_command : public load_command {
    uint32_t ilocalsym;
    uint32_t nlocalsym;
    uint32_t iextdefsym;
    uint32_t nextdefsym;
    uint32_t iundefsym;
    uint32_t nundefsym;
    uint32_t tocoff;
    uint32_t ntoc;
    uint32_t modtaboff;
    uint32_t nmodtab;
    uint32_t extrefsymoff;
    uint32_t nextrefsyms;
    uint32_t indirectsymoff;
    uint32_t nindirectsyms;
    uint32_t extreloff;
    uint32_t nextrel;
    uint32_t locreloff;
    uint32_t nlocrel;
};

struct twolevel_hints_command : public load_command {
    uint32_t offset;
    uint32_t nhints;
};

struct segment_command_64 : public load_command {
	char		segname[16];
	uint64_t	vmaddr;
	uint64_t	vmsize;
	uint64_t	fileoff;
	uint64_t	filesize;
	vm_prot_t	maxprot;
	vm_prot_t	initprot;
	uint32_t	nsects;
	uint32_t	flags;
};

struct section_64 {
	char		sectname[16];
	char		segname[16];
	uint64_t	addr;
	uint64_t	size;
	uint32_t	offset;
	uint32_t	align;
	uint32_t	reloff;
	uint32_t	nreloc;
	uint32_t	flags;
	uint32_t	reserved1;
	uint32_t	reserved2;
	uint32_t	reserved3;
};

struct linkedit_data_command : public load_command {
    uint32_t	dataoff;
    uint32_t	datasize;
};

struct encryption_info_command : public load_command {
   uint32_t	cryptoff;
   uint32_t	cryptsize;
   uint32_t	cryptid;
};

struct dyld_info_command : public load_command {
    uint32_t   rebase_off;
    uint32_t   rebase_size;
    uint32_t   bind_off;
    uint32_t   bind_size;
    uint32_t   weak_bind_off;
    uint32_t   weak_bind_size;
    uint32_t   lazy_bind_off;
    uint32_t   lazy_bind_size;
    uint32_t   export_off;
    uint32_t   export_size;
};

struct dylib {
    uint32_t name;
    uint32_t timestamp;
    uint32_t current_version;
    uint32_t compatibility_version;
};

struct dylib_command : public load_command {
    struct dylib dylib;
};

struct nlist {
	int32_t n_strx;
	uint8_t n_type;
	uint8_t n_sect;
	int16_t n_desc;
	uint32_t n_value;
};

struct class_t {
    uint32_t isa;
    uint32_t superclass;
    uint32_t cache;
    uint32_t vtable;
    uint32_t data;
};

struct class_ro_t {
    uint32_t flags;
    uint32_t instanceStart;
    uint32_t instanceSize;
    uint32_t ivarLayout;
    uint32_t name;
    uint32_t baseMethods;
    uint32_t baseProtocols;
    uint32_t ivars;
    uint32_t weakIvarLayout;
    uint32_t baseProperties;
};

struct method_t {
    uint32_t name;
    uint32_t types;
    uint32_t imp;
};

struct property_t {
    uint32_t name;
    uint32_t attributes;
};

struct protocol_t {
    uint32_t isa;
    uint32_t name;
    uint32_t protocols;
    uint32_t instanceMethods;
    uint32_t classMethods;
    uint32_t optionalInstanceMethods;
    uint32_t optionalClassMethods;
    uint32_t instanceProperties;
};

struct category_t {
    uint32_t name;
    uint32_t cls;
    uint32_t instanceMethods;
    uint32_t classMethods;
    uint32_t protocols;
    uint32_t instanceProperties;
};

#define BIND_OPCODE_MASK					0xF0
#define BIND_IMMEDIATE_MASK					0x0F
#define BIND_OPCODE_DONE					0x00
#define BIND_OPCODE_SET_DYLIB_ORDINAL_IMM			0x10
#define BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB			0x20
#define BIND_OPCODE_SET_DYLIB_SPECIAL_IMM			0x30
#define BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM		0x40
#define BIND_OPCODE_SET_TYPE_IMM				0x50
#define BIND_OPCODE_SET_ADDEND_SLEB				0x60
#define BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB			0x70
#define BIND_OPCODE_ADD_ADDR_ULEB				0x80
#define BIND_OPCODE_DO_BIND					0x90
#define BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB			0xA0
#define BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED			0xB0
#define BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB		0xC0


//------------------------------------------------------------------------------
// END THIRD-PARTY STRUCTURES
//------------------------------------------------------------------------------

// Check if two strings are equal within 16 characters.
// Used for comparing segment and section names.
static bool streq(const char x[16], const char* y) {
    return strncmp(x, y, 16) == 0;
}

static long write_uleb128(FILE* f, unsigned u) {
    uint8_t buf[16];
    int byte_count = 0;
    while (u) {
        buf[byte_count++] = u | 0x80;
        u >>= 7;
    }
    buf[byte_count-1] &= ~0x80;
    fwrite(buf, byte_count, sizeof(*buf), f);
    return byte_count;
}

static boost::filesystem::path remove_all_extensions(const char* the_path) {
    boost::filesystem::path retval (the_path);
    do {
        retval = retval.stem();
    } while (!retval.extension().empty());
    return retval;
}



class ProgramContext;

// When dyld create the cache file, if it recognize common Objective-C strings
//  and methods across different libraries, they will be coalesced. However,
//  this poses a big trouble when decaching, because the references to the other
//  library will become a dangling pointer. This class is to store these
//  external references, and put them back in an extra section of the decached
//  library.
// ("String" is a misnomer because it can also store non-strings.)
class ExtraStringRepository {
    struct Entry {
        const char* string;
        size_t size;
        uint32_t new_address;
        std::vector<uint32_t> override_addresses;
    };

    boost::unordered_map<const char*, int> _indices;
    std::vector<Entry> _entries;
    size_t _total_size;

    section _template;

public:
    ExtraStringRepository(const char* segname, const char* sectname, uint32_t flags, uint32_t alignment) {
        memset(&_template, 0, sizeof(_template));
        strncpy(_template.segname, segname, 16);
        strncpy(_template.sectname, sectname, 16);
        _template.flags = flags;
        _template.align = alignment;
    }

    // Insert a piece of external data referred from 'override_address' to the
    //  repository.
    void insert(const char* string, size_t size, uint32_t override_address) {
        boost::unordered_map<const char*, int>::const_iterator it = _indices.find(string);
        if (it != _indices.end()) {
            _entries[it->second].override_addresses.push_back(override_address);
        } else {
            Entry entry;
            entry.string = string;
            entry.size = size;
            entry.new_address = this->next_vmaddr();
            entry.override_addresses.push_back(override_address);
            _indices.insert(std::make_pair(string, _entries.size()));
            _entries.push_back(entry);
            _template.size += size;
        }
    }

    void insert(const char* string, uint32_t override_address) {
        this->insert(string, strlen(string) + 1, override_address);
    }

    // Iterate over all external data in this repository.
    template <typename Object>
    void foreach_entry(const Object* self, void (Object::*action)(const char* string, size_t size, uint32_t new_address, const std::vector<uint32_t>& override_addresses) const) const {
        BOOST_FOREACH(const Entry& e, _entries) {
            (self->*action)(e.string, e.size, e.new_address, e.override_addresses);
        }
    }

    void increase_size_by(size_t delta) { _template.size += delta; }
    size_t total_size() const { return _template.size; }
    bool has_content() const { return _template.size != 0; }

    // Get the 'section' structure for the extra section this repository
    //  represents.
    section section_template() const { return _template; }

    void set_section_vmaddr(uint32_t vmaddr) { _template.addr = vmaddr; }
    void set_section_fileoff(uint32_t fileoff) { _template.offset = fileoff; }
    uint32_t next_vmaddr() const { return _template.addr + _template.size; }
};

class ExtraBindRepository {
    struct Entry {
        std::string symname;
        int libord;
        std::vector<std::pair<int, uint32_t> > replace_offsets;
    };
    
    boost::unordered_map<uint32_t, Entry> _entries;
    
public:
    bool contains(uint32_t target_address) const {
        return (_entries.find(target_address) != _entries.end());
    }
    
    template <typename Object>
    void insert(uint32_t target_address, std::pair<int, uint32_t> replace_offset, const Object* self, void (Object::*addr_info_getter)(uint32_t addr, std::string* p_symname, int* p_libord) const) {
        boost::unordered_map<uint32_t, Entry>::iterator it = _entries.find(target_address);
        if (it != _entries.end()) {
            it->second.replace_offsets.push_back(replace_offset);
        } else {
            Entry entry;
            entry.replace_offsets.push_back(replace_offset);
            (self->*addr_info_getter)(target_address, &entry.symname, &entry.libord);
            _entries.insert(std::make_pair(target_address, entry));
        }
    }
    
    long optimize_and_write(FILE* f) {
        typedef boost::unordered_map<uint32_t, Entry>::value_type V;
        typedef boost::unordered_map<int, std::vector<const Entry*> > M;
        typedef std::pair<int, uint32_t> P;
        
        M entries_by_libord;
        
        BOOST_FOREACH(V& pair, _entries) {
            Entry& entry = pair.second;
            std::sort(entry.replace_offsets.begin(), entry.replace_offsets.end());
            entries_by_libord[entry.libord].push_back(&entry);
        }
        
        fputc(BIND_OPCODE_SET_TYPE_IMM | 1, f);
        
        long size = 1;
        BOOST_FOREACH(const M::value_type& pair, entries_by_libord) {
            int libord = pair.first;
            if (libord < 0x10) {
                unsigned char imm = libord & BIND_IMMEDIATE_MASK;
                unsigned char opcode = libord < 0 ? BIND_OPCODE_SET_DYLIB_SPECIAL_IMM : BIND_OPCODE_SET_DYLIB_ORDINAL_IMM;
                fputc(opcode | imm, f);
                ++ size;
            } else {
                fputc(BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB, f);
                size += 1 + write_uleb128(f, libord);
            }
            
            BOOST_FOREACH(const Entry* entry, pair.second) {
                size_t string_len = entry->symname.size();
                size += string_len + 2;
                fputc(BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM, f);
                fwrite(entry->symname.c_str(), string_len+1, 1, f);
                
                int segnum = -1;
                uint32_t last_offset = 0;
                BOOST_FOREACH(P offset, entry->replace_offsets) {
                    if (offset.first != segnum) {
                        segnum = offset.first;
                        last_offset = offset.second + 4;
                        fputc(BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB | segnum, f);
                        size += 1 + write_uleb128(f, offset.second);
                    } else {
                        uint32_t delta = offset.second - last_offset;
                        unsigned imm_scale = delta % 4 == 0 ? delta / 4 : ~0u;
                        if (imm_scale == 0) {
                            fputc(BIND_OPCODE_DO_BIND, f);
                        } else if (imm_scale < 0x10u) {
                            fputc(BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED | imm_scale, f);
                        } else {
                            fputc(BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB, f);
                            size += write_uleb128(f, delta);
                        }
                        ++ size;
                        last_offset = offset.second + 4;
                    }
                }
                fputc(BIND_OPCODE_DO_BIND, f);
                ++ size;
            }
        }
        
        return size;
    }
};

// A simple structure which only provides services related to VM address.
class MachOFile {
protected:
    const mach_header* _header;
    const ProgramContext* _context;
    std::vector<const segment_command*> _segments;
    uint32_t _image_vmaddr;
    
private:
    boost::unordered_map<std::string, int> _libords;
    int _cur_libord;
    boost::unordered_map<uint32_t, std::string> _exports;

protected:
    template <typename T>
    void foreach_command(void(T::*action)(const load_command* cmd)) {
        const unsigned char* cur_cmd = reinterpret_cast<const unsigned char*>(_header + 1);

        for (uint32_t i = 0; i < _header->ncmds; ++ i) {
            const load_command* cmd = reinterpret_cast<const load_command*>(cur_cmd);
            cur_cmd += cmd->cmdsize;

            (static_cast<T*>(this)->*action)(cmd);
        }
    }

    // Convert VM address to file offset of the decached file _before_ inserting
    //  the extra sections.
    long from_vmaddr(uint32_t vmaddr) const {
        BOOST_FOREACH(const segment_command* segcmd, _segments) {
            if (segcmd->vmaddr <= vmaddr && vmaddr < segcmd->vmaddr + segcmd->vmsize)
                return vmaddr - segcmd->vmaddr + segcmd->fileoff;
        }
        return -1;
    }

private:
    void retrieve_segments_and_libords(const load_command* cmd);

public:
    // Checks if the VM address is included in the decached file _before_
    //  inserting the extra sections.
    bool contains_address(uint32_t vmaddr) const {
        BOOST_FOREACH(const segment_command* segcmd, _segments) {
            if (segcmd->vmaddr <= vmaddr && vmaddr < segcmd->vmaddr + segcmd->vmsize)
                return true;
        }
        return false;
    }
    
    MachOFile(const mach_header* header, const ProgramContext* context, uint32_t image_vmaddr = 0)
        : _header(header), _context(context), _image_vmaddr(image_vmaddr), _cur_libord(0)
    {
        if (header->magic != 0xfeedface)
            return;

        this->foreach_command(&MachOFile::retrieve_segments_and_libords);
    }

    const mach_header* header() const { return _header; }
    
    int libord_with_name(const char* libname) const {
        boost::unordered_map<std::string, int>::const_iterator cit = _libords.find(libname); 
        if (cit == _libords.end())
            return 0;
        else
            return cit->second;
    }
    
    std::string exported_symbol(uint32_t vmaddr) const {
        boost::unordered_map<uint32_t, std::string>::const_iterator cit = _exports.find(vmaddr);
        if (cit != _exports.end())
            return cit->second;
        else
            return "";
    }
};

// This class represents one file going to be decached.
// Decaching is performed in several phases:
//  1. Search for all Objective-C selectors and methods that point outside of
//     this library, and put this into an ExtraStringRepository.
//  2. Write out the __TEXT and __DATA segments, including the data from the
//     ExtraStringRepository.
//  3. Inspect the DYLD_INFO, SYMTAB and DYSYMTAB commands to collect the
//     relevant parts of global __LINKEDIT segment and copy them to the output
//     file.
//  4. Revisit the output file to fix the file offsets. All file offsets were
//     originally pointing to locations in the cache file, but in the decached
//     file these will be no longer meaningful if not fixed.
//  5. Append the extra 'section' header to the corresponding segments, if there
//     are external Objective-C selectors or methods.
//  6. Go through the Objective-C sections and rewire the external references.
class DecachingFile : public MachOFile {
    struct FileoffFixup {
        uint32_t sourceBegin;
        uint32_t sourceEnd;
        int32_t negDelta;
    };

    struct ObjcExtraString {
        const char* string;
        size_t entry_size;
        uint32_t new_address;
        off_t override_offset;
    };

    struct {
        long rebase_off, bind_off, weak_bind_off,
               lazy_bind_off, export_off,            // dyld_info
             symoff, stroff,                         // symtab
             tocoff, modtaboff, extrefsymoff,
               indirectsymoff, extreloff, locreloff, // dysymtab
             dataoff,    // linkedit_data_command (dummy)
               dataoff_cs, dataoff_ssi, dataoff_fs;
        long bind_size;
        int32_t strsize;
    } _new_linkedit_offsets;

private:
    uint32_t _linkedit_offset, _linkedit_size;
    uint32_t _imageinfo_address, _imageinfo_replacement;

    FILE* _f;
    std::vector<FileoffFixup> _fixups;
    std::vector<segment_command> _new_segments;
    ExtraStringRepository _extra_text, _extra_data;
    std::vector<uint32_t> _nullify_patches;
    ExtraBindRepository _extra_bind;

private:
    void open_file(const boost::filesystem::path& filename) {
        boost::filesystem::create_directories(filename.parent_path());
        _f = fopen(filename.c_str(), "wb");
        if (!_f) {
            perror("Error");
            fprintf(stderr, "Error: Cannot write to '%s'.\n", filename.c_str());
        }
    }

    void write_extrastr(const char* string, size_t size, uint32_t, const std::vector<uint32_t>&) const {
        fwrite(string, size, 1, _f);
    }

    void write_segment_content(const segment_command* cmd);

    ExtraStringRepository* repo_for_segname(const char* segname) {
        if (!strcmp(segname, "__DATA"))
            return &_extra_data;
        else if (!strcmp(segname, "__TEXT"))
            return &_extra_text;
        return NULL;
    }

    template<typename T>
    void fix_offset(T& fileoff) const {
        if (fileoff == 0)
            return;

        BOOST_REVERSE_FOREACH(const FileoffFixup& fixup, _fixups) {
            if (fixup.sourceBegin <= fileoff && fileoff < fixup.sourceEnd) {
                fileoff -= fixup.negDelta;
                return;
            }
        }
    }

    void write_real_linkedit(const load_command* cmd);

    void fix_file_offsets(const load_command* cmd) {
        switch (cmd->cmd) {
            default:
                fwrite(cmd, cmd->cmdsize, 1, _f);
                break;

            case LC_SEGMENT: {
                segment_command segcmd = *static_cast<const segment_command*>(cmd);
                if (streq(segcmd.segname, "__LINKEDIT")) {
                    segcmd.vmsize = _linkedit_size;
                    segcmd.fileoff = _linkedit_offset;
                    segcmd.filesize = _linkedit_size;
                    fwrite(&segcmd, sizeof(segcmd), 1, _f);
                } else {
                    const ExtraStringRepository* extra_repo = this->repo_for_segname(segcmd.segname);
                    bool has_extra_sect = extra_repo && extra_repo->has_content();

                    this->fix_offset(segcmd.fileoff);
                    section* sects = new section[segcmd.nsects + has_extra_sect];
                    memcpy(sects, 1 + static_cast<const segment_command*>(cmd), segcmd.nsects * sizeof(*sects));
                    for (uint32_t i = 0; i < segcmd.nsects; ++ i) {
                        this->fix_offset(sects[i].offset);
                        this->fix_offset(sects[i].reloff);
                    }
                    if (has_extra_sect) {
                        uint32_t extra_sect_size = extra_repo->total_size();
                        sects[segcmd.nsects] = extra_repo->section_template();
                        segcmd.cmdsize += sizeof(*sects);
                        segcmd.vmsize += extra_sect_size;
                        segcmd.filesize += extra_sect_size;
                        segcmd.nsects += 1;
                    }
                    fwrite(&segcmd, sizeof(segcmd), 1, _f);
                    fwrite(sects, sizeof(*sects), segcmd.nsects, _f);
                    delete[] sects;
                }
                _new_segments.push_back(segcmd);
                break;
            }

            case LC_SYMTAB: {
                symtab_command symcmd = *static_cast<const symtab_command*>(cmd);
                symcmd.symoff = _new_linkedit_offsets.symoff;
                symcmd.stroff = _new_linkedit_offsets.stroff;
                symcmd.strsize = _new_linkedit_offsets.strsize;
                fwrite(&symcmd, sizeof(symcmd), 1, _f);
                break;
            }

            case LC_DYSYMTAB: {
                dysymtab_command dycmd = *static_cast<const dysymtab_command*>(cmd);
                dycmd.tocoff = _new_linkedit_offsets.tocoff;
                dycmd.modtaboff = _new_linkedit_offsets.modtaboff;
                dycmd.extrefsymoff = _new_linkedit_offsets.extrefsymoff;
                dycmd.indirectsymoff = _new_linkedit_offsets.indirectsymoff;
                dycmd.extreloff = _new_linkedit_offsets.extreloff;
                dycmd.locreloff = _new_linkedit_offsets.locreloff;
                fwrite(&dycmd, sizeof(dycmd), 1, _f);
                break;
            }

            case LC_TWOLEVEL_HINTS: {
                twolevel_hints_command tlcmd = *static_cast<const twolevel_hints_command*>(cmd);
                this->fix_offset(tlcmd.offset);
                fwrite(&tlcmd, sizeof(tlcmd), 1, _f);
                break;
            }

            /*
            case LC_SEGMENT_64: {
                segment_command_64 segcmd = *static_cast<const segment_command_64*>(cmd);
                this->fix_offset(segcmd.fileoff);
                fwrite(&segcmd, sizeof(segcmd), 1, _f);
                section_64* sects = new section_64[segcmd.nsects];
                memcpy(sects, 1 + static_cast<const segment_command_64*>(cmd), segcmd.nsects * sizeof(*sects));
                for (uint32_t i = 0; i < segcmd.nsects; ++ i) {
                    this->fix_offset(sects[i].offset);
                    this->fix_offset(sects[i].reloff);
                }
                fwrite(sects, sizeof(*sects), segcmd.nsects, _f);
                delete[] sects;
                break;
            }
            */

            case LC_CODE_SIGNATURE:
            case LC_SEGMENT_SPLIT_INFO: 
            case LC_FUNCTION_STARTS: {
                linkedit_data_command ldcmd = *static_cast<const linkedit_data_command*>(cmd);
                if (ldcmd.cmd == LC_CODE_SIGNATURE)
                    ldcmd.dataoff = _new_linkedit_offsets.dataoff_cs;
                else if (ldcmd.cmd == LC_SEGMENT_SPLIT_INFO)
                    ldcmd.dataoff = _new_linkedit_offsets.dataoff_ssi;
                else if (ldcmd.cmd == LC_FUNCTION_STARTS)
                    ldcmd.dataoff = _new_linkedit_offsets.dataoff_fs;
                fwrite(&ldcmd, sizeof(ldcmd), 1, _f);
                break;
            }

            case LC_ENCRYPTION_INFO: {
                encryption_info_command eicmd = *static_cast<const encryption_info_command*>(cmd);
                this->fix_offset(eicmd.cryptoff);
                fwrite(&eicmd, sizeof(eicmd), 1, _f);
                break;
            }

            case LC_DYLD_INFO:
            case LC_DYLD_INFO_ONLY: {
                dyld_info_command dicmd = *static_cast<const dyld_info_command*>(cmd);
                dicmd.rebase_off = _new_linkedit_offsets.rebase_off;
                dicmd.bind_off = _new_linkedit_offsets.bind_off;
                dicmd.weak_bind_off = _new_linkedit_offsets.weak_bind_off;
                dicmd.lazy_bind_off = _new_linkedit_offsets.lazy_bind_off;
                dicmd.export_off = _new_linkedit_offsets.export_off;
                dicmd.bind_size = _new_linkedit_offsets.bind_size;
                fwrite(&dicmd, sizeof(dicmd), 1, _f);
                break;
            }
        }
    }

    // Convert VM address to file offset of the decached file _after_ inserting
    //  the extra sections.
    long from_new_vmaddr(uint32_t vmaddr) const {
        std::vector<segment_command>::const_iterator nit;
        std::vector<const segment_command*>::const_iterator oit;
        
        std::vector<segment_command>::const_iterator end = _new_segments.end(); 
        for (nit = _new_segments.begin(), oit = _segments.begin(); nit != end; ++ nit, ++ oit) {
            if (nit->vmaddr <= vmaddr && vmaddr < nit->vmaddr + nit->vmsize) {
                uint32_t retval = vmaddr - nit->vmaddr + nit->fileoff;
                // This mess is added to solve the __DATA,__bss section issue.
                // This section is zero-filled, causing the segment's vmsize
                //  larger than the filesize. Since the __extradat section is
                //  placed after the __bss section, using just the formula above
                //  will cause the imaginary size comes from that section to be
                //  included as well. The "-=" below attempts to fix it.
                if (vmaddr >= (*oit)->vmaddr + (*oit)->vmsize)
                    retval -= (*oit)->vmsize - (*oit)->filesize;
                return retval;
            }
        }
        
        return -1;
    }
    
    // Get the segment number and offset from that segment given a VM address.
    std::pair<int, uint32_t> segnum_and_offset(uint32_t vmaddr) const {
        int i = 0;
        BOOST_FOREACH(const segment_command* segcmd, _segments) {
            if (segcmd->vmaddr <= vmaddr && vmaddr < segcmd->vmaddr + segcmd->vmsize)
                return std::make_pair(i, vmaddr - segcmd->vmaddr);
            ++ i;
        }
        return std::make_pair(-1, ~0u);
    }

    template <typename T>
    void prepare_patch_objc_list(uint32_t list_vmaddr, uint32_t override_vmaddr);
    void prepare_objc_extrastr(const segment_command* segcmd);

    void get_address_info(uint32_t vmaddr, std::string* p_name, int* p_libord) const;
    void add_extlink_to(uint32_t vmaddr, uint32_t override_vmaddr);

    void patch_objc_sects_callback(const char*, size_t, uint32_t new_address, const std::vector<uint32_t>& override_addresses) const {
        BOOST_FOREACH(uint32_t vmaddr, override_addresses) {
            long actual_offset = this->from_new_vmaddr(vmaddr);
            fseek(_f, actual_offset, SEEK_SET);
            fwrite(&new_address, 4, 1, _f);
        }
    }

    void patch_objc_sects() const {
        _extra_text.foreach_entry(this, &DecachingFile::patch_objc_sects_callback);
        _extra_data.foreach_entry(this, &DecachingFile::patch_objc_sects_callback);

        this->patch_objc_sects_callback(NULL, 0, 0, _nullify_patches);

        if (_imageinfo_address) {
            long actual_offset = this->from_new_vmaddr(_imageinfo_address);
            fseek(_f, actual_offset, SEEK_SET);
            fwrite(&_imageinfo_replacement, 4, 1, _f);
        }
    }

public:
    DecachingFile(const boost::filesystem::path& filename, const mach_header* header, const ProgramContext* context) :
        MachOFile(header, context), _imageinfo_address(0),
        _extra_text("__TEXT", "__objc_extratxt", 2, 0),
        _extra_data("__DATA", "__objc_extradat", 0, 2)
    {
        if (header->magic != 0xfeedface) {
            fprintf(stderr,
                "Error: Cannot dump '%s'. Only 32-bit little-endian single-file\n"
                "       Mach-O objects are supported.\n", filename.c_str());
            return;
        }
        memset(&_new_linkedit_offsets, 0, sizeof(_new_linkedit_offsets));

        this->open_file(filename);
        if (!_f)
            return;

        // phase 1
        BOOST_FOREACH(const segment_command* segcmd, _segments) {
            ExtraStringRepository* repo = this->repo_for_segname(segcmd->segname);
            if (repo)
                repo->set_section_vmaddr(segcmd->vmaddr + segcmd->vmsize);
        }
        BOOST_FOREACH(const segment_command* segcmd, _segments)
            this->prepare_objc_extrastr(segcmd);

        // phase 2
        BOOST_FOREACH(const segment_command* segcmd, _segments)
            this->write_segment_content(segcmd);

        // phase 3
        _linkedit_offset = static_cast<uint32_t>(ftell(_f));
        this->foreach_command(&DecachingFile::write_real_linkedit);
        _linkedit_size = static_cast<uint32_t>(ftell(_f)) - _linkedit_offset;

        // phase 4 & 5
        fseek(_f, offsetof(mach_header, sizeofcmds), SEEK_SET);
        uint32_t new_sizeofcmds = _header->sizeofcmds + (_extra_text.has_content() + _extra_data.has_content()) * sizeof(section);
        fwrite(&new_sizeofcmds, sizeof(new_sizeofcmds), 1, _f);
        fseek(_f, sizeof(*header), SEEK_SET);
        this->foreach_command(&DecachingFile::fix_file_offsets);

        // phase 6
        this->patch_objc_sects();
    }

    ~DecachingFile() {
        if (_f)
            fclose(_f);
    }

    bool is_open() const { return _f != NULL; }

};

class ProgramContext {
    const char* _folder;
    char* _filename;
    DataFile* _f;
    bool _printmode;
    std::vector<boost::filesystem::path> _namefilters;
    boost::unordered_map<const mach_header*, boost::filesystem::path> _already_dumped;

    const dyld_cache_header* _header;
    const shared_file_mapping_np* _mapping;
    const dyld_cache_image_info* _images;
    std::vector<MachOFile> _macho_files;

public:
    ProgramContext() :
        _folder("libraries"),
        _filename(NULL),
        _f(NULL),
        _printmode(false)
    {}

private:
    void print_usage(char* path) const {
        const char* progname = path ? strrchr(path, '/')+1 : "dyld_decache";
        printf(
            "dyld_decache v0.1c\n"
            "Usage:\n"
            "  %s [-p] [-o folder] [-f name [-f name] ...] path/to/dyld_shared_cache_armvX\n"
            "\n"
            "Options:\n"
            "  -o folder : Extract files into 'folder'. Default to './libraries'\n"
            "  -p        : Print the content of the cache file and exit.\n"
            "  -f name   : Only extract the file with filename 'name', e.g. '-f UIKit' or\n"
            "              '-f liblockdown'. This option may be specified multiple times to\n"
            "              extract more than one file. If not specified, all files will be\n"
            "              extracted.\n"
        , progname);
    }

    void parse_options(int argc, char* argv[]) {
        int opt;

        while ((opt = getopt(argc, argv, "o:plf:")) != -1) {
            switch (opt) {
                case 'o':
                    _folder = optarg;
                    break;
                case 'p':
                    _printmode = true;
                    break;
                case 'f':
                    _namefilters.push_back(remove_all_extensions(optarg));
                    break;
                case '?':
                case -1:
                    break;
                default:
                    printf ("Unknown option '%c'\n", opt);
                    return;
            }
        }

        if (optind < argc)
            _filename = argv[optind];
    }

    bool check_magic() const {
        return !strncmp(_header->magic, "dyld_v1", 7);
    }

    const mach_header* mach_header_of_image(int i) const {
        return _macho_files[i].header();
    }

    off_t from_vmaddr(uint64_t vmaddr) const {
        for (uint32_t i = 0; i < _header->mappingCount; ++ i) {
            if (_mapping[i].sfm_address <= vmaddr && vmaddr < _mapping[i].sfm_address + _mapping[i].sfm_size)
                return vmaddr - _mapping[i].sfm_address + _mapping[i].sfm_file_offset;
        }
        return -1;
    }

    const char* peek_char_at_vmaddr(uint64_t vmaddr) const {
        off_t offset = this->from_vmaddr(vmaddr);
        if (offset >= 0) {
            return _f->peek_data_at<char>(offset);
        } else {
            return NULL;
        }
    }
    
    void process_export_trie_node(off_t start, off_t cur, off_t end, const std::string& prefix, uint32_t bias, boost::unordered_map<uint32_t, std::string>& exports) const {
    	if (cur < end) {
    		_f->seek(cur);
    		unsigned char term_size = static_cast<unsigned char>(_f->read_char());
    		if (term_size != 0) {
    			/*unsigned flags =*/ _f->read_uleb128<unsigned>();
    			unsigned addr = _f->read_uleb128<unsigned>() + bias;
    			exports.insert(std::make_pair(addr, prefix));
    		}
    		_f->seek(cur + term_size + 1);
    		unsigned char child_count = static_cast<unsigned char>(_f->read_char());
    		off_t last_pos;
    		for (unsigned char i = 0; i < child_count; ++ i) {
    			const char* suffix = _f->read_string();
    			unsigned offset = _f->read_uleb128<unsigned>();
    			last_pos = _f->tell();
    			this->process_export_trie_node(start, start + offset, end, prefix + suffix, bias, exports);
    			_f->seek(last_pos);
    		}
    	}
    }
    
public:
    void fill_export(off_t start, off_t end, uint32_t bias, boost::unordered_map<uint32_t, std::string>& exports) const {
        process_export_trie_node(start, start, end, "", bias, exports);
    }

    bool initialize(int argc, char* argv[]) {
        this->parse_options(argc, argv);
        if (_filename == NULL) {
            this->print_usage(argv[0]);
            return false;
        }
        return true;
    }

    void close() {
        if (_f) {
            delete _f;
            _f = NULL;
        }
    }

    bool open() {
        _f = new DataFile(_filename);

        _header = _f->peek_data_at<dyld_cache_header>(0);
        if (!this->check_magic()) {
            close();
            return false;
        }

        _mapping = _f->peek_data_at<shared_file_mapping_np>(_header->mappingOffset);
        _images = _f->peek_data_at<dyld_cache_image_info>(_header->imagesOffset);
        return true;
    }
    
    uint32_t image_containing_address(uint32_t vmaddr, std::string* symname = NULL) const {
        uint32_t i = 0;
        BOOST_FOREACH(const MachOFile& mo, _macho_files) {
            if (mo.contains_address(vmaddr)) {
                if (symname)
                    *symname = mo.exported_symbol(vmaddr);
                return i;
            }
            ++ i;
        }
        return ~0u;
    }
    
    bool is_print_mode() const { return _printmode; }

    const char* path_of_image(uint32_t i) const {
        return _f->peek_data_at<char>(_images[i].pathFileOffset);
    }

    bool should_skip_image(uint32_t i) const {
        const char* path = this->path_of_image(i);
        if (_namefilters.empty())
            return false;
            
        boost::filesystem::path stem = remove_all_extensions(path);
        BOOST_FOREACH(const boost::filesystem::path& filt, _namefilters) {
            if (stem == filt)
                return false;
        }

        return true;
    }

    // Decache the file of the specified index. If the file is already decached
    //  under a different name, create a symbolic link to it.
    void save_complete_image(uint32_t image_index) {
        boost::filesystem::path filename (_folder);
        const char* path = this->path_of_image(image_index);
        filename /= path;

        const mach_header* header = this->mach_header_of_image(image_index);
        boost::unordered_map<const mach_header*, boost::filesystem::path>::const_iterator cit = _already_dumped.find(header);

        bool already_dumped = (cit != _already_dumped.end());
        printf("%3d/%d: %sing '%s'...\n", image_index, _header->imagesCount, already_dumped ? "Link" : "Dump", path);

        if (already_dumped) {
            boost::system::error_code ec;
            boost::filesystem::path src_path (path);
            boost::filesystem::path target_path (".");
            boost::filesystem::path::iterator it = src_path.begin();
            ++ it;
            ++ it;
            for (; it != src_path.end(); ++ it) {
                target_path /= "..";
            }
            target_path /= cit->second;

            boost::filesystem::remove(filename);
            boost::filesystem::create_directories(filename.parent_path());
            boost::filesystem::create_symlink(target_path, filename, ec);
            if (ec)
                fprintf(stderr, "**** Failed: %s\n", ec.message().c_str());

        } else {
            _already_dumped.insert(std::make_pair(header, path));
            DecachingFile df (filename, header, this);
            if (!df.is_open())
                perror("**** Failed");
        }
    }

    void save_all_images() {
        _macho_files.clear();
        for (uint32_t i = 0; i < _header->imagesCount; ++ i) {
            const mach_header* mh = _f->peek_data_at<mach_header>(this->from_vmaddr(_images[i].address));
            _macho_files.push_back(MachOFile(mh, this, _images[i].address));
        }
        
        for (uint32_t i = 0; i < _header->imagesCount; ++ i) {
            if (!this->should_skip_image(i)) {
                this->save_complete_image(i);
            }
        }
    }
    
    void print_info() const {
        printf(
            "magic = \"%-.16s\", dyldBaseAddress = 0x%llx\n"
            "\n"
            "Mappings (%d):\n"
            "  ---------address  ------------size  ----------offset  prot\n"
        , _header->magic, _header->dyldBaseAddress, _header->mappingCount);

        for (uint32_t i = 0; i < _header->mappingCount; ++ i) {
            printf("  %16llx  %16llx  %16llx  %x (<= %x)\n",
                _mapping[i].sfm_address, _mapping[i].sfm_size, _mapping[i].sfm_file_offset,
                _mapping[i].sfm_init_prot, _mapping[i].sfm_max_prot
            );
        }

        printf(
            "\n"
            "Images (%d):\n"
            "  ---------address  filename\n"
        , _header->imagesCount);

        for (uint32_t i = 0; i < _header->imagesCount; ++ i) {
            printf("  %16llx  %s\n", _images[i].address, this->path_of_image(i));
        }
    }

    ~ProgramContext() { close(); }

    friend class DecachingFile;
};


void MachOFile::retrieve_segments_and_libords(const load_command* cmd) {
    switch (cmd->cmd) {
        default:
            break;
        case LC_SEGMENT: {
            const segment_command* segcmd = static_cast<const segment_command*>(cmd);
            _segments.push_back(segcmd);
            break;
        }
        case LC_LOAD_DYLIB:
        case LC_ID_DYLIB:
        case LC_LOAD_WEAK_DYLIB:
        case LC_REEXPORT_DYLIB:
        case LC_LAZY_LOAD_DYLIB:
        case LC_LOAD_UPWARD_DYLIB: {
            const dylib_command* dlcmd = static_cast<const dylib_command*>(cmd);
            std::string dlname (dlcmd->dylib.name + reinterpret_cast<const char*>(dlcmd));
            _libords.insert(std::make_pair(dlname, _cur_libord));
            ++ _cur_libord;
            break;
        }
        
        case LC_DYLD_INFO:
        case LC_DYLD_INFO_ONLY: {
            if (_image_vmaddr) {
                const dyld_info_command* dicmd = static_cast<const dyld_info_command*>(cmd);
                if (dicmd->export_off)
                    _context->fill_export(dicmd->export_off, dicmd->export_off + dicmd->export_size, _image_vmaddr, _exports);
            }
            break;
        }
    }
}

void DecachingFile::write_segment_content(const segment_command* segcmd) {
    if (!streq(segcmd->segname, "__LINKEDIT")) {
        ExtraStringRepository* repo = this->repo_for_segname(segcmd->segname);

        const char* data_ptr = _context->peek_char_at_vmaddr(segcmd->vmaddr);
        long new_fileoff = ftell(_f);

        fwrite(data_ptr, 1, segcmd->filesize, _f);
        uint32_t filesize = segcmd->filesize;

        if (repo && repo->has_content()) {
            repo->foreach_entry(this, &DecachingFile::write_extrastr);

            // make sure the section is aligned on 8-byte boundary...
            long extra = ftell(_f) % 8;
            if (extra) {
                char padding[8] = {0};
                fwrite(padding, 1, 8-extra, _f);
                repo->increase_size_by(8-extra);
            }
            repo->set_section_fileoff(new_fileoff + filesize);
            filesize += repo->total_size();
        }

        FileoffFixup fixup = {segcmd->fileoff, segcmd->fileoff + filesize, segcmd->fileoff - new_fileoff};
        _fixups.push_back(fixup);
    }
}

void DecachingFile::write_real_linkedit(const load_command* cmd) {
    const unsigned char* data_ptr = _context->_f->data();

    // Write all data in [offmem .. offmem+countmem*objsize] to the output file,
    //  and pad to make sure the beginning is aligned with 'objsize' boundary.
    #define TRY_WRITE(offmem, countmem, objsize) \
        if (cmdvar->offmem && cmdvar->countmem) { \
            long curloc = ftell(_f); \
            long extra = curloc % objsize; \
            if (extra != 0) { \
                char padding[objsize] = {0}; \
                fwrite(padding, 1, objsize-extra, _f); \
                curloc += objsize-extra; \
            } \
            _new_linkedit_offsets.offmem = curloc; \
            fwrite(cmdvar->offmem + data_ptr, cmdvar->countmem * objsize, 1, _f); \
        }

    switch (cmd->cmd) {
        default:
            break;

        case LC_DYLD_INFO:
        case LC_DYLD_INFO_ONLY: {
            const dyld_info_command* cmdvar = static_cast<const dyld_info_command*>(cmd);
            TRY_WRITE(rebase_off, rebase_size, 1);
            long curloc = ftell(_f);
            long extra_size = _extra_bind.optimize_and_write(_f);
            TRY_WRITE(bind_off, bind_size, 1);
            _new_linkedit_offsets.bind_off = curloc;
            _new_linkedit_offsets.bind_size += extra_size;
            TRY_WRITE(weak_bind_off, weak_bind_size, 1);
            TRY_WRITE(lazy_bind_off, lazy_bind_size, 1);
            TRY_WRITE(export_off, export_size, 1);
            break;
        }

        case LC_SYMTAB: {
            // The string table is shared by all library, so naively using
            //  TRY_WRITE will create a huge file with lots of unnecessary
            //  strings. Therefore, we have to scan through all symbols and only
            //  take those strings which are used by the symbol.
            const symtab_command* cmdvar = static_cast<const symtab_command*>(cmd);
            if (cmdvar->symoff && cmdvar->nsyms) {
                _new_linkedit_offsets.stroff = ftell(_f);

                nlist* syms = new nlist[cmdvar->nsyms];
                memcpy(syms, _context->_f->peek_data_at<nlist>(cmdvar->symoff), sizeof(*syms) * cmdvar->nsyms);

                int32_t cur_strx = 0;
                for (uint32_t i = 0; i < cmdvar->nsyms; ++ i) {
                    const char* the_string = _context->_f->peek_data_at<char>(syms[i].n_strx + cmdvar->stroff);
                    size_t entry_len = strlen(the_string) + 1;
                    fwrite(the_string, entry_len, 1, _f);
                    syms[i].n_strx = cur_strx;
                    cur_strx += entry_len;
                }
                _new_linkedit_offsets.strsize = cur_strx;

                long curloc = ftell(_f);
                long extra = curloc % sizeof(nlist);
                if (extra != 0) {
                    char padding[sizeof(nlist)] = {0};
                    fwrite(padding, 1, sizeof(nlist)-extra, _f);
                    curloc += sizeof(nlist)-extra;
                }
                _new_linkedit_offsets.symoff = curloc;
                fwrite(syms, cmdvar->nsyms, sizeof(nlist), _f);

                delete[] syms;
            }

            break;
        }

        case LC_DYSYMTAB: {
            const dysymtab_command* cmdvar = static_cast<const dysymtab_command*>(cmd);
            TRY_WRITE(tocoff, ntoc, 8);
            TRY_WRITE(modtaboff, nmodtab, 52);
            TRY_WRITE(extrefsymoff, nextrefsyms, 4);
            TRY_WRITE(indirectsymoff, nindirectsyms, 4);
            TRY_WRITE(extreloff, nextrel, 8);
            TRY_WRITE(locreloff, nlocrel, 8);
            break;
        }

        case LC_CODE_SIGNATURE:
        case LC_SEGMENT_SPLIT_INFO:
        case LC_FUNCTION_STARTS: {
            const linkedit_data_command* cmdvar = static_cast<const linkedit_data_command*>(cmd);
            TRY_WRITE(dataoff, datasize, 1);
            if (cmd->cmd == LC_CODE_SIGNATURE)
                _new_linkedit_offsets.dataoff_cs = _new_linkedit_offsets.dataoff;
            else if (cmd->cmd == LC_SEGMENT_SPLIT_INFO)
                _new_linkedit_offsets.dataoff_ssi = _new_linkedit_offsets.dataoff;
            else if (cmd->cmd == LC_FUNCTION_STARTS)
                _new_linkedit_offsets.dataoff_fs = _new_linkedit_offsets.dataoff;
            break;
        }
    }

    #undef TRY_WRITE
}

void DecachingFile::get_address_info(uint32_t vmaddr, std::string* p_name, int* p_libord) const {
    uint32_t which_image = _context->image_containing_address(vmaddr, p_name);
    const char* image_name = _context->path_of_image(which_image);
    *p_libord = this->libord_with_name(image_name);
}

void DecachingFile::add_extlink_to(uint32_t vmaddr, uint32_t override_vmaddr) {
    if (!vmaddr)
        return;
    if (this->contains_address(vmaddr))
        return;
    _extra_bind.insert(vmaddr, this->segnum_and_offset(override_vmaddr), this, &DecachingFile::get_address_info);
    // get class-dump-z to search for symbols instead of using this invalid
    //  address directly.
    _nullify_patches.push_back(override_vmaddr);
}

template <typename T>
void DecachingFile::prepare_patch_objc_list(uint32_t list_vmaddr, uint32_t override_vmaddr) {
    if (!list_vmaddr)
        return;

    off_t offset = _context->from_vmaddr(list_vmaddr);
    _context->_f->seek(offset);
    uint32_t entsize = _context->_f->copy_data<uint32_t>() & ~(uint32_t)3;
    uint32_t count = _context->_f->copy_data<uint32_t>();

    if (entsize != sizeof(T))
        throw TRException("DecachingFile::prepare_patch_objc_list():\n\tWrong entsize: %u instead of %lu\n", entsize, sizeof(T));

    if (!this->contains_address(list_vmaddr)) {
        list_vmaddr = _extra_data.next_vmaddr();
        size_t size = 8 + sizeof(T)*count;
        _extra_data.insert(_context->_f->peek_data_at<char>(offset), size, override_vmaddr);
    }

    const T* objects = _context->_f->peek_data<T>();
    for (uint32_t j = 0; j < count; ++ j) {
        if (!this->contains_address(objects[j].name)) {
            const char* the_string = _context->peek_char_at_vmaddr(objects[j].name);
            _extra_text.insert(the_string, list_vmaddr + 8 + sizeof(T)*j);
        }
    }
}

void DecachingFile::prepare_objc_extrastr(const segment_command* segcmd) {
    if (streq(segcmd->segname, "__DATA")) {
        const section* sects = reinterpret_cast<const section*>(1 + segcmd);
        for (uint32_t i = 0; i < segcmd->nsects; ++ i) {
            const section& sect = sects[i];
            if (streq(sect.sectname, "__objc_selrefs")) {
                const uint32_t* refs = _context->_f->peek_data_at<uint32_t>(sect.offset);
                for (uint32_t j = 0; j < sect.size/4; ++ j) {
                    if (!this->contains_address(refs[j])) {
                        const char* the_string = _context->peek_char_at_vmaddr(refs[j]);
                        _extra_text.insert(the_string, sect.addr + 4*j);
                    }
                }
            } else if (streq(sect.sectname, "__objc_classlist")) {
                const uint32_t* classes = _context->_f->peek_data_at<uint32_t>(sect.offset);
                for (uint32_t j = 0; j < sect.size/4; ++ j) {
                    uint32_t class_vmaddr = classes[j];
                    const class_t* class_obj = reinterpret_cast<const class_t*>(_context->peek_char_at_vmaddr(class_vmaddr));
                    this->add_extlink_to(class_obj->superclass, class_vmaddr + offsetof(class_t, superclass));
                    const class_ro_t* class_data = reinterpret_cast<const class_ro_t*>(_context->peek_char_at_vmaddr(class_obj->data));
                    this->prepare_patch_objc_list<method_t>(class_data->baseMethods, class_obj->data + offsetof(class_ro_t, baseMethods));
                    this->prepare_patch_objc_list<property_t>(class_data->baseProperties, class_obj->data + offsetof(class_ro_t, baseProperties));
                    
                    const class_t* metaclass_obj = reinterpret_cast<const class_t*>(_context->peek_char_at_vmaddr(class_obj->isa));
                    this->add_extlink_to(metaclass_obj->isa, class_obj->isa + offsetof(class_t, isa));
                    this->add_extlink_to(metaclass_obj->superclass, class_obj->isa + offsetof(class_t, superclass));
                    const class_ro_t* metaclass_data = reinterpret_cast<const class_ro_t*>(_context->peek_char_at_vmaddr(metaclass_obj->data));
                    this->prepare_patch_objc_list<method_t>(metaclass_data->baseMethods, metaclass_obj->data + offsetof(class_ro_t, baseMethods));
                    this->prepare_patch_objc_list<property_t>(metaclass_data->baseProperties, metaclass_obj->data + offsetof(class_ro_t, baseProperties));
                }
            } else if (streq(sect.sectname, "__objc_protolist")) {
                const uint32_t* protos = _context->_f->peek_data_at<uint32_t>(sect.offset);
                for (uint32_t j = 0; j < sect.size/4; ++ j) {
                    uint32_t proto_vmaddr = protos[j];
                    const protocol_t* proto_obj = reinterpret_cast<const protocol_t*>(_context->peek_char_at_vmaddr(proto_vmaddr));
                    this->prepare_patch_objc_list<method_t>(proto_obj->instanceMethods, proto_vmaddr + offsetof(protocol_t, instanceMethods));
                    this->prepare_patch_objc_list<method_t>(proto_obj->classMethods, proto_vmaddr + offsetof(protocol_t, classMethods));
                    this->prepare_patch_objc_list<method_t>(proto_obj->optionalInstanceMethods, proto_vmaddr + offsetof(protocol_t, optionalInstanceMethods));
                    this->prepare_patch_objc_list<method_t>(proto_obj->optionalClassMethods, proto_vmaddr + offsetof(protocol_t, optionalClassMethods));
                }
            } else if (streq(sect.sectname, "__objc_catlist")) {
                const uint32_t* cats = _context->_f->peek_data_at<uint32_t>(sect.offset);
                for (uint32_t j = 0; j < sect.size/4; ++ j) {
                    uint32_t cat_vmaddr = cats[j];
                    const category_t* cat_obj = reinterpret_cast<const category_t*>(_context->peek_char_at_vmaddr(cat_vmaddr));
                    this->add_extlink_to(cat_obj->cls, cat_vmaddr + offsetof(category_t, cls));
                    this->prepare_patch_objc_list<method_t>(cat_obj->instanceMethods, cat_vmaddr + offsetof(category_t, instanceMethods));
                    this->prepare_patch_objc_list<method_t>(cat_obj->classMethods, cat_vmaddr + offsetof(category_t, classMethods));
                }
            } else if (streq(sect.sectname, "__objc_imageinfo")) {
                _imageinfo_address = sect.addr + 4;
                uint32_t original_flag = *reinterpret_cast<const uint32_t*>(_context->peek_char_at_vmaddr(_imageinfo_address));
                _imageinfo_replacement = original_flag & ~8;    // clear the OBJC_IMAGE_OPTIMIZED_BY_DYLD flag. (this chokes class-dump-3.3.3.)
            } else if (streq(sect.sectname, "__objc_classrefs")) {
                const uint32_t* refs = _context->_f->peek_data_at<uint32_t>(sect.offset);
                uint32_t addr = sect.addr;
                for (uint32_t j = 0; j < sect.size/4; ++ j, ++ refs, addr += 4) {
                    this->add_extlink_to(*refs, addr);
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    ProgramContext ctx;
    if (ctx.initialize(argc, argv)) {
        if (ctx.open()) {
            if (ctx.is_print_mode()) {
                ctx.print_info();
            } else {
                ctx.save_all_images();
            }
        }
    }

    return 0;
}
