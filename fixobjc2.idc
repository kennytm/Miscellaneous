/*

fixobjc2.idc ... IDA Pro 5.x script to fix ObjC ABI 2.0 for iPhoneOS binaries.

Copyright (C) 2010  KennyTM~

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

#include <idc.idc>
#define RO_META 1

static offsetize(name) {
	auto base, ea;
	base = SegByBase(SegByName(name));
	if (base >= 0) {
		for (ea = SegStart(base); ea != SegEnd(base); ea = ea + 4) {
			OpOff(ea, 0, 0);
		}
	}
}

static functionize (ea, is_meta, cls, sel) {
	auto is_thumb, head_ea, ea_flags;
	is_thumb = ea & 1;
	ea = ea & ~1;
	ea_flags = GetFlags(ea);
	if (!isCode(ea_flags) || GetReg(ea, "T") != is_thumb) {
		head_ea = !isHead(ea_flags) ? PrevHead(ea, 0) : ea;
		MakeUnkn(head_ea, DOUNK_EXPAND);
		SetRegEx(ea, "T", is_thumb, SR_autostart);
		MakeCode(ea);
	}
	MakeFunction(ea, BADADDR);
	MakeName(ea, (is_meta ? "@" : "") + cls + "." + sel);
	SetFunctionCmt(ea, (is_meta ? "+" : "-") + "[" + cls + " " + sel + "]", 1);
}

static methodize (m_ea, is_meta, cl_name) {
	auto m_size, m_count, m_i, m_sname, m_cname;

	if (m_ea <= 0)
		return;

	m_size = Dword(m_ea);
	m_count = Dword(m_ea + 4);
	MakeStruct(m_ea, "method_list_t");
	MakeName(m_ea, cl_name + (is_meta ? "_$classMethods" : "_$methods"));
	m_ea = m_ea + 8;
	for (m_i = 0; m_i < m_count; m_i = m_i + 1) {
		MakeStruct(m_ea, "method_t");
		m_sname = GetString(Dword(m_ea), -1, ASCSTR_C);
		functionize(Dword(m_ea + 8), is_meta, cl_name, m_sname);
		m_ea = m_ea + m_size;
	}
}

static classize (c_ea, is_meta) {
	auto cd_ea, cl_name, c_name, i_ea, i_count, i_size, i_i, i_sname;
	
	MakeStruct(c_ea, "class_t");
	cd_ea = Dword(c_ea + 16);
	MakeStruct(cd_ea, "class_ro_t");
	cl_name = GetString(Dword(cd_ea + 16), -1, ASCSTR_C);
	MakeName(c_ea, (is_meta ? "_OBJC_METACLASS_$_" : "_OBJC_CLASS_$_") + cl_name);
	MakeName(cd_ea, cl_name + (is_meta ? "_$metaData" : "_$classData"));
	
	// methods
	methodize(Dword(cd_ea + 20), is_meta, cl_name);
	
	// ivars
	i_ea = Dword(cd_ea + 28);
	if (i_ea > 0) {
		i_size = Dword(i_ea);
		i_count = Dword(i_ea + 4);
		MakeStruct(i_ea, "ivar_list_t");
		MakeName(i_ea, cl_name + (is_meta ? "_$classIvars" : "_$ivars"));
		i_ea = i_ea + 8;
		for (i_i = 0; i_i < i_count; i_i = i_i + 1) {
			MakeStruct(i_ea, "ivar_t");
			i_sname = GetString(Dword(i_ea+4), -1, ASCSTR_C);
			MakeDword(Dword(i_ea));
			MakeName(Dword(i_ea), "_OBJC_IVAR_$_" + cl_name + "." + i_sname);
			i_ea = i_ea + i_size;
		}
	}
}

static categorize(c_ea) {
	auto cat_name, cl_name, s_name;
	cat_name = GetString(Dword(c_ea), -1, ASCSTR_C);
	s_name = substr(Name(Dword(c_ea + 4)), 14, -1);
	cl_name = s_name + "(" + cat_name + ")";
	methodize(Dword(c_ea + 8), 0, cl_name);
	methodize(Dword(c_ea + 12), 1, cl_name);
}

static main () {
	auto cl_ea, cl_base, c_ea, s_base, s_ea, s_name, cr_base, cr_ea, cr_target;
	auto cat_base, cat_ea;
	
	if (GetLongPrm(INF_FILETYPE) != 25 || (GetLongPrm(INF_PROCNAME) & 0xffffff) != 0x4d5241) {
		Warning("fixobjc2.idc only works for Mach-O binaries with ARM processors.");
		return;
	}

	offsetize("__objc_classrefs");
	offsetize("__objc_classlist");
	offsetize("__objc_catlist");
	offsetize("__objc_protolist");
	offsetize("__objc_superrefs");
	offsetize("__objc_selrefs");
	
	// find all methods & ivars.
	cl_base = SegByBase(SegByName("__objc_classlist"));
	if (cl_base >= 0) {
		for (cl_ea = SegStart(cl_base); cl_ea != SegEnd(cl_base); cl_ea = cl_ea + 4) {
			c_ea = Dword(cl_ea);
			classize(c_ea, 0);
			classize(Dword(c_ea), 1);
		}
	}
	
	// name all selectors
	s_base = SegByBase(SegByName("__objc_selrefs"));
	if (s_base >= 0) {
		for (s_ea = SegStart(s_base); s_ea != SegEnd(s_base); s_ea = s_ea + 4) {
			s_name = GetString(Dword(s_ea), -1, ASCSTR_C);
			MakeRptCmt(s_ea, "@selector(" + s_name + ")");
		}
	}
	
	// name all classrefs
	cr_base = SegByBase(SegByName("__objc_classrefs"));
	if (cr_base >= 0) {
		for (cr_ea = SegStart(cr_base); cr_ea != SegEnd(cr_base); cr_ea = cr_ea + 4) {
			cr_target = Dword(cr_ea);
			if (cr_target > 0) {
				MakeRptCmt(cr_ea, Name(cr_target));
			}
		}
	}
	
	// categories.
	cat_base = SegByBase(SegByName("__objc_catlist"));
	if (cat_base >= 0) {
		for (cat_ea = SegStart(cat_base); cat_ea != SegEnd(cat_base); cat_ea = cat_ea + 4) {
			categorize(Dword(cat_ea));
		}
	}
}
