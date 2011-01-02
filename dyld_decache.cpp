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

/*
#define	S_NON_LAZY_SYMBOL_POINTERS	0x6
#define	S_LAZY_SYMBOL_POINTERS		0x7
#define	S_LAZY_DYLIB_SYMBOL_POINTERS	0x10
*/

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

//------------------------------------------------------------------------------
// END THIRD-PARTY STRUCTURES
//------------------------------------------------------------------------------

static FILE* fopen_and_create_missing_dirs(const std::string& filename) {
    boost::filesystem::path path (filename);
    boost::filesystem::create_directories(path.parent_path());
    return fopen(filename.c_str(), "wb");
}

static void putchar_and_flush(char c) throw() {
    putchar(c);
    fflush(stdout);
}

static bool streq(const char x[16], const char* y) {
    return strncmp(x, y, 16) == 0;
} 

class ProgramContext;


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

        
    template <typename Object>
    void foreach_entry(const Object* self, void (Object::*action)(const char* string, size_t size, uint32_t new_address, const std::vector<uint32_t>& override_addresses) const) const {
        BOOST_FOREACH(const Entry& e, _entries) {
            (self->*action)(e.string, e.size, e.new_address, e.override_addresses);
        }
    }
    
    void increase_size_by(size_t delta) { _template.size += delta; }
    size_t total_size() const { return _template.size; }
    bool has_content() const { return _template.size != 0; }
    
    section section_template() const { return _template; }
    
    void set_section_vmaddr(uint32_t vmaddr) { _template.addr = vmaddr; }
    void set_section_fileoff(uint32_t fileoff) { _template.offset = fileoff; }
    uint32_t next_vmaddr() const { return _template.addr + _template.size; }
};


class DecachingFile {
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
               indirectsymoff, extreloff, locreloff; // dysymtab
        int32_t strsize;
    } _new_linkedit_offsets;
    
private:
    
    uint32_t _linkedit_offset, _linkedit_size;
    
    FILE* _f;
    const mach_header* _header;
    std::vector<FileoffFixup> _fixups;
    const ProgramContext* _context;
    std::vector<const segment_command*> _segments;
    std::vector<segment_command> _new_segments;
    ExtraStringRepository _extra_text, _extra_data;
    std::vector<uint32_t> _entsize12_patches;
    
private:
    
    void open_file(const std::string& filename) {
        _f = fopen_and_create_missing_dirs(filename);
        if (!_f) {
            perror("Error");
            fprintf(stderr, "Error: Cannot write to '%s'.\n", filename.c_str());
        }
    }
    
    void write_extrastr(const char* string, size_t size, uint32_t, const std::vector<uint32_t>&) const {
        fwrite(string, size, 1, _f);
    }
    
    void write_segment_content(const segment_command* cmd);
        
    void foreach_command(void(DecachingFile::*action)(const load_command* cmd)) {
        const unsigned char* cur_cmd = reinterpret_cast<const unsigned char*>(_header + 1);
        
        for (uint32_t i = 0; i < _header->ncmds; ++ i) {
            const load_command* cmd = reinterpret_cast<const load_command*>(cur_cmd);
            cur_cmd += cmd->cmdsize;
            
            (this->*action)(cmd);
        }
    }
    
    ExtraStringRepository* repo_for_segname(const char* segname) {
        if (!strcmp(segname, "__DATA"))
            return &_extra_data;
        else if (!strcmp(segname, "__TEXT"))
            return &_extra_text;
        return NULL;
    }
    
    const ExtraStringRepository* repo_for_segname(const char* segname) const {
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
                putchar_and_flush('+');
                break;
                
            case LC_SEGMENT: {
                segment_command segcmd = *static_cast<const segment_command*>(cmd);
                if (streq(segcmd.segname, "__LINKEDIT")) {
                    segcmd.vmsize = _linkedit_size;
                    segcmd.fileoff = _linkedit_offset;
                    segcmd.filesize = _linkedit_size;
                    fwrite(&segcmd, sizeof(segcmd), 1, _f);
                    putchar_and_flush('l');
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
                    putchar_and_flush('s');
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
                putchar_and_flush('y');
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
                putchar_and_flush('d');
                break;
            }
            
            case LC_TWOLEVEL_HINTS: {
                twolevel_hints_command tlcmd = *static_cast<const twolevel_hints_command*>(cmd);
                this->fix_offset(tlcmd.offset);
                fwrite(&tlcmd, sizeof(tlcmd), 1, _f);
                putchar_and_flush('t');
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
                putchar_and_flush('s');
                break;
            }
            */
            
            case LC_CODE_SIGNATURE:
            case LC_SEGMENT_SPLIT_INFO: {
                linkedit_data_command ldcmd = *static_cast<const linkedit_data_command*>(cmd);
                this->fix_offset(ldcmd.dataoff);
                fwrite(&ldcmd, sizeof(ldcmd), 1, _f);
                putchar_and_flush('c');
                break;
            }
            
            case LC_ENCRYPTION_INFO: {
                encryption_info_command eicmd = *static_cast<const encryption_info_command*>(cmd);
                this->fix_offset(eicmd.cryptoff);
                fwrite(&eicmd, sizeof(eicmd), 1, _f);
                putchar_and_flush('e');
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
                fwrite(&dicmd, sizeof(dicmd), 1, _f);
                putchar_and_flush('i');
                break;
            }
        }
    }
    
    void retrieve_segments(const load_command* cmd) {
        if (cmd->cmd == LC_SEGMENT) {
            const segment_command* segcmd = static_cast<const segment_command*>(cmd);
            _segments.push_back(segcmd);
            ExtraStringRepository* repo = this->repo_for_segname(segcmd->segname);
            if (repo)
                repo->set_section_vmaddr(segcmd->vmaddr + segcmd->vmsize);
        }
    }
    
    long from_vmaddr(uint32_t vmaddr) const {
        BOOST_FOREACH(const segment_command* segcmd, _segments) {
            if (segcmd->vmaddr <= vmaddr && vmaddr < segcmd->vmaddr + segcmd->vmsize)
                return vmaddr - segcmd->vmaddr + segcmd->fileoff;
        }
        return -1;
    }
    
    long from_new_vmaddr(uint32_t vmaddr) const {
        BOOST_FOREACH(const segment_command& segcmd, _new_segments) {
            if (segcmd.vmaddr <= vmaddr && vmaddr < segcmd.vmaddr + segcmd.vmsize)
                return vmaddr - segcmd.vmaddr + segcmd.fileoff;
        }
        return -1;
    }
    
    bool contains_address(uint32_t vmaddr) const {
        BOOST_FOREACH(const segment_command* segcmd, _segments) {
            if (segcmd->vmaddr <= vmaddr && vmaddr < segcmd->vmaddr + segcmd->vmsize)
                return true;
        }
        return false;
    }
    
    void prepare_patch_objc_methods(uint32_t method_vmaddr, uint32_t override_vmaddr);
    void prepare_objc_extrastr(const segment_command* segcmd);
    
    void patch_objc_sects_callback(const char*, size_t, uint32_t new_address, const std::vector<uint32_t>& override_addresses) const {
        BOOST_FOREACH(uint32_t vmaddr, override_addresses) {
            long actual_offset = this->from_new_vmaddr(vmaddr);
            assert(actual_offset >= 0);
            fseek(_f, actual_offset, SEEK_SET);
            fwrite(&new_address, 4, 1, _f);
        }
    }
    
    void patch_objc_sects() const {
        _extra_text.foreach_entry(this, &DecachingFile::patch_objc_sects_callback);
        _extra_data.foreach_entry(this, &DecachingFile::patch_objc_sects_callback);
        
        this->patch_objc_sects_callback(NULL, 0, sizeof(method_t), _entsize12_patches);
    }
    
public:
    DecachingFile(const std::string& filename, const mach_header* header, const ProgramContext* context) : 
        _header(header), _context(context),
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
        
        this->foreach_command(&DecachingFile::retrieve_segments);
        BOOST_FOREACH(const segment_command* segcmd, _segments)
            this->prepare_objc_extrastr(segcmd);
        
        BOOST_FOREACH(const segment_command* segcmd, _segments)
            this->write_segment_content(segcmd);
        
        _linkedit_offset = static_cast<uint32_t>(ftell(_f));
        this->foreach_command(&DecachingFile::write_real_linkedit);
        _linkedit_size = static_cast<uint32_t>(ftell(_f)) - _linkedit_offset;
        
        fseek(_f, offsetof(mach_header, sizeofcmds), SEEK_SET);
        uint32_t new_sizeofcmds = _header->sizeofcmds + (_extra_text.has_content() + _extra_data.has_content()) * sizeof(section);
        fwrite(&new_sizeofcmds, sizeof(new_sizeofcmds), 1, _f);
        fseek(_f, sizeof(*header), SEEK_SET);
        this->foreach_command(&DecachingFile::fix_file_offsets);
        
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
    bool _linkonly;
    bool _printmode;
    std::vector<std::pair<const char*, size_t> > _namefilters;
    
    const dyld_cache_header* _header;
    const shared_file_mapping_np* _mapping;
    const dyld_cache_image_info* _images;

public:
    ProgramContext() : 
        _folder("libraries"),
        _filename(NULL),
        _f(NULL),
        _linkonly(false),
        _printmode(false)
    {}

private:
    void print_usage(char* path) const {
        const char* progname = path ? strrchr(path, '/')+1 : "dyld_decache";
        printf(
            "Usage:\n"
            "  %s [-p] [-o folder] [-l] [-f name ...] dyld_shared_cache_armv7\n"
            "\n"
            "Options:\n"
            "  -o folder : Extract files into 'folder'. Default to ./libraries\n"
            "  -l        : Extract for linking only. The __TEXT and __DATA\n"
            "              segments will not be included, and all target will be\n"
            "              soft-linked to a common file.\n"
            "  -p        : Print the content of the cache file and exit.\n"
            "  -f name   : Only extract the file which _ends_ with 'name'. This\n"
            "              option may be specified multiple times to extract\n"
            "              more than one file. If not specified, all files will\n"
            "              be extracted."
        , progname);
    }

    void parse_options(int argc, char* argv[]) {
        int opt;
        
        while ((opt = getopt(argc, argv, "o:plf:")) != -1) {
            switch (opt) {
                case 'o':
                    _folder = optarg;
                    break;
                case 'l':
                    _linkonly = true;
                    break;
                case 'p':
                    _printmode = true;
                    break;
                case 'f':
                    _namefilters.push_back(std::make_pair(optarg, strlen(optarg)));
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
        return _f->peek_data_at<mach_header>(this->from_vmaddr(_images[i].address));
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
    
public:
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
    
    bool is_print_mode() const { return _printmode; }
    bool is_link_mode() const { return _linkonly; }
        
    const char* path_of_image(uint32_t i) const {
        return _f->peek_data_at<char>(_images[i].pathFileOffset);
    }
    
    bool should_skip_image(uint32_t i) const {
        const char* path = this->path_of_image(i);
        if (_namefilters.empty())
            return false;
        size_t path_length = strlen(path);
            
        for (std::vector<std::pair<const char*, size_t> >::const_iterator cit = _namefilters.begin(); cit != _namefilters.end(); ++ cit) {
            if (!strncmp(path + path_length - cit->second, cit->first, cit->second))
                return false;
        }
        
        return true;
    }

    
    bool save_complete_image(uint32_t image_index) const {
        std::string filename (_folder);
        const char* path = this->path_of_image(image_index);
        filename += path;

        printf("Dumping '%s' (%d/%d)\n... ", path, image_index, _header->imagesCount);

        DecachingFile df (filename, this->mach_header_of_image(image_index), this);
        
        printf("\n");
        
        return df.is_open();
    }
    
    void save_all_images() const {
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
        //  "  1234567812345678  1234567812345678  1234567812345678  x (<= x)" 
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


void DecachingFile::write_segment_content(const segment_command* segcmd) {
    ExtraStringRepository* repo = this->repo_for_segname(segcmd->segname);
    
    if (repo) {    
        const char* data_ptr = _context->peek_char_at_vmaddr(segcmd->vmaddr);
        long new_fileoff = ftell(_f);

        fwrite(data_ptr, 1, segcmd->filesize, _f);
        uint32_t filesize = segcmd->filesize;
    
        if (repo->has_content()) {
            repo->foreach_entry(this, &DecachingFile::write_extrastr);
            
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
        putchar_and_flush('w');
    }
}

void DecachingFile::write_real_linkedit(const load_command* cmd) {
    const unsigned char* data_ptr = _context->_f->data();
    
    #define TRY_WRITE(offmem, countmem, objsize, ch) \
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
            putchar_and_flush(ch); \
        }
    
    switch (cmd->cmd) {
        default:
            break;
        
        case LC_DYLD_INFO:
        case LC_DYLD_INFO_ONLY: {
            const dyld_info_command* cmdvar = static_cast<const dyld_info_command*>(cmd);
            TRY_WRITE(rebase_off, rebase_size, 1, 'R');
            TRY_WRITE(bind_off, bind_size, 1, 'B');
            TRY_WRITE(weak_bind_off, weak_bind_size, 1, 'W');
            TRY_WRITE(lazy_bind_off, lazy_bind_size, 1, 'L');
            TRY_WRITE(export_off, export_size, 1, 'X');
            break;
        }
        
        case LC_SYMTAB: {
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
                putchar_and_flush('S');
                
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
                putchar_and_flush('Y');
            }

            break;
        }
        
        case LC_DYSYMTAB: {
            const dysymtab_command* cmdvar = static_cast<const dysymtab_command*>(cmd);
            TRY_WRITE(tocoff, ntoc, 8, 'T');
            TRY_WRITE(modtaboff, nmodtab, 52, 'M');
            TRY_WRITE(extrefsymoff, nextrefsyms, 4, 'F');
            TRY_WRITE(indirectsymoff, nindirectsyms, 4, 'I');
            TRY_WRITE(extreloff, nextrel, 8, 'E');
            TRY_WRITE(locreloff, nlocrel, 8, 'C');
            break;
        }
    }
    
    #undef TRY_WRITE
}

void DecachingFile::prepare_patch_objc_methods(uint32_t method_vmaddr, uint32_t override_vmaddr) {
    if (!method_vmaddr)
        return;
    
    off_t method_offset = _context->from_vmaddr(method_vmaddr);
    _context->_f->seek(method_offset);
    bool wrong_entsize = _context->_f->copy_data<uint32_t>() != sizeof(method_t);
    uint32_t count = _context->_f->copy_data<uint32_t>();
    
    if (!this->contains_address(method_vmaddr)) {
        method_vmaddr = _extra_data.next_vmaddr();
        size_t size = 8 + sizeof(method_t)*count;
        _extra_data.insert(_context->_f->peek_data_at<char>(method_offset), size, override_vmaddr);
    }
    
    if (wrong_entsize)
        _entsize12_patches.push_back(method_vmaddr);
        
    const method_t* methods = _context->_f->peek_data<method_t>();
    for (uint32_t j = 0; j < count; ++ j) {
        if (!this->contains_address(methods[j].name)) {
            const char* the_string = _context->peek_char_at_vmaddr(methods[j].name);
            _extra_text.insert(the_string, method_vmaddr + 8 + sizeof(method_t)*j);
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
                    const class_ro_t* class_data = reinterpret_cast<const class_ro_t*>(_context->peek_char_at_vmaddr(class_obj->data));
                    this->prepare_patch_objc_methods(class_data->baseMethods, class_obj->data + offsetof(class_ro_t, baseMethods));
                    const class_t* metaclass_obj = reinterpret_cast<const class_t*>(_context->peek_char_at_vmaddr(class_obj->isa));
                    const class_ro_t* metaclass_data = reinterpret_cast<const class_ro_t*>(_context->peek_char_at_vmaddr(metaclass_obj->data));
                    this->prepare_patch_objc_methods(metaclass_data->baseMethods, metaclass_obj->data + offsetof(class_ro_t, baseMethods));
                }
            } else if (streq(sect.sectname, "__objc_protolist")) {
                const uint32_t* protos = _context->_f->peek_data_at<uint32_t>(sect.offset);
                for (uint32_t j = 0; j < sect.size/4; ++ j) {
                    uint32_t proto_vmaddr = protos[j];
                    const protocol_t* proto_obj = reinterpret_cast<const protocol_t*>(_context->peek_char_at_vmaddr(proto_vmaddr));
                    this->prepare_patch_objc_methods(proto_obj->instanceMethods, proto_vmaddr + offsetof(protocol_t, instanceMethods));
                    this->prepare_patch_objc_methods(proto_obj->classMethods, proto_vmaddr + offsetof(protocol_t, classMethods));
                    this->prepare_patch_objc_methods(proto_obj->optionalInstanceMethods, proto_vmaddr + offsetof(protocol_t, optionalInstanceMethods));
                    this->prepare_patch_objc_methods(proto_obj->optionalClassMethods, proto_vmaddr + offsetof(protocol_t, optionalClassMethods));
                }
            } else if (streq(sect.sectname, "__objc_catlist")) {
                const uint32_t* cats = _context->_f->peek_data_at<uint32_t>(sect.offset);
                for (uint32_t j = 0; j < sect.size/4; ++ j) {
                    uint32_t cat_vmaddr = cats[j];
                    const category_t* cat_obj = reinterpret_cast<const category_t*>(_context->peek_char_at_vmaddr(cat_vmaddr));
                    this->prepare_patch_objc_methods(cat_obj->instanceMethods, cat_vmaddr + offsetof(category_t, instanceMethods));
                    this->prepare_patch_objc_methods(cat_obj->classMethods, cat_vmaddr + offsetof(category_t, classMethods));
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
