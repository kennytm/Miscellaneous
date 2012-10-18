/*

fix_pcrel.idc ... IDA Pro 6.1 script to fix PC-relative offsets in LLVM-
                  generated objects.

Copyright (C) 2012  KennyTM~
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, this
  list of conditions and the following disclaimer in the documentation and/or
  other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include <idc.idc>

static is_arm()
{
    return ((GetLongPrm(INF_PROCNAME) & 0xffffff) == 0x4d5241);
}

#define IT_NOP      0
#define IT_SET_PC   1
#define IT_ADD_IMM  2
#define IT_COMMIT   3
#define IT_DISMISS  4

static parse_instruction(ea, pc_store)
{
    auto instr, opcode;
    auto instr_type, instr_reg, instr_op, tmp;

    instr = DecodeInstruction(ea);
    if (!instr)
        return;

    opcode = GetMnem(ea);

    instr_type = IT_NOP;
    instr_reg = -1;
    instr_op = -1;

    // Check what to do w.r.t. each instruction.

    if (opcode == "ADD")
    {
        if (instr[1].type == o_reg && instr[1].reg == 15) {
            if (instr.n == 2) {
                instr_reg = instr[0].reg;
                instr_type = IT_SET_PC;
            } else if (instr.n == 3 && instr[2].type == o_reg) {
                instr_reg = instr[2].reg;
                instr_type = IT_SET_PC;
            }
        }
    }
    else if (opcode == "LDR" || opcode == "STR")
    {
        if (instr[1].type == o_phrase && instr[1].reg == 15) {
            instr_reg = instr[1].specflag1;
            instr_type = IT_SET_PC;
        }
    }
    else if (opcode == "MOVT" || opcode == "MOVT.W")
    {
        if (instr[0].type == o_reg && instr[1].type == o_imm)
        {
            instr_reg = instr[0].reg;
            instr_type = IT_ADD_IMM;
            instr_op = instr[1].value << 16;
        }
    }
    else if (opcode == "MOV" || opcode == "MOVW")
    {
        if (instr[0].type == o_reg)
        {
            instr_reg = instr[0].reg;
            instr_op = 1;
            if (instr[1].type == o_imm)
                instr_type = IT_COMMIT;
            else
                instr_type = IT_DISMISS;
        }
    }

    // Perform the corresponding action.

    if (instr_type == IT_SET_PC)
    {
        pc_store[instr_reg] = ea + (GetReg(ea, "T") ? 4 : 8);
    }
    else if (instr_type == IT_ADD_IMM)
    {
        tmp = pc_store[instr_reg];
        if (tmp != BADADDR)
            pc_store[instr_reg] = tmp + instr_op;
    }
    else if (instr_type == IT_COMMIT)
    {
        tmp = pc_store[instr_reg];
        if (tmp != BADADDR)
        {
            pc_store[instr_reg] = BADADDR;
            OpOffEx(ea, instr_op, REF_OFF32|REFINFO_NOBASE, -1, tmp, 0);
            pc_store.fix = pc_store.fix + 1;
        }
    }
    else if (instr_type == IT_DISMISS)
    {
        pc_store[instr_reg] = BADADDR;
    }
}

static prev_ea(ea, ea_min, pc_store)
{
    auto res, i;

    for (i = 0; i < 15; ++ i)
        if (pc_store[i] != BADADDR) {
            return PrevHead(ea, ea_min);
        }

    res = FindText(ea, SEARCH_UP, 0, -1, "PC");
    if (res < ea_min)
        res = BADADDR;
    return res;
}

static main()
{
    auto ea_begin, ea_end, text_seg_id, seg_ea, ea, pc_store, i, report_ea, prev_status;

    if (!is_arm())
    {
        Warning("fix_pcrel.idc is only suitable for ARM processors.");
        return;
    }

    ea_begin = SelStart();
    ea = SelEnd();

    if (ea == BADADDR)
    {
        text_seg_id = SegByName("__text");
        if (text_seg_id == BADADDR)
        {
            Warning("__text segment not found --- is this a real Mach-O file?");
            return;
        }
        seg_ea = SegByBase(text_seg_id);
        ea_begin = SegStart(seg_ea);
        ea = SegEnd(seg_ea);
    }

    report_ea = ea;

    pc_store = object();
    for (i = 0; i < 15; ++ i)
        pc_store[i] = BADADDR;
    pc_store.fix = 0;

    prev_status = SetStatus(IDA_STATUS_WORK);

    while (ea != BADADDR)
    {
        ea = prev_ea(ea, ea_begin, pc_store);
        parse_instruction(ea, pc_store);

        if (ea < report_ea)
        {
            report_ea = ea - 0x4000;
            Message("Fixing PCRel, at %a, fixed %d instructions...\n", report_ea, pc_store.fix);
        }
    }

    SetStatus(prev_status);

    Message("Done! (Fixed %d instructions)\n", pc_store.fix);
}

