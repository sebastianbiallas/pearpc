/*
 *  PearPC
 *  ppc_dec.cc
 *
 *  Copyright (C) 2026 Sebastian Biallas (sb@biallas.net)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include <cstring>

#include "system/types.h"
#include "debug/tracers.h"
#include "cpu/cpu.h"
#include "ppc_alu.h"
#include "ppc_cpu.h"
#include "ppc_dec.h"
#include "ppc_esc.h"
#include "ppc_exc.h"
#include "ppc_fpu.h"
#include "ppc_vec.h"
#include "ppc_mmu.h"
#include "ppc_opc.h"
#include "jitc_asm.h"

#include "io/prom/promosi.h"

static int ppc_opc_invalid(PPC_CPU_State &aCPU)
{
    fprintf(stderr, "[INVALID] opc=%08x pc=%08x\n", aCPU.current_opc,
        aCPU.current_code_base + aCPU.pc_ofs);
    SINGLESTEP("unknown instruction\n");
	return 0;
}

static JITCFlow ppc_opc_gen_invalid(JITC &jitc)
{
    fprintf(stderr, "[JITC] WARNING: unknown opcode %08x at pc_ofs=%04x (base+ofs)\n",
        jitc.current_opc, jitc.pc);
    jitc.clobberAll();
    // Store pc_ofs for exception handler
    jitc.asmMOV(W0, jitc.pc);
    jitc.asmSTRw_cpu(W0, offsetof(PPC_CPU_State, pc_ofs));
    // SRR1 bit 17 = illegal instruction
    jitc.asmMOV(W1, (uint32)0x00080000);
    // Jump to program exception handler
    jitc.asmCALL((NativeAddress)ppc_program_exception_asm);
    return flowEndBlockUnreachable;
}

/*
 *  Naive gen_ wrappers: emit a call to the C++ interpreter function.
 *  Non-branch opcodes return flowContinue (execution continues to next insn).
 *  Branch opcodes return flowEndBlockUnreachable (control flow changes).
 */

/* Non-branch: call interpreter, continue to next instruction */
#define GEN_INTERPRET(name)                                                                                            \
    JITCFlow ppc_opc_gen_##name(JITC &jitc)                                                                            \
    {                                                                                                                  \
        ppc_opc_gen_interpret(jitc, ppc_opc_##name);                                                                   \
        return flowContinue;                                                                                           \
    }

/* Branch: call interpreter, then jump to new PC via npc */
#define GEN_INTERPRET_BRANCH(name)                                                                                     \
    JITCFlow ppc_opc_gen_##name(JITC &jitc)                                                                            \
    {                                                                                                                  \
        ppc_opc_gen_interpret(jitc, ppc_opc_##name);                                                                   \
        /* Load npc from CPU state into W0 */                                                                          \
        jitc.asmLDRw_cpu(W0, offsetof(PPC_CPU_State, npc));                                                             \
        /* Jump to ppc_new_pc_asm */                                                                                   \
        jitc.asmCALL((NativeAddress)ppc_new_pc_asm);                                                                   \
        return flowEndBlockUnreachable;                                                                                \
    }

/* Load/store: check return value for DSI/ISI and dispatch to
 * exception vector on fault. */
#define GEN_INTERPRET_LOADSTORE(name)                                                                                  \
    JITCFlow ppc_opc_gen_##name(JITC &jitc)                                                                            \
    {                                                                                                                  \
        ppc_opc_gen_interpret_loadstore(jitc, ppc_opc_##name);                                                         \
        return flowEndBlock;                                                                                           \
    }

/* Opcodes that change MSR or other global state: end the block */
#define GEN_INTERPRET_ENDBLOCK(name)                                                                                   \
    JITCFlow ppc_opc_gen_##name(JITC &jitc)                                                                            \
    {                                                                                                                  \
        ppc_opc_gen_interpret(jitc, ppc_opc_##name);                                                                   \
        return flowEndBlock;                                                                                           \
    }

/*
 * ALU - native gen_ in ppc_alu.cc:
 *   addi, addis, ori, oris, xori, xoris, cmpi, bx
 *   addx, subfx, andx, orx, xorx, negx, mullwx
 *   slwx, srwx, rlwinmx
 */

GEN_INTERPRET(addi)
GEN_INTERPRET(addis)
GEN_INTERPRET(ori)
GEN_INTERPRET(oris)
GEN_INTERPRET(xori)
GEN_INTERPRET(xoris)
GEN_INTERPRET(cmpi)
GEN_INTERPRET(addx)
GEN_INTERPRET(subfx)
GEN_INTERPRET(andx)
GEN_INTERPRET(orx)
GEN_INTERPRET(xorx)
GEN_INTERPRET(negx)
GEN_INTERPRET(mullwx)
GEN_INTERPRET(slwx)
GEN_INTERPRET(srwx)
GEN_INTERPRET(rlwinmx)

GEN_INTERPRET(addic)
GEN_INTERPRET(addic_)
GEN_INTERPRET(subfic)
GEN_INTERPRET(mulli)
GEN_INTERPRET(andi_)
GEN_INTERPRET(andis_)
GEN_INTERPRET(cmpli)
GEN_INTERPRET(rlwimix)
GEN_INTERPRET(rlwnmx)

/* Group 2 ALU - native: addx, subfx, andx, orx, xorx, negx, mullwx */
GEN_INTERPRET(cmp)
GEN_INTERPRET(cmpl)
GEN_INTERPRET(addcx)
GEN_INTERPRET(addex)
GEN_INTERPRET(addzex)
GEN_INTERPRET(addmex)
GEN_INTERPRET(subfcx)
GEN_INTERPRET(subfex)
GEN_INTERPRET(subfzex)
GEN_INTERPRET(subfmex)
GEN_INTERPRET(mulhwx)
GEN_INTERPRET(mulhwux)
GEN_INTERPRET(divwx)
GEN_INTERPRET(divwux)
GEN_INTERPRET(andcx)
GEN_INTERPRET(orcx)
GEN_INTERPRET(norx)
GEN_INTERPRET(nandx)
GEN_INTERPRET(eqvx)
GEN_INTERPRET(srawx)
GEN_INTERPRET(srawix)
GEN_INTERPRET(cntlzwx)
GEN_INTERPRET(extsbx)
GEN_INTERPRET(extshx)
GEN_INTERPRET(mfcr)
GEN_INTERPRET(mtcrf)
GEN_INTERPRET(mcrxr)

/* SPR */
GEN_INTERPRET(mfspr)
GEN_INTERPRET(mtspr)
GEN_INTERPRET(mftb)
/* mfmsr has native gen_ in ppc_alu.cc */
GEN_INTERPRET_ENDBLOCK(mtmsr)

/* SR — native gen_ in ppc_alu.cc */
/* mfsr, mtsr, mfsrin, mtsrin have native gen_ in ppc_alu.cc */

/* Branch */
/* bx - native gen_ in ppc_alu.cc */
/* bcx has native gen_ in ppc_alu.cc */
/* bclrx and bcctrx have native gen_ in ppc_alu.cc */
GEN_INTERPRET_BRANCH(rfi)
/* sc has native gen_ in ppc_alu.cc */

/* CR */
GEN_INTERPRET(mcrf)
GEN_INTERPRET(crand)
GEN_INTERPRET(crandc)
GEN_INTERPRET(creqv)
GEN_INTERPRET(crnand)
GEN_INTERPRET(crnor)
GEN_INTERPRET(cror)
GEN_INTERPRET(crorc)
GEN_INTERPRET(crxor)

/*
 * Load/Store - native gen_ with TLB fast path in ppc_mmu.cc:
 *   lwz, stw, lbz, stb, lhz, sth
 */
GEN_INTERPRET_LOADSTORE(lwzu)
GEN_INTERPRET_LOADSTORE(lwzx)
GEN_INTERPRET_LOADSTORE(lwzux)
GEN_INTERPRET_LOADSTORE(lbzu)
GEN_INTERPRET_LOADSTORE(lbzx)
GEN_INTERPRET_LOADSTORE(lbzux)
GEN_INTERPRET_LOADSTORE(lhzu)
GEN_INTERPRET_LOADSTORE(lhzx)
GEN_INTERPRET_LOADSTORE(lhzux)
GEN_INTERPRET_LOADSTORE(lha)
GEN_INTERPRET_LOADSTORE(lhau)
GEN_INTERPRET_LOADSTORE(lhax)
GEN_INTERPRET_LOADSTORE(lhaux)
GEN_INTERPRET_LOADSTORE(stwu)
GEN_INTERPRET_LOADSTORE(stwx)
GEN_INTERPRET_LOADSTORE(stwux)
GEN_INTERPRET_LOADSTORE(stbu)
GEN_INTERPRET_LOADSTORE(stbx)
GEN_INTERPRET_LOADSTORE(stbux)
GEN_INTERPRET_LOADSTORE(sthu)
GEN_INTERPRET_LOADSTORE(sthx)
GEN_INTERPRET_LOADSTORE(sthux)
GEN_INTERPRET_LOADSTORE(lmw)
GEN_INTERPRET_LOADSTORE(stmw)
GEN_INTERPRET_LOADSTORE(lwarx)
GEN_INTERPRET_LOADSTORE(stwcx_)
GEN_INTERPRET_LOADSTORE(lswi)
GEN_INTERPRET_LOADSTORE(lswx)
GEN_INTERPRET_LOADSTORE(stswi)
GEN_INTERPRET_LOADSTORE(stswx)
GEN_INTERPRET_LOADSTORE(lwbrx)
GEN_INTERPRET_LOADSTORE(lhbrx)
GEN_INTERPRET_LOADSTORE(stwbrx)
GEN_INTERPRET_LOADSTORE(sthbrx)

/* Cache/stream hints (no-ops) */
GEN_INTERPRET(dss)
GEN_INTERPRET(dstst)

/* FPU load/store */
GEN_INTERPRET_LOADSTORE(lfs)
GEN_INTERPRET_LOADSTORE(lfsu)
GEN_INTERPRET_LOADSTORE(lfsx)
GEN_INTERPRET_LOADSTORE(lfsux)
GEN_INTERPRET_LOADSTORE(lfd)
GEN_INTERPRET_LOADSTORE(lfdu)
GEN_INTERPRET_LOADSTORE(lfdx)
GEN_INTERPRET_LOADSTORE(lfdux)
GEN_INTERPRET_LOADSTORE(stfs)
GEN_INTERPRET_LOADSTORE(stfsu)
GEN_INTERPRET_LOADSTORE(stfsx)
GEN_INTERPRET_LOADSTORE(stfsux)
GEN_INTERPRET_LOADSTORE(stfd)
GEN_INTERPRET_LOADSTORE(stfdu)
GEN_INTERPRET_LOADSTORE(stfdx)
GEN_INTERPRET_LOADSTORE(stfdux)
GEN_INTERPRET_LOADSTORE(stfiwx)

/* FPU arithmetic */
GEN_INTERPRET(fdivx)
GEN_INTERPRET(fdivsx)
GEN_INTERPRET(fsubx)
GEN_INTERPRET(fsubsx)
GEN_INTERPRET(faddx)
GEN_INTERPRET(faddsx)
GEN_INTERPRET(fsqrtx)
GEN_INTERPRET(fsqrtsx)
GEN_INTERPRET(fresx)
GEN_INTERPRET(fmulx)
GEN_INTERPRET(fmulsx)
GEN_INTERPRET(frsqrtex)
GEN_INTERPRET(fmsubx)
GEN_INTERPRET(fmsubsx)
GEN_INTERPRET(fmaddx)
GEN_INTERPRET(fmaddsx)
GEN_INTERPRET(fnmsubx)
GEN_INTERPRET(fnmsubsx)
GEN_INTERPRET(fnmaddx)
GEN_INTERPRET(fnmaddsx)
GEN_INTERPRET(fselx)
GEN_INTERPRET(fcmpu)
GEN_INTERPRET(fcmpo)
GEN_INTERPRET(frspx)
GEN_INTERPRET(fctiwx)
GEN_INTERPRET(fctiwzx)
GEN_INTERPRET(fabsx)
GEN_INTERPRET(fnabsx)
GEN_INTERPRET(fnegx)
GEN_INTERPRET(fmrx)
GEN_INTERPRET(mffsx)
GEN_INTERPRET(mcrfs)
GEN_INTERPRET(mtfsb0x)
GEN_INTERPRET(mtfsb1x)
GEN_INTERPRET(mtfsfx)
GEN_INTERPRET(mtfsfix)

/* Misc */
/* twi, tw have native gen_ in ppc_alu.cc */
GEN_INTERPRET(eciwx)
GEN_INTERPRET(ecowx)
GEN_INTERPRET(isync)
GEN_INTERPRET(sync)
GEN_INTERPRET(eieio)
/* tlbie, tlbia, tlbsync have native gen_ in ppc_alu.cc */
GEN_INTERPRET_ENDBLOCK(icbi)
GEN_INTERPRET_LOADSTORE(dcbz)

/* AltiVec load/store */
GEN_INTERPRET_LOADSTORE(lvx)
GEN_INTERPRET_LOADSTORE(lvxl)
GEN_INTERPRET_LOADSTORE(lvebx)
GEN_INTERPRET_LOADSTORE(lvehx)
GEN_INTERPRET_LOADSTORE(lvewx)
GEN_INTERPRET_LOADSTORE(lvsl)
GEN_INTERPRET_LOADSTORE(lvsr)
GEN_INTERPRET_LOADSTORE(stvx)
GEN_INTERPRET_LOADSTORE(stvxl)
GEN_INTERPRET_LOADSTORE(stvebx)
GEN_INTERPRET_LOADSTORE(stvehx)
GEN_INTERPRET_LOADSTORE(stvewx)
GEN_INTERPRET(dst)
GEN_INTERPRET(dcba)
GEN_INTERPRET(dcbf)
GEN_INTERPRET(dcbi)
GEN_INTERPRET(dcbst)
GEN_INTERPRET(dcbt)
GEN_INTERPRET(dcbtst)

static int ppc_opc_special(PPC_CPU_State &aCPU)
{
    if (aCPU.pc == gPromOSIEntry && aCPU.current_opc == PROM_MAGIC_OPCODE) {
        call_prom_osi();
        return 0;
    }
    if (aCPU.current_opc == 0x00333301) {
        // memset(r3, r4, r5)
        uint32 dest = aCPU.gpr[3];
        uint32 c = aCPU.gpr[4];
        uint32 size = aCPU.gpr[5];
        if (dest & 0xfff) {
            byte *dst;
            ppc_direct_effective_memory_handle(aCPU, dest, dst);
            uint32 a = 4096 - (dest & 0xfff);
            memset(dst, c, a);
            size -= a;
            dest += a;
        }
        while (size >= 4096) {
            byte *dst;
            ppc_direct_effective_memory_handle(aCPU, dest, dst);
            memset(dst, c, 4096);
            dest += 4096;
            size -= 4096;
        }
        if (size) {
            byte *dst;
            ppc_direct_effective_memory_handle(aCPU, dest, dst);
            memset(dst, c, size);
        }
        aCPU.pc = aCPU.npc;
        return 0;
    }
    if (aCPU.current_opc == 0x00333302) {
        // memcpy
        uint32 dest = aCPU.gpr[3];
        uint32 src = aCPU.gpr[4];
        uint32 size = aCPU.gpr[5];
        byte *d, *s;
        ppc_direct_effective_memory_handle(aCPU, dest, d);
        ppc_direct_effective_memory_handle(aCPU, src, s);
        while (size--) {
            if (!(dest & 0xfff)) {
                ppc_direct_effective_memory_handle(aCPU, dest, d);
            }
            if (!(src & 0xfff)) {
                ppc_direct_effective_memory_handle(aCPU, src, s);
            }
            *d = *s;
            src++;
            dest++;
            d++;
            s++;
        }
        aCPU.pc = aCPU.npc;
        return 0;
    }
    if (aCPU.current_opc == 0x00333303) {
        // print string: r3 = address, r4 = length
        uint32 addr = aCPU.gpr[3];
        uint32 len = aCPU.gpr[4];
        for (uint32 i = 0; i < len; i++) {
            uint8 ch;
            if (ppc_read_effective_byte(aCPU, addr + i, ch) == PPC_MMU_OK) {
                ht_printf("%c", ch);
            }
        }
        aCPU.pc = aCPU.npc;
        return 0;
    }
    if (aCPU.current_opc == 0x00333304) {
        // exit: r3 = exit code
        ht_printf("[TEST] exit with code %d\n", aCPU.gpr[3]);
        exit(aCPU.gpr[3]);
    }
    ppc_opc_invalid(aCPU);
	return 0;
}

static JITCFlow ppc_opc_gen_special(JITC &jitc)
{
    // Handle custom opcodes and PROM OSI calls via interpreter
    ppc_opc_gen_interpret(jitc, ppc_opc_special);
    return flowEndBlock;
}

// main opcode 19
static int ppc_opc_group_1(PPC_CPU_State &aCPU)
{
    uint32 ext = PPC_OPC_EXT(aCPU.current_opc);
    if (ext & 1) {
        // crxxx
        if (ext <= 225) {
            switch (ext) {
            case 33: ppc_opc_crnor(aCPU); return 0;
            case 129: ppc_opc_crandc(aCPU); return 0;
            case 193: ppc_opc_crxor(aCPU); return 0;
            case 225: ppc_opc_crnand(aCPU); return 0;
            }
        } else {
            switch (ext) {
            case 257: ppc_opc_crand(aCPU); return 0;
            case 289: ppc_opc_creqv(aCPU); return 0;
            case 417: ppc_opc_crorc(aCPU); return 0;
            case 449: ppc_opc_cror(aCPU); return 0;
            }
        }
    } else if (ext & (1 << 9)) {
        // bcctrx
        if (ext == 528) {
            ppc_opc_bcctrx(aCPU);
            return 0;
        }
    } else {
        switch (ext) {
        case 16: ppc_opc_bclrx(aCPU); return 0;
        case 0: ppc_opc_mcrf(aCPU); return 0;
        case 50: ppc_opc_rfi(aCPU); return 0;
        case 150: ppc_opc_isync(aCPU); return 0;
        }
    }
    return ppc_opc_invalid(aCPU);
}

static JITCFlow ppc_opc_gen_group_1(JITC &aJITC)
{
    uint32 ext = PPC_OPC_EXT(aJITC.current_opc);
    if (ext & 1) {
        if (ext <= 225) {
            switch (ext) {
            case 33: return ppc_opc_gen_crnor(aJITC);
            case 129: return ppc_opc_gen_crandc(aJITC);
            case 193: return ppc_opc_gen_crxor(aJITC);
            case 225: return ppc_opc_gen_crnand(aJITC);
            }
        } else {
            switch (ext) {
            case 257: return ppc_opc_gen_crand(aJITC);
            case 289: return ppc_opc_gen_creqv(aJITC);
            case 417: return ppc_opc_gen_crorc(aJITC);
            case 449: return ppc_opc_gen_cror(aJITC);
            }
        }
    } else if (ext & (1 << 9)) {
        if (ext == 528) {
            return ppc_opc_gen_bcctrx(aJITC);
        }
    } else {
        switch (ext) {
        case 16: return ppc_opc_gen_bclrx(aJITC);
        case 0: return ppc_opc_gen_mcrf(aJITC);
        case 50: return ppc_opc_gen_rfi(aJITC);
        case 150: return ppc_opc_gen_isync(aJITC);
        }
    }
    return ppc_opc_gen_invalid(aJITC);
}

ppc_opc_function ppc_opc_table_group2[1015];
ppc_opc_gen_function ppc_opc_table_gen_group2[1015];

// main opcode 31
static void ppc_opc_init_group2()
{
    for (uint i = 0; i < (sizeof ppc_opc_table_group2 / sizeof ppc_opc_table_group2[0]); i++) {
        ppc_opc_table_group2[i] = ppc_opc_invalid;
        ppc_opc_table_gen_group2[i] = ppc_opc_gen_invalid;
    }
    ppc_opc_table_group2[0] = ppc_opc_cmp;
    ppc_opc_table_group2[4] = ppc_opc_tw;
    ppc_opc_table_group2[8] = ppc_opc_subfcx;
    ppc_opc_table_group2[10] = ppc_opc_addcx;
    ppc_opc_table_group2[11] = ppc_opc_mulhwux;
    ppc_opc_table_group2[19] = ppc_opc_mfcr;
    ppc_opc_table_group2[20] = ppc_opc_lwarx;
    ppc_opc_table_group2[23] = ppc_opc_lwzx;
    ppc_opc_table_group2[24] = ppc_opc_slwx;
    ppc_opc_table_group2[26] = ppc_opc_cntlzwx;
    ppc_opc_table_group2[28] = ppc_opc_andx;
    ppc_opc_table_group2[32] = ppc_opc_cmpl;
    ppc_opc_table_group2[40] = ppc_opc_subfx;
    ppc_opc_table_group2[54] = ppc_opc_dcbst;
    ppc_opc_table_group2[55] = ppc_opc_lwzux;
    ppc_opc_table_group2[60] = ppc_opc_andcx;
    ppc_opc_table_group2[75] = ppc_opc_mulhwx;
    ppc_opc_table_group2[83] = ppc_opc_mfmsr;
    ppc_opc_table_group2[86] = ppc_opc_dcbf;
    ppc_opc_table_group2[87] = ppc_opc_lbzx;
    ppc_opc_table_group2[104] = ppc_opc_negx;
    ppc_opc_table_group2[119] = ppc_opc_lbzux;
    ppc_opc_table_group2[124] = ppc_opc_norx;
    ppc_opc_table_group2[136] = ppc_opc_subfex;
    ppc_opc_table_group2[138] = ppc_opc_addex;
    ppc_opc_table_group2[144] = ppc_opc_mtcrf;
    ppc_opc_table_group2[146] = ppc_opc_mtmsr;
    ppc_opc_table_group2[150] = ppc_opc_stwcx_;
    ppc_opc_table_group2[151] = ppc_opc_stwx;
    ppc_opc_table_group2[183] = ppc_opc_stwux;
    ppc_opc_table_group2[200] = ppc_opc_subfzex;
    ppc_opc_table_group2[202] = ppc_opc_addzex;
    ppc_opc_table_group2[210] = ppc_opc_mtsr;
    ppc_opc_table_group2[215] = ppc_opc_stbx;
    ppc_opc_table_group2[232] = ppc_opc_subfmex;
    ppc_opc_table_group2[234] = ppc_opc_addmex;
    ppc_opc_table_group2[235] = ppc_opc_mullwx;
    ppc_opc_table_group2[242] = ppc_opc_mtsrin;
    ppc_opc_table_group2[246] = ppc_opc_dcbtst;
    ppc_opc_table_group2[247] = ppc_opc_stbux;
    ppc_opc_table_group2[266] = ppc_opc_addx;
    ppc_opc_table_group2[278] = ppc_opc_dcbt;
    ppc_opc_table_group2[279] = ppc_opc_lhzx;
    ppc_opc_table_group2[284] = ppc_opc_eqvx;
    ppc_opc_table_group2[306] = ppc_opc_tlbie;
    ppc_opc_table_group2[310] = ppc_opc_eciwx;
    ppc_opc_table_group2[311] = ppc_opc_lhzux;
    ppc_opc_table_group2[316] = ppc_opc_xorx;
    ppc_opc_table_group2[339] = ppc_opc_mfspr;
    ppc_opc_table_group2[343] = ppc_opc_lhax;
    ppc_opc_table_group2[370] = ppc_opc_tlbia;
    ppc_opc_table_group2[371] = ppc_opc_mftb;
    ppc_opc_table_group2[375] = ppc_opc_lhaux;
    ppc_opc_table_group2[407] = ppc_opc_sthx;
    ppc_opc_table_group2[412] = ppc_opc_orcx;
    ppc_opc_table_group2[438] = ppc_opc_ecowx;
    ppc_opc_table_group2[439] = ppc_opc_sthux;
    ppc_opc_table_group2[444] = ppc_opc_orx;
    ppc_opc_table_group2[459] = ppc_opc_divwux;
    ppc_opc_table_group2[467] = ppc_opc_mtspr;
    ppc_opc_table_group2[470] = ppc_opc_dcbi;
    ppc_opc_table_group2[476] = ppc_opc_nandx;
    ppc_opc_table_group2[491] = ppc_opc_divwx;
    ppc_opc_table_group2[512] = ppc_opc_mcrxr;
    ppc_opc_table_group2[533] = ppc_opc_lswx;
    ppc_opc_table_group2[534] = ppc_opc_lwbrx;
    ppc_opc_table_group2[535] = ppc_opc_lfsx;
    ppc_opc_table_group2[536] = ppc_opc_srwx;
    ppc_opc_table_group2[566] = ppc_opc_tlbsync;
    ppc_opc_table_group2[567] = ppc_opc_lfsux;
    ppc_opc_table_group2[595] = ppc_opc_mfsr;
    ppc_opc_table_group2[597] = ppc_opc_lswi;
    ppc_opc_table_group2[598] = ppc_opc_sync;
    ppc_opc_table_group2[599] = ppc_opc_lfdx;
    ppc_opc_table_group2[631] = ppc_opc_lfdux;
    ppc_opc_table_group2[659] = ppc_opc_mfsrin;
    ppc_opc_table_group2[661] = ppc_opc_stswx;
    ppc_opc_table_group2[662] = ppc_opc_stwbrx;
    ppc_opc_table_group2[663] = ppc_opc_stfsx;
    ppc_opc_table_group2[695] = ppc_opc_stfsux;
    ppc_opc_table_group2[725] = ppc_opc_stswi;
    ppc_opc_table_group2[727] = ppc_opc_stfdx;
    ppc_opc_table_group2[758] = ppc_opc_dcba;
    ppc_opc_table_group2[759] = ppc_opc_stfdux;
    ppc_opc_table_group2[790] = ppc_opc_lhbrx;
    ppc_opc_table_group2[792] = ppc_opc_srawx;
    ppc_opc_table_group2[824] = ppc_opc_srawix;
    ppc_opc_table_group2[854] = ppc_opc_eieio;
    ppc_opc_table_group2[918] = ppc_opc_sthbrx;
    ppc_opc_table_group2[922] = ppc_opc_extshx;
    ppc_opc_table_group2[954] = ppc_opc_extsbx;
    ppc_opc_table_group2[982] = ppc_opc_icbi;
    ppc_opc_table_group2[983] = ppc_opc_stfiwx;
    ppc_opc_table_group2[1014] = ppc_opc_dcbz;

    // Gen functions: naive interpreter calls
    ppc_opc_table_gen_group2[0] = ppc_opc_gen_cmp;
    ppc_opc_table_gen_group2[4] = ppc_opc_gen_tw;
    ppc_opc_table_gen_group2[8] = ppc_opc_gen_subfcx;
    ppc_opc_table_gen_group2[10] = ppc_opc_gen_addcx;
    ppc_opc_table_gen_group2[11] = ppc_opc_gen_mulhwux;
    ppc_opc_table_gen_group2[19] = ppc_opc_gen_mfcr;
    ppc_opc_table_gen_group2[20] = ppc_opc_gen_lwarx;
    ppc_opc_table_gen_group2[23] = ppc_opc_gen_lwzx;
    ppc_opc_table_gen_group2[24] = ppc_opc_gen_slwx;
    ppc_opc_table_gen_group2[26] = ppc_opc_gen_cntlzwx;
    ppc_opc_table_gen_group2[28] = ppc_opc_gen_andx;
    ppc_opc_table_gen_group2[32] = ppc_opc_gen_cmpl;
    ppc_opc_table_gen_group2[40] = ppc_opc_gen_subfx;
    ppc_opc_table_gen_group2[54] = ppc_opc_gen_dcbst;
    ppc_opc_table_gen_group2[55] = ppc_opc_gen_lwzux;
    ppc_opc_table_gen_group2[60] = ppc_opc_gen_andcx;
    ppc_opc_table_gen_group2[75] = ppc_opc_gen_mulhwx;
    ppc_opc_table_gen_group2[83] = ppc_opc_gen_mfmsr;
    ppc_opc_table_gen_group2[86] = ppc_opc_gen_dcbf;
    ppc_opc_table_gen_group2[87] = ppc_opc_gen_lbzx;
    ppc_opc_table_gen_group2[104] = ppc_opc_gen_negx;
    ppc_opc_table_gen_group2[119] = ppc_opc_gen_lbzux;
    ppc_opc_table_gen_group2[124] = ppc_opc_gen_norx;
    ppc_opc_table_gen_group2[136] = ppc_opc_gen_subfex;
    ppc_opc_table_gen_group2[138] = ppc_opc_gen_addex;
    ppc_opc_table_gen_group2[144] = ppc_opc_gen_mtcrf;
    ppc_opc_table_gen_group2[146] = ppc_opc_gen_mtmsr;
    ppc_opc_table_gen_group2[150] = ppc_opc_gen_stwcx_;
    ppc_opc_table_gen_group2[151] = ppc_opc_gen_stwx;
    ppc_opc_table_gen_group2[183] = ppc_opc_gen_stwux;
    ppc_opc_table_gen_group2[200] = ppc_opc_gen_subfzex;
    ppc_opc_table_gen_group2[202] = ppc_opc_gen_addzex;
    ppc_opc_table_gen_group2[210] = ppc_opc_gen_mtsr;
    ppc_opc_table_gen_group2[215] = ppc_opc_gen_stbx;
    ppc_opc_table_gen_group2[232] = ppc_opc_gen_subfmex;
    ppc_opc_table_gen_group2[234] = ppc_opc_gen_addmex;
    ppc_opc_table_gen_group2[235] = ppc_opc_gen_mullwx;
    ppc_opc_table_gen_group2[242] = ppc_opc_gen_mtsrin;
    ppc_opc_table_gen_group2[246] = ppc_opc_gen_dcbtst;
    ppc_opc_table_gen_group2[247] = ppc_opc_gen_stbux;
    ppc_opc_table_gen_group2[266] = ppc_opc_gen_addx;
    ppc_opc_table_gen_group2[278] = ppc_opc_gen_dcbt;
    ppc_opc_table_gen_group2[279] = ppc_opc_gen_lhzx;
    ppc_opc_table_gen_group2[284] = ppc_opc_gen_eqvx;
    ppc_opc_table_gen_group2[306] = ppc_opc_gen_tlbie;
    ppc_opc_table_gen_group2[310] = ppc_opc_gen_eciwx;
    ppc_opc_table_gen_group2[311] = ppc_opc_gen_lhzux;
    ppc_opc_table_gen_group2[316] = ppc_opc_gen_xorx;
    ppc_opc_table_gen_group2[339] = ppc_opc_gen_mfspr;
    ppc_opc_table_gen_group2[343] = ppc_opc_gen_lhax;
    ppc_opc_table_gen_group2[370] = ppc_opc_gen_tlbia;
    ppc_opc_table_gen_group2[371] = ppc_opc_gen_mftb;
    ppc_opc_table_gen_group2[374] = ppc_opc_gen_dstst;
    ppc_opc_table_gen_group2[375] = ppc_opc_gen_lhaux;
    ppc_opc_table_gen_group2[407] = ppc_opc_gen_sthx;
    ppc_opc_table_gen_group2[412] = ppc_opc_gen_orcx;
    ppc_opc_table_gen_group2[438] = ppc_opc_gen_ecowx;
    ppc_opc_table_gen_group2[439] = ppc_opc_gen_sthux;
    ppc_opc_table_gen_group2[444] = ppc_opc_gen_orx;
    ppc_opc_table_gen_group2[459] = ppc_opc_gen_divwux;
    ppc_opc_table_gen_group2[467] = ppc_opc_gen_mtspr;
    ppc_opc_table_gen_group2[470] = ppc_opc_gen_dcbi;
    ppc_opc_table_gen_group2[476] = ppc_opc_gen_nandx;
    ppc_opc_table_gen_group2[491] = ppc_opc_gen_divwx;
    ppc_opc_table_gen_group2[512] = ppc_opc_gen_mcrxr;
    ppc_opc_table_gen_group2[533] = ppc_opc_gen_lswx;
    ppc_opc_table_gen_group2[534] = ppc_opc_gen_lwbrx;
    ppc_opc_table_gen_group2[535] = ppc_opc_gen_lfsx;
    ppc_opc_table_gen_group2[536] = ppc_opc_gen_srwx;
    ppc_opc_table_gen_group2[566] = ppc_opc_gen_tlbsync;
    ppc_opc_table_gen_group2[567] = ppc_opc_gen_lfsux;
    ppc_opc_table_gen_group2[595] = ppc_opc_gen_mfsr;
    ppc_opc_table_gen_group2[597] = ppc_opc_gen_lswi;
    ppc_opc_table_gen_group2[598] = ppc_opc_gen_sync;
    ppc_opc_table_gen_group2[599] = ppc_opc_gen_lfdx;
    ppc_opc_table_gen_group2[631] = ppc_opc_gen_lfdux;
    ppc_opc_table_gen_group2[659] = ppc_opc_gen_mfsrin;
    ppc_opc_table_gen_group2[661] = ppc_opc_gen_stswx;
    ppc_opc_table_gen_group2[662] = ppc_opc_gen_stwbrx;
    ppc_opc_table_gen_group2[663] = ppc_opc_gen_stfsx;
    ppc_opc_table_gen_group2[695] = ppc_opc_gen_stfsux;
    ppc_opc_table_gen_group2[725] = ppc_opc_gen_stswi;
    ppc_opc_table_gen_group2[727] = ppc_opc_gen_stfdx;
    ppc_opc_table_gen_group2[758] = ppc_opc_gen_dcba;
    ppc_opc_table_gen_group2[759] = ppc_opc_gen_stfdux;
    ppc_opc_table_gen_group2[790] = ppc_opc_gen_lhbrx;
    ppc_opc_table_gen_group2[792] = ppc_opc_gen_srawx;
    ppc_opc_table_gen_group2[822] = ppc_opc_gen_dss;
    ppc_opc_table_gen_group2[824] = ppc_opc_gen_srawix;
    ppc_opc_table_gen_group2[854] = ppc_opc_gen_eieio;
    ppc_opc_table_gen_group2[918] = ppc_opc_gen_sthbrx;
    ppc_opc_table_gen_group2[922] = ppc_opc_gen_extshx;
    ppc_opc_table_gen_group2[954] = ppc_opc_gen_extsbx;
    ppc_opc_table_gen_group2[982] = ppc_opc_gen_icbi;
    ppc_opc_table_gen_group2[983] = ppc_opc_gen_stfiwx;
    ppc_opc_table_gen_group2[1014] = ppc_opc_gen_dcbz;

    /* AltiVec load/store (primary opcode 31) */
    if ((ppc_cpu_get_pvr(0) & 0xffff0000) == 0x000c0000) {
        ppc_opc_table_group2[6] = ppc_opc_lvsl;
        ppc_opc_table_group2[7] = ppc_opc_lvebx;
        ppc_opc_table_group2[38] = ppc_opc_lvsr;
        ppc_opc_table_group2[39] = ppc_opc_lvehx;
        ppc_opc_table_group2[71] = ppc_opc_lvewx;
        ppc_opc_table_group2[103] = ppc_opc_lvx;
        ppc_opc_table_group2[135] = ppc_opc_stvebx;
        ppc_opc_table_group2[167] = ppc_opc_stvehx;
        ppc_opc_table_group2[199] = ppc_opc_stvewx;
        ppc_opc_table_group2[231] = ppc_opc_stvx;
        ppc_opc_table_group2[342] = ppc_opc_dst;
        ppc_opc_table_group2[359] = ppc_opc_lvxl;
        ppc_opc_table_group2[374] = ppc_opc_dstst;
        ppc_opc_table_group2[487] = ppc_opc_stvxl;
        ppc_opc_table_group2[822] = ppc_opc_dss;

        ppc_opc_table_gen_group2[6] = ppc_opc_gen_lvsl;
        ppc_opc_table_gen_group2[7] = ppc_opc_gen_lvebx;
        ppc_opc_table_gen_group2[38] = ppc_opc_gen_lvsr;
        ppc_opc_table_gen_group2[39] = ppc_opc_gen_lvehx;
        ppc_opc_table_gen_group2[71] = ppc_opc_gen_lvewx;
        ppc_opc_table_gen_group2[103] = ppc_opc_gen_lvx;
        ppc_opc_table_gen_group2[135] = ppc_opc_gen_stvebx;
        ppc_opc_table_gen_group2[167] = ppc_opc_gen_stvehx;
        ppc_opc_table_gen_group2[199] = ppc_opc_gen_stvewx;
        ppc_opc_table_gen_group2[231] = ppc_opc_gen_stvx;
        ppc_opc_table_gen_group2[342] = ppc_opc_gen_dst;
        ppc_opc_table_gen_group2[359] = ppc_opc_gen_lvxl;
        ppc_opc_table_gen_group2[374] = ppc_opc_gen_dstst;
        ppc_opc_table_gen_group2[487] = ppc_opc_gen_stvxl;
        ppc_opc_table_gen_group2[822] = ppc_opc_gen_dss;
    }
}

// main opcode 31
static int ppc_opc_group_2(PPC_CPU_State &aCPU)
{
    uint32 ext = PPC_OPC_EXT(aCPU.current_opc);
    if (ext >= (sizeof ppc_opc_table_group2 / sizeof ppc_opc_table_group2[0])) {
        ppc_opc_invalid(aCPU);
    }
    ppc_opc_table_group2[ext](aCPU);
	return 0;
}
static JITCFlow ppc_opc_gen_group_2(JITC &aJITC)
{
    uint32 ext = PPC_OPC_EXT(aJITC.current_opc);
    if (ext >= (sizeof ppc_opc_table_group2 / sizeof ppc_opc_table_group2[0])) {
        return ppc_opc_gen_invalid(aJITC);
    }
    return ppc_opc_table_gen_group2[ext](aJITC);
}

/*
 *  Emit a FPU check: if MSR_FP is clear, jump to
 *  ppc_no_fpu_exception_asm (which never returns).
 *  Uses checkedFloat to skip redundant checks within
 *  the same block (same as x86 JIT).
 */
static void ppc_opc_gen_check_fpu(JITC &jitc)
{
    if (!jitc.checkedFloat) {
        jitc.clobberAll();
        // Load MSR, test MSR_FP (bit 13)
        jitc.asmLDRw_cpu(W0, offsetof(PPC_CPU_State, msr));
        // TST W0, #(1<<13) = ANDS WZR, W0, #0x2000 (MSR_FP)
        // Logical immediate encoding for (1<<13): immr=19, imms=0
        jitc.asmTSTw(W0, 19, 0);

        // Precompute body size: MOV(pc) + BL(exception)
        uint body = a64_movw_size(jitc.pc)
                  + a64_bl_size((uint64)ppc_no_fpu_exception_asm);
        jitc.emitAssure(4 + body);
        NativeAddress target = jitc.asmHERE() + 4 + body;
        jitc.asmBccForward(A64_NE, body); // B.NE skip (FP enabled)

        // FP disabled: raise NO_FPU exception
        jitc.asmMOV(W0, jitc.pc);
        jitc.asmCALL((NativeAddress)ppc_no_fpu_exception_asm);
        // ppc_no_fpu_exception_asm does not return

        jitc.asmAssertHERE(target, "check_fpu");
        jitc.checkedFloat = true;
    }
}

// main opcode 59
static int ppc_opc_group_f1(PPC_CPU_State &aCPU)
{
    if ((aCPU.msr & MSR_FP) == 0) {
        ppc_exception(aCPU, PPC_EXC_NO_FPU, 0, 0);
        return 0;
    }
    uint32 ext = PPC_OPC_EXT(aCPU.current_opc);
    switch (ext & 0x1f) {
    case 18: ppc_opc_fdivsx(aCPU); return 0;
    case 20: ppc_opc_fsubsx(aCPU); return 0;
    case 21: ppc_opc_faddsx(aCPU); return 0;
    case 22: ppc_opc_fsqrtsx(aCPU); return 0;
    case 24: ppc_opc_fresx(aCPU); return 0;
    case 25: ppc_opc_fmulsx(aCPU); return 0;
    case 28: ppc_opc_fmsubsx(aCPU); return 0;
    case 29: ppc_opc_fmaddsx(aCPU); return 0;
    case 30: ppc_opc_fnmsubsx(aCPU); return 0;
    case 31: ppc_opc_fnmaddsx(aCPU); return 0;
    }
    ppc_opc_invalid(aCPU);
	return 0;
}
static JITCFlow ppc_opc_gen_group_f1(JITC &aJITC)
{
    ppc_opc_gen_check_fpu(aJITC);
    uint32 ext = PPC_OPC_EXT(aJITC.current_opc);
    switch (ext & 0x1f) {
    case 18: return ppc_opc_gen_fdivsx(aJITC);
    case 20: return ppc_opc_gen_fsubsx(aJITC);
    case 21: return ppc_opc_gen_faddsx(aJITC);
    case 22: return ppc_opc_gen_fsqrtsx(aJITC);
    case 24: return ppc_opc_gen_fresx(aJITC);
    case 25: return ppc_opc_gen_fmulsx(aJITC);
    case 28: return ppc_opc_gen_fmsubsx(aJITC);
    case 29: return ppc_opc_gen_fmaddsx(aJITC);
    case 30: return ppc_opc_gen_fnmsubsx(aJITC);
    case 31: return ppc_opc_gen_fnmaddsx(aJITC);
    }
    return ppc_opc_gen_invalid(aJITC);
}

// main opcode 63
static int ppc_opc_group_f2(PPC_CPU_State &aCPU)
{
    if ((aCPU.msr & MSR_FP) == 0) {
        ppc_exception(aCPU, PPC_EXC_NO_FPU, 0, 0);
        return 0;
    }
    uint32 ext = PPC_OPC_EXT(aCPU.current_opc);
    if (ext & 16) {
        switch (ext & 0x1f) {
        case 18: ppc_opc_fdivx(aCPU); return 0;
        case 20: ppc_opc_fsubx(aCPU); return 0;
        case 21: ppc_opc_faddx(aCPU); return 0;
        case 22: ppc_opc_fsqrtx(aCPU); return 0;
        case 23: ppc_opc_fselx(aCPU); return 0;
        case 25: ppc_opc_fmulx(aCPU); return 0;
        case 26: ppc_opc_frsqrtex(aCPU); return 0;
        case 28: ppc_opc_fmsubx(aCPU); return 0;
        case 29: ppc_opc_fmaddx(aCPU); return 0;
        case 30: ppc_opc_fnmsubx(aCPU); return 0;
        case 31: ppc_opc_fnmaddx(aCPU); return 0;
        }
    } else {
        switch (ext) {
        case 0: ppc_opc_fcmpu(aCPU); return 0;
        case 12: ppc_opc_frspx(aCPU); return 0;
        case 14: ppc_opc_fctiwx(aCPU); return 0;
        case 15: ppc_opc_fctiwzx(aCPU); return 0;
        case 32: ppc_opc_fcmpo(aCPU); return 0;
        case 38: ppc_opc_mtfsb1x(aCPU); return 0;
        case 40: ppc_opc_fnegx(aCPU); return 0;
        case 64: ppc_opc_mcrfs(aCPU); return 0;
        case 70: ppc_opc_mtfsb0x(aCPU); return 0;
        case 72: ppc_opc_fmrx(aCPU); return 0;
        case 134: ppc_opc_mtfsfix(aCPU); return 0;
        case 136: ppc_opc_fnabsx(aCPU); return 0;
        case 264: ppc_opc_fabsx(aCPU); return 0;
        case 583: ppc_opc_mffsx(aCPU); return 0;
        case 711: ppc_opc_mtfsfx(aCPU); return 0;
        }
    }
    ppc_opc_invalid(aCPU);
	return 0;
}
static JITCFlow ppc_opc_gen_group_f2(JITC &aJITC)
{
    ppc_opc_gen_check_fpu(aJITC);
    uint32 ext = PPC_OPC_EXT(aJITC.current_opc);
    if (ext & 16) {
        switch (ext & 0x1f) {
        case 18: return ppc_opc_gen_fdivx(aJITC);
        case 20: return ppc_opc_gen_fsubx(aJITC);
        case 21: return ppc_opc_gen_faddx(aJITC);
        case 22: return ppc_opc_gen_fsqrtx(aJITC);
        case 23: return ppc_opc_gen_fselx(aJITC);
        case 25: return ppc_opc_gen_fmulx(aJITC);
        case 26: return ppc_opc_gen_frsqrtex(aJITC);
        case 28: return ppc_opc_gen_fmsubx(aJITC);
        case 29: return ppc_opc_gen_fmaddx(aJITC);
        case 30: return ppc_opc_gen_fnmsubx(aJITC);
        case 31: return ppc_opc_gen_fnmaddx(aJITC);
        }
    } else {
        switch (ext) {
        case 0: return ppc_opc_gen_fcmpu(aJITC);
        case 12: return ppc_opc_gen_frspx(aJITC);
        case 14: return ppc_opc_gen_fctiwx(aJITC);
        case 15: return ppc_opc_gen_fctiwzx(aJITC);
        case 32: return ppc_opc_gen_fcmpo(aJITC);
        case 38: return ppc_opc_gen_mtfsb1x(aJITC);
        case 40: return ppc_opc_gen_fnegx(aJITC);
        case 64: return ppc_opc_gen_mcrfs(aJITC);
        case 70: return ppc_opc_gen_mtfsb0x(aJITC);
        case 72: return ppc_opc_gen_fmrx(aJITC);
        case 134: return ppc_opc_gen_mtfsfix(aJITC);
        case 136: return ppc_opc_gen_fnabsx(aJITC);
        case 264: return ppc_opc_gen_fabsx(aJITC);
        case 583: return ppc_opc_gen_mffsx(aJITC);
        case 711: return ppc_opc_gen_mtfsfx(aJITC);
        }
    }
    return ppc_opc_gen_invalid(aJITC);
}

ppc_opc_function ppc_opc_table_groupv[965];
ppc_opc_gen_function ppc_opc_table_gen_groupv[965];

// main opcode 04 - AltiVec
static int ppc_opc_group_v(PPC_CPU_State &aCPU)
{
    uint32 ext = PPC_OPC_EXT(aCPU.current_opc);
#ifndef  __VEC_EXC_OFF__
    if ((aCPU.msr & MSR_VEC) == 0) {
        ppc_exception(aCPU, PPC_EXC_NO_VEC);
        return 1;
    }
#endif
    switch(ext & 0x1f) {
    case 16:
        if (aCPU.current_opc & PPC_OPC_Rc)
            return ppc_opc_vmhraddshs(aCPU);
        else
            return ppc_opc_vmhaddshs(aCPU);
    case 17: return ppc_opc_vmladduhm(aCPU);
    case 18:
        if (aCPU.current_opc & PPC_OPC_Rc)
            return ppc_opc_vmsummbm(aCPU);
        else
            return ppc_opc_vmsumubm(aCPU);
    case 19:
        if (aCPU.current_opc & PPC_OPC_Rc)
            return ppc_opc_vmsumuhs(aCPU);
        else
            return ppc_opc_vmsumuhm(aCPU);
    case 20:
        if (aCPU.current_opc & PPC_OPC_Rc)
            return ppc_opc_vmsumshs(aCPU);
        else
            return ppc_opc_vmsumshm(aCPU);
    case 21:
        if (aCPU.current_opc & PPC_OPC_Rc)
            return ppc_opc_vperm(aCPU);
        else
            return ppc_opc_vsel(aCPU);
    case 22: return ppc_opc_vsldoi(aCPU);
    case 23:
        if (aCPU.current_opc & PPC_OPC_Rc)
            return ppc_opc_vnmsubfp(aCPU);
        else
            return ppc_opc_vmaddfp(aCPU);
    }
    switch(ext & 0x1ff)
    {
    case 3: return ppc_opc_vcmpequbx(aCPU);
    case 35: return ppc_opc_vcmpequhx(aCPU);
    case 67: return ppc_opc_vcmpequwx(aCPU);
    case 99: return ppc_opc_vcmpeqfpx(aCPU);
    case 227: return ppc_opc_vcmpgefpx(aCPU);
    case 259: return ppc_opc_vcmpgtubx(aCPU);
    case 291: return ppc_opc_vcmpgtuhx(aCPU);
    case 323: return ppc_opc_vcmpgtuwx(aCPU);
    case 355: return ppc_opc_vcmpgtfpx(aCPU);
    case 387: return ppc_opc_vcmpgtsbx(aCPU);
    case 419: return ppc_opc_vcmpgtshx(aCPU);
    case 451: return ppc_opc_vcmpgtswx(aCPU);
    case 483: return ppc_opc_vcmpbfpx(aCPU);
    }

    if (ext >= (sizeof ppc_opc_table_groupv / sizeof ppc_opc_table_groupv[0])) {
        return ppc_opc_invalid(aCPU);
    }
    return ppc_opc_table_groupv[ext](aCPU);
}

static JITCFlow ppc_opc_gen_group_v(JITC &jitc)
{
    // Route all AltiVec opcodes through interpreter for correctness
    ppc_opc_gen_interpret(jitc, ppc_opc_group_v);
    return flowContinue;
}

static void ppc_opc_init_groupv()
{
    for (uint i=0; i<(sizeof ppc_opc_table_groupv / sizeof ppc_opc_table_groupv[0]); i++) {
        ppc_opc_table_groupv[i] = ppc_opc_invalid;
        ppc_opc_table_gen_groupv[i] = ppc_opc_gen_invalid;
    }
    ppc_opc_table_groupv[0] = ppc_opc_vaddubm;
    ppc_opc_table_groupv[1] = ppc_opc_vmaxub;
    ppc_opc_table_groupv[2] = ppc_opc_vrlb;
    ppc_opc_table_groupv[4] = ppc_opc_vmuloub;
    ppc_opc_table_groupv[5] = ppc_opc_vaddfp;
    ppc_opc_table_groupv[6] = ppc_opc_vmrghb;
    ppc_opc_table_groupv[7] = ppc_opc_vpkuhum;
    ppc_opc_table_groupv[32] = ppc_opc_vadduhm;
    ppc_opc_table_groupv[33] = ppc_opc_vmaxuh;
    ppc_opc_table_groupv[34] = ppc_opc_vrlh;
    ppc_opc_table_groupv[36] = ppc_opc_vmulouh;
    ppc_opc_table_groupv[37] = ppc_opc_vsubfp;
    ppc_opc_table_groupv[38] = ppc_opc_vmrghh;
    ppc_opc_table_groupv[39] = ppc_opc_vpkuwum;
    ppc_opc_table_groupv[42] = ppc_opc_vpkpx;
    ppc_opc_table_groupv[64] = ppc_opc_vadduwm;
    ppc_opc_table_groupv[65] = ppc_opc_vmaxuw;
    ppc_opc_table_groupv[66] = ppc_opc_vrlw;
    ppc_opc_table_groupv[70] = ppc_opc_vmrghw;
    ppc_opc_table_groupv[71] = ppc_opc_vpkuhus;
    ppc_opc_table_groupv[103] = ppc_opc_vpkuwus;
    ppc_opc_table_groupv[129] = ppc_opc_vmaxsb;
    ppc_opc_table_groupv[130] = ppc_opc_vslb;
    ppc_opc_table_groupv[132] = ppc_opc_vmulosb;
    ppc_opc_table_groupv[133] = ppc_opc_vrefp;
    ppc_opc_table_groupv[134] = ppc_opc_vmrglb;
    ppc_opc_table_groupv[135] = ppc_opc_vpkshus;
    ppc_opc_table_groupv[161] = ppc_opc_vmaxsh;
    ppc_opc_table_groupv[162] = ppc_opc_vslh;
    ppc_opc_table_groupv[164] = ppc_opc_vmulosh;
    ppc_opc_table_groupv[165] = ppc_opc_vrsqrtefp;
    ppc_opc_table_groupv[166] = ppc_opc_vmrglh;
    ppc_opc_table_groupv[167] = ppc_opc_vpkswus;
    ppc_opc_table_groupv[192] = ppc_opc_vaddcuw;
    ppc_opc_table_groupv[193] = ppc_opc_vmaxsw;
    ppc_opc_table_groupv[194] = ppc_opc_vslw;
    ppc_opc_table_groupv[197] = ppc_opc_vexptefp;
    ppc_opc_table_groupv[198] = ppc_opc_vmrglw;
    ppc_opc_table_groupv[199] = ppc_opc_vpkshss;
    ppc_opc_table_groupv[226] = ppc_opc_vsl;
    ppc_opc_table_groupv[229] = ppc_opc_vlogefp;
    ppc_opc_table_groupv[231] = ppc_opc_vpkswss;
    ppc_opc_table_groupv[256] = ppc_opc_vaddubs;
    ppc_opc_table_groupv[257] = ppc_opc_vminub;
    ppc_opc_table_groupv[258] = ppc_opc_vsrb;
    ppc_opc_table_groupv[260] = ppc_opc_vmuleub;
    ppc_opc_table_groupv[261] = ppc_opc_vrfin;
    ppc_opc_table_groupv[262] = ppc_opc_vspltb;
    ppc_opc_table_groupv[263] = ppc_opc_vupkhsb;
    ppc_opc_table_groupv[288] = ppc_opc_vadduhs;
    ppc_opc_table_groupv[289] = ppc_opc_vminuh;
    ppc_opc_table_groupv[290] = ppc_opc_vsrh;
    ppc_opc_table_groupv[292] = ppc_opc_vmuleuh;
    ppc_opc_table_groupv[293] = ppc_opc_vrfiz;
    ppc_opc_table_groupv[294] = ppc_opc_vsplth;
    ppc_opc_table_groupv[295] = ppc_opc_vupkhsh;
    ppc_opc_table_groupv[320] = ppc_opc_vadduws;
    ppc_opc_table_groupv[321] = ppc_opc_vminuw;
    ppc_opc_table_groupv[322] = ppc_opc_vsrw;
    ppc_opc_table_groupv[325] = ppc_opc_vrfip;
    ppc_opc_table_groupv[326] = ppc_opc_vspltw;
    ppc_opc_table_groupv[327] = ppc_opc_vupklsb;
    ppc_opc_table_groupv[354] = ppc_opc_vsr;
    ppc_opc_table_groupv[357] = ppc_opc_vrfim;
    ppc_opc_table_groupv[359] = ppc_opc_vupklsh;
    ppc_opc_table_groupv[384] = ppc_opc_vaddsbs;
    ppc_opc_table_groupv[385] = ppc_opc_vminsb;
    ppc_opc_table_groupv[386] = ppc_opc_vsrab;
    ppc_opc_table_groupv[388] = ppc_opc_vmulesb;
    ppc_opc_table_groupv[389] = ppc_opc_vcfux;
    ppc_opc_table_groupv[390] = ppc_opc_vspltisb;
    ppc_opc_table_groupv[391] = ppc_opc_vpkpx;
    ppc_opc_table_groupv[416] = ppc_opc_vaddshs;
    ppc_opc_table_groupv[417] = ppc_opc_vminsh;
    ppc_opc_table_groupv[418] = ppc_opc_vsrah;
    ppc_opc_table_groupv[420] = ppc_opc_vmulesh;
    ppc_opc_table_groupv[421] = ppc_opc_vcfsx;
    ppc_opc_table_groupv[422] = ppc_opc_vspltish;
    ppc_opc_table_groupv[423] = ppc_opc_vupkhpx;
    ppc_opc_table_groupv[448] = ppc_opc_vaddsws;
    ppc_opc_table_groupv[449] = ppc_opc_vminsw;
    ppc_opc_table_groupv[450] = ppc_opc_vsraw;
    ppc_opc_table_groupv[453] = ppc_opc_vctuxs;
    ppc_opc_table_groupv[454] = ppc_opc_vspltisw;
    ppc_opc_table_groupv[485] = ppc_opc_vctsxs;
    ppc_opc_table_groupv[487] = ppc_opc_vupklpx;
    ppc_opc_table_groupv[512] = ppc_opc_vsububm;
    ppc_opc_table_groupv[513] = ppc_opc_vavgub;
    ppc_opc_table_groupv[514] = ppc_opc_vand;
    ppc_opc_table_groupv[517] = ppc_opc_vmaxfp;
    ppc_opc_table_groupv[518] = ppc_opc_vslo;
    ppc_opc_table_groupv[544] = ppc_opc_vsubuhm;
    ppc_opc_table_groupv[545] = ppc_opc_vavguh;
    ppc_opc_table_groupv[546] = ppc_opc_vandc;
    ppc_opc_table_groupv[549] = ppc_opc_vminfp;
    ppc_opc_table_groupv[550] = ppc_opc_vsro;
    ppc_opc_table_groupv[576] = ppc_opc_vsubuwm;
    ppc_opc_table_groupv[577] = ppc_opc_vavguw;
    ppc_opc_table_groupv[578] = ppc_opc_vor;
    ppc_opc_table_groupv[610] = ppc_opc_vxor;
    ppc_opc_table_groupv[641] = ppc_opc_vavgsb;
    ppc_opc_table_groupv[642] = ppc_opc_vnor;
    ppc_opc_table_groupv[673] = ppc_opc_vavgsh;
    ppc_opc_table_groupv[704] = ppc_opc_vsubcuw;
    ppc_opc_table_groupv[705] = ppc_opc_vavgsw;
    ppc_opc_table_groupv[768] = ppc_opc_vsububs;
    ppc_opc_table_groupv[770] = ppc_opc_mfvscr;
    ppc_opc_table_groupv[772] = ppc_opc_vsum4ubs;
    ppc_opc_table_groupv[800] = ppc_opc_vsubuhs;
    ppc_opc_table_groupv[802] = ppc_opc_mtvscr;
    ppc_opc_table_groupv[804] = ppc_opc_vsum4shs;
    ppc_opc_table_groupv[832] = ppc_opc_vsubuws;
    ppc_opc_table_groupv[836] = ppc_opc_vsum2sws;
    ppc_opc_table_groupv[896] = ppc_opc_vsubsbs;
    ppc_opc_table_groupv[900] = ppc_opc_vsum4sbs;
    ppc_opc_table_groupv[928] = ppc_opc_vsubshs;
    ppc_opc_table_groupv[960] = ppc_opc_vsubsws;
    ppc_opc_table_groupv[964] = ppc_opc_vsumsws;
}

static ppc_opc_function ppc_opc_table_main[64] = {
    &ppc_opc_special,  //  0
    &ppc_opc_invalid,  //  1
    &ppc_opc_invalid,  //  2
    &ppc_opc_twi,      //  3
    &ppc_opc_invalid,  //  4
    &ppc_opc_invalid,  //  5
    &ppc_opc_invalid,  //  6
    &ppc_opc_mulli,    //  7
    &ppc_opc_subfic,   //  8
    &ppc_opc_invalid,  //  9
    &ppc_opc_cmpli,    // 10
    &ppc_opc_cmpi,     // 11
    &ppc_opc_addic,    // 12
    &ppc_opc_addic_,   // 13
    &ppc_opc_addi,     // 14
    &ppc_opc_addis,    // 15
    &ppc_opc_bcx,      // 16
    &ppc_opc_sc,       // 17
    &ppc_opc_bx,       // 18
    &ppc_opc_group_1,  // 19
    &ppc_opc_rlwimix,  // 20
    &ppc_opc_rlwinmx,  // 21
    &ppc_opc_invalid,  // 22
    &ppc_opc_rlwnmx,   // 23
    &ppc_opc_ori,      // 24
    &ppc_opc_oris,     // 25
    &ppc_opc_xori,     // 26
    &ppc_opc_xoris,    // 27
    &ppc_opc_andi_,    // 28
    &ppc_opc_andis_,   // 29
    &ppc_opc_invalid,  // 30
    &ppc_opc_group_2,  // 31
    &ppc_opc_lwz,      // 32
    &ppc_opc_lwzu,     // 33
    &ppc_opc_lbz,      // 34
    &ppc_opc_lbzu,     // 35
    &ppc_opc_stw,      // 36
    &ppc_opc_stwu,     // 37
    &ppc_opc_stb,      // 38
    &ppc_opc_stbu,     // 39
    &ppc_opc_lhz,      // 40
    &ppc_opc_lhzu,     // 41
    &ppc_opc_lha,      // 42
    &ppc_opc_lhau,     // 43
    &ppc_opc_sth,      // 44
    &ppc_opc_sthu,     // 45
    &ppc_opc_lmw,      // 46
    &ppc_opc_stmw,     // 47
    &ppc_opc_lfs,      // 48
    &ppc_opc_lfsu,     // 49
    &ppc_opc_lfd,      // 50
    &ppc_opc_lfdu,     // 51
    &ppc_opc_stfs,     // 52
    &ppc_opc_stfsu,    // 53
    &ppc_opc_stfd,     // 54
    &ppc_opc_stfdu,    // 55
    &ppc_opc_invalid,  // 56
    &ppc_opc_invalid,  // 57
    &ppc_opc_invalid,  // 58
    &ppc_opc_group_f1, // 59
    &ppc_opc_invalid,  // 60
    &ppc_opc_invalid,  // 61
    &ppc_opc_invalid,  // 62
    &ppc_opc_group_f2, // 63
};
static ppc_opc_gen_function ppc_opc_table_gen_main[64] = {
    &ppc_opc_gen_special,  //  0
    &ppc_opc_gen_invalid,  //  1
    &ppc_opc_gen_invalid,  //  2
    &ppc_opc_gen_twi,      //  3
    &ppc_opc_gen_invalid,  //  4
    &ppc_opc_gen_invalid,  //  5
    &ppc_opc_gen_invalid,  //  6
    &ppc_opc_gen_mulli,    //  7
    &ppc_opc_gen_subfic,   //  8
    &ppc_opc_gen_invalid,  //  9
    &ppc_opc_gen_cmpli,    // 10
    &ppc_opc_gen_cmpi,     // 11
    &ppc_opc_gen_addic,    // 12
    &ppc_opc_gen_addic_,   // 13
    &ppc_opc_gen_addi,     // 14
    &ppc_opc_gen_addis,    // 15
    &ppc_opc_gen_bcx,      // 16
    &ppc_opc_gen_sc,       // 17
    &ppc_opc_gen_bx,       // 18
    &ppc_opc_gen_group_1,  // 19
    &ppc_opc_gen_rlwimix,  // 20
    &ppc_opc_gen_rlwinmx,  // 21
    &ppc_opc_gen_invalid,  // 22
    &ppc_opc_gen_rlwnmx,   // 23
    &ppc_opc_gen_ori,      // 24
    &ppc_opc_gen_oris,     // 25
    &ppc_opc_gen_xori,     // 26
    &ppc_opc_gen_xoris,    // 27
    &ppc_opc_gen_andi_,    // 28
    &ppc_opc_gen_andis_,   // 29
    &ppc_opc_gen_invalid,  // 30
    &ppc_opc_gen_group_2,  // 31
    &ppc_opc_gen_lwz,      // 32
    &ppc_opc_gen_lwzu,     // 33
    &ppc_opc_gen_lbz,      // 34
    &ppc_opc_gen_lbzu,     // 35
    &ppc_opc_gen_stw,      // 36
    &ppc_opc_gen_stwu,     // 37
    &ppc_opc_gen_stb,      // 38
    &ppc_opc_gen_stbu,     // 39
    &ppc_opc_gen_lhz,      // 40
    &ppc_opc_gen_lhzu,     // 41
    &ppc_opc_gen_lha,      // 42
    &ppc_opc_gen_lhau,     // 43
    &ppc_opc_gen_sth,      // 44
    &ppc_opc_gen_sthu,     // 45
    &ppc_opc_gen_lmw,      // 46
    &ppc_opc_gen_stmw,     // 47
    &ppc_opc_gen_lfs,      // 48
    &ppc_opc_gen_lfsu,     // 49
    &ppc_opc_gen_lfd,      // 50
    &ppc_opc_gen_lfdu,     // 51
    &ppc_opc_gen_stfs,     // 52
    &ppc_opc_gen_stfsu,    // 53
    &ppc_opc_gen_stfd,     // 54
    &ppc_opc_gen_stfdu,    // 55
    &ppc_opc_gen_invalid,  // 56
    &ppc_opc_gen_invalid,  // 57
    &ppc_opc_gen_invalid,  // 58
    &ppc_opc_gen_group_f1, // 59
    &ppc_opc_gen_invalid,  // 60
    &ppc_opc_gen_invalid,  // 61
    &ppc_opc_gen_invalid,  // 62
    &ppc_opc_gen_group_f2, // 63
};

void FASTCALL ppc_exec_opc(PPC_CPU_State &aCPU)
{
    uint32 mainopc = PPC_OPC_MAIN(aCPU.current_opc);
    ppc_opc_table_main[mainopc](aCPU);
}

JITCFlow FASTCALL ppc_gen_opc(JITC &aJITC)
{
    uint32 mainopc = PPC_OPC_MAIN(aJITC.current_opc);
    return ppc_opc_table_gen_main[mainopc](aJITC);
}

void ppc_dec_init()
{
    ppc_opc_init_group2();
    if ((ppc_cpu_get_pvr(0) & 0xffff0000) == 0x000c0000) {
        ht_printf("[PPC/VEC] AltiVec enabled\n");
        ppc_opc_table_main[4] = ppc_opc_group_v;
        ppc_opc_table_gen_main[4] = ppc_opc_gen_group_v;
        ppc_opc_init_groupv();
    }
}
