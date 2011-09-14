/*

fix_pcrel.idc ... IDA Pro 6.1 script to fix PC-relative offsets in LLVM-
                  generated objects.

Copyright (C) 2011  KennyTM~
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
    if ((GetLongPrm(INF_PROCNAME) & 0xffffff) != 0x4d5241)
    {
        Warning("fix_pcrel.idc is only suitable for ARM processors.");
        return 0;
    }
    else
        return 1;
}

static is_op_register(op, reg)
{
    return op.type == o_reg && op.reg == reg;
}

static get_pc_rel_register(ea)
{
    auto opcode, instr;
    opcode = GetMnem(ea);
    if (opcode == "ADD" || opcode == "add")
    {
        instr = DecodeInstruction(ea);
        if (instr.n == 2)
            if (instr[0].type == o_reg && is_op_register(instr[1], 15))
                return instr[0].reg;
    }
    return -1;
}

/**
Try to apply the PC-relative fix to the 'ea'.
Returns 1 when we want to stop the back-tracking.
*/
static fix(ea, pc_addr, reg, movt_offset)
{
    auto opcode, instr;
    opcode = GetMnem(ea);
    instr = DecodeInstruction(ea);
    if (opcode == "MOVT" || opcode == "movt")
    {
        if (is_op_register(instr[0], reg))
            // bail out if we MOVT some non-numbers.
            if (instr[1].type == o_imm)
                movt_offset = instr[1].value * 0x10000;
            else
                return 1;
    }
    else if (opcode == "MOV" || opcode == "mov" || opcode == "MOVW" || opcode == "movw")
    {
        if (is_op_register(instr[0], reg))
        {
            if (instr[1].type == o_imm)
                OpOffEx(ea, 1, REF_OFF32|REFINFO_NOBASE, -1, pc_addr + movt_offset, 0);
            return 1;
        }
    }
    return 0;
}

static backtrack_fix_mov(reg, start, min)
{
    auto ea, pc_addr, movt_offset;
    pc_addr = start + (GetReg(start, "T") ? 4 : 8);
    movt_offset = 0;
    for (ea = start; ea != BADADDR && ea + 100 >= start; ea = PrevHead(ea, min))
    {
        if (fix(ea, pc_addr, reg, &movt_offset))
            break;
    }
}

static fix_in_range(min, max)
{
    auto ea, reg;
    for (ea = min; ea != BADADDR; ea = NextHead(ea, max))
    {
        reg = get_pc_rel_register(ea);
        if (reg == -1)
            continue;
        backtrack_fix_mov(reg, ea, min);
    }
}

static main()
{
    auto ea_min, ea_max, ea;

    if (!is_arm())
        return;

    ea_min = GetLongPrm(INF_MIN_EA);
    ea_max = GetLongPrm(INF_MAX_EA);

    fix_in_range(ea_min, ea_max);
}

