/*

log_rename.idc ... IDA Pro 6.0 script for renaming functions based on call to a
                   logger function.
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

#include <idc.idc>

static main () {
    auto logfunc_user_ea, logfunc_ea, caller_ea, head_ea, head_operand, reg, head_op, caller_start;
    auto conflict_count, string_ea, func_name, orig_func_name, what_reg;
    
    if (GetLongPrm(INF_FILETYPE) != 25 || (GetLongPrm(INF_PROCNAME) & 0xffffff) != 0x4d5241) {
        Warning("log_rename.idc only works for Mach-O binaries with ARM processors.");
        return;
    }

    logfunc_user_ea = AskAddr(ScreenEA(), "Please enter the address or name of the logger function.");
    logfunc_ea = GetFunctionAttr(logfunc_user_ea, FUNCATTR_START);
    if (logfunc_ea < 0) {
        Warning("The address you have entered (%x) does not belong to a function.", logfunc_user_ea);
        return;
    }
    
    what_reg = AskLong(0, "Which parameter will contain the function's name? (r0 = 0, r1 = 1, etc.)");
    conflict_count = 0;
    
    caller_ea = RfirstB(logfunc_ea);
    while (caller_ea >= 0) {
        head_ea = caller_ea;
        reg = what_reg;
        caller_start = GetFunctionAttr(caller_ea, FUNCATTR_START);
        if (caller_start < 0) {
            Message("S0 [%x]: not defined within a function.\n", caller_ea);
            conflict_count ++;
        } else {
            while (head_ea >= 0) {
                head_ea = PrevHead(head_ea, caller_start);
                if (GetOpType(head_ea, 0) == o_reg && GetOperandValue(head_ea, 0) == reg) {
                    head_op = substr(GetMnem(head_ea), 0, 3);
                    func_name = "";
                    
                    if (head_op == "ldr" || head_op == "LDR") {
                        head_operand = GetOperandValue(head_ea, 1);
                        string_ea = Dword(head_operand);
                        MakeStr(string_ea, BADADDR);
                        func_name = GetString(string_ea, -1, ASCSTR_C);
                    } else if (head_op == "mov" || head_op == "MOV") {
                        if (GetOpType(head_ea, 1) == o_reg) {
                            reg = GetOperandValue(head_ea, 1);
                            continue;
                        }
                    }
                        
                    if (func_name == "") {
                        string_ea = Dfirst(head_ea);
                        if (isASCII(GetFlags(string_ea)))
                            func_name = GetString(string_ea, -1, ASCSTR_C);
                        else {
                            Message("S1 [%x]: don't know how to get function name from '%s'.\n", caller_ea, GetDisasm(head_ea));
                            conflict_count ++;
                            break;
                        }
                    }
                    
                    if (func_name == "") {
                        Message("S2 [%x]: invalid string received while parsing instruction '%s' at %x.\n", caller_ea, GetDisasm(head_ea), head_ea);
                        conflict_count ++;
                    } else {
                        orig_func_name = GetFunctionName(caller_start);
                        if (orig_func_name != func_name) {
                            if (substr(orig_func_name, 0, 4) == "sub_" || substr(orig_func_name, 0, 8) == "nullsub_") {
                                if (!MakeNameEx(caller_start, func_name, SN_NOWARN|SN_CHECK)) {
                                    Message("E3 [%x]: fail to rename %s (%x) to %s.\n", caller_ea, orig_func_name, caller_start, func_name);
                                    conflict_count ++;
                                }
                            } else {
                                Message("S4 [%x]: cannot rename to %s, as the function was already named as %s.\n", caller_ea, func_name, orig_func_name);
                                conflict_count ++;
                            }
                        }
                    }
                    break;
                }
            }
            if (head_ea < 0) {
                Message("S5 [%x]: reached beginning of function but no instruction is assigning to r%d.\n", caller_ea, reg);
                conflict_count ++;
            }
        }
            
        caller_ea = RnextB(logfunc_ea, caller_ea);
    }
    
    if (conflict_count > 0) {
        Warning("The script completed with %d conflicts or errors. Please check the output pane.", conflict_count);
    }
}
