/*
 *	PearPC
 *	ppc_semantics_dispatch.h
 *
 *	Copyright (C) 2003-2006 Sebastian Biallas (sb@biallas.net)
 *
 *	Dispatches a PPC opcode to the appropriate semantics template,
 *	returning an InsnEffect for liveness analysis.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __PPC_SEMANTICS_DISPATCH_H__
#define __PPC_SEMANTICS_DISPATCH_H__

#include "system/arch/sysendian.h"
#include "cpu/ppc_liveness_sem.h"
#include "cpu/ppc_semantics_alu.h"
#include "cpu/ppc_semantics_branch.h"
#include "cpu/ppc_semantics_mem.h"
#include "cpu/ppc_semantics_spr.h"
#include "cpu/ppc_semantics_fpu.h"

// Analyze group 1 (main opcode 19): CR logical + branch ops
static inline InsnEffect ppc_analyze_group1(uint32 opc)
{
    LivenessSemantics s;
    uint32 ext = PPC_OPC_EXT(opc);
    switch (ext) {
    case 33: ppc_sem_crnor(s, opc); return s.fx;
    case 129: ppc_sem_crandc(s, opc); return s.fx;
    case 193: ppc_sem_crxor(s, opc); return s.fx;
    case 225: ppc_sem_crnand(s, opc); return s.fx;
    case 257: ppc_sem_crand(s, opc); return s.fx;
    case 289: ppc_sem_creqv(s, opc); return s.fx;
    case 417: ppc_sem_crorc(s, opc); return s.fx;
    case 449: ppc_sem_cror(s, opc); return s.fx;

    // Branch
    case 16: ppc_sem_bclrx(s, opc); return s.fx;
    case 528: ppc_sem_bcctrx(s, opc); return s.fx;

    // CR field move
    case 0: ppc_sem_mcrf(s, opc); return s.fx;

    default: return InsnEffect::everything();
    }
}

// Analyze group 2 (main opcode 31): ALU register-register ops
static inline InsnEffect ppc_analyze_group2(uint32 opc)
{
    LivenessSemantics s;
    uint32 ext = PPC_OPC_EXT(opc);
    switch (ext) {
    // Compare
    case 0: ppc_sem_cmp(s, opc); return s.fx;
    case 32: ppc_sem_cmpl(s, opc); return s.fx;

    // Subtract
    case 8: ppc_sem_subfcx(s, opc); return s.fx;
    case 40: ppc_sem_subfx(s, opc); return s.fx;
    case 136: ppc_sem_subfex(s, opc); return s.fx;
    case 200: ppc_sem_subfzex(s, opc); return s.fx;
    case 232: ppc_sem_subfmex(s, opc); return s.fx;

    // Add
    case 10: ppc_sem_addcx(s, opc); return s.fx;
    case 138: ppc_sem_addex(s, opc); return s.fx;
    case 202: ppc_sem_addzex(s, opc); return s.fx;
    case 234: ppc_sem_addmex(s, opc); return s.fx;
    case 266: ppc_sem_addx(s, opc); return s.fx;

    // Multiply
    case 11: ppc_sem_mulhwux(s, opc); return s.fx;
    case 75: ppc_sem_mulhwx(s, opc); return s.fx;
    case 235: ppc_sem_mullwx(s, opc); return s.fx;

    // Divide
    case 459: ppc_sem_divwux(s, opc); return s.fx;
    case 491: ppc_sem_divwx(s, opc); return s.fx;

    // Logic
    case 28: ppc_sem_andx(s, opc); return s.fx;
    case 60: ppc_sem_andcx(s, opc); return s.fx;
    case 124: ppc_sem_norx(s, opc); return s.fx;
    case 284: ppc_sem_eqvx(s, opc); return s.fx;
    case 316: ppc_sem_xorx(s, opc); return s.fx;
    case 412: ppc_sem_orcx(s, opc); return s.fx;
    case 444: ppc_sem_orx(s, opc); return s.fx;
    case 476: ppc_sem_nandx(s, opc); return s.fx;

    // Negate
    case 104: ppc_sem_negx(s, opc); return s.fx;

    // Shift
    case 24: ppc_sem_slwx(s, opc); return s.fx;
    case 536: ppc_sem_srwx(s, opc); return s.fx;
    case 792: ppc_sem_srawx(s, opc); return s.fx;
    case 824: ppc_sem_srawix(s, opc); return s.fx;

    // Count leading zeros
    case 26: ppc_sem_cntlzwx(s, opc); return s.fx;

    // Sign extension
    case 922: ppc_sem_extshx(s, opc); return s.fx;
    case 954: ppc_sem_extsbx(s, opc); return s.fx;

    // Load indexed
    case 23: ppc_sem_lwzx(s, opc); return s.fx;
    case 55: ppc_sem_lwzux(s, opc); return s.fx;
    case 87: ppc_sem_lbzx(s, opc); return s.fx;
    case 119: ppc_sem_lbzux(s, opc); return s.fx;
    case 279: ppc_sem_lhzx(s, opc); return s.fx;
    case 311: ppc_sem_lhzux(s, opc); return s.fx;
    case 343: ppc_sem_lhax(s, opc); return s.fx;
    case 375: ppc_sem_lhaux(s, opc); return s.fx;

    // Store indexed
    case 151: ppc_sem_stwx(s, opc); return s.fx;
    case 183: ppc_sem_stwux(s, opc); return s.fx;
    case 215: ppc_sem_stbx(s, opc); return s.fx;
    case 247: ppc_sem_stbux(s, opc); return s.fx;
    case 407: ppc_sem_sthx(s, opc); return s.fx;
    case 439: ppc_sem_sthux(s, opc); return s.fx;

    // SPR/CR moves
    case 19: ppc_sem_mfcr(s, opc); return s.fx;
    case 144: ppc_sem_mtcrf(s, opc); return s.fx;
    case 339: ppc_sem_mfspr(s, opc); return s.fx;
    case 467: ppc_sem_mtspr(s, opc); return s.fx;

    // FPU indexed load
    case 535: ppc_sem_lfsx(s, opc); return s.fx;
    case 567: ppc_sem_lfsux(s, opc); return s.fx;
    case 599: ppc_sem_lfdx(s, opc); return s.fx;
    case 631: ppc_sem_lfdux(s, opc); return s.fx;

    // FPU indexed store
    case 663: ppc_sem_stfsx(s, opc); return s.fx;
    case 695: ppc_sem_stfsux(s, opc); return s.fx;
    case 727: ppc_sem_stfdx(s, opc); return s.fx;
    case 759: ppc_sem_stfdux(s, opc); return s.fx;

    default: return InsnEffect::everything();
    }
}

// Analyze any PPC instruction, return its read/write effects
static inline InsnEffect ppc_analyze_insn(uint32 opc)
{
    LivenessSemantics s;
    uint32 mainopc = PPC_OPC_MAIN(opc);
    switch (mainopc) {
    // Subtract immediate
    case 8: ppc_sem_subfic(s, opc); return s.fx;

    // Compare immediate
    case 10: ppc_sem_cmpli(s, opc); return s.fx;
    case 11: ppc_sem_cmpi(s, opc); return s.fx;

    // Add immediate
    case 12: ppc_sem_addic(s, opc); return s.fx;
    case 13: ppc_sem_addic_(s, opc); return s.fx;
    case 14: ppc_sem_addi(s, opc); return s.fx;
    case 15: ppc_sem_addis(s, opc); return s.fx;

    // Group 1: CR logical + branches
    case 19: return ppc_analyze_group1(opc);

    // Rotate and mask
    case 20: ppc_sem_rlwimix(s, opc); return s.fx;
    case 21: ppc_sem_rlwinmx(s, opc); return s.fx;
    case 23: ppc_sem_rlwnmx(s, opc); return s.fx;

    // Logic immediate
    case 24: ppc_sem_ori(s, opc); return s.fx;
    case 25: ppc_sem_oris(s, opc); return s.fx;
    case 26: ppc_sem_xori(s, opc); return s.fx;
    case 27: ppc_sem_xoris(s, opc); return s.fx;
    case 28: ppc_sem_andi_(s, opc); return s.fx;
    case 29: ppc_sem_andis_(s, opc); return s.fx;

    // Group 2: ALU register-register
    case 31: return ppc_analyze_group2(opc);

    // Multiply immediate
    case 7: ppc_sem_mulli(s, opc); return s.fx;

    // Branch
    case 16: ppc_sem_bcx(s, opc); return s.fx;
    case 18: ppc_sem_bx(s, opc); return s.fx;

    // Load word
    case 32: ppc_sem_lwz(s, opc); return s.fx;
    case 33: ppc_sem_lwzu(s, opc); return s.fx;

    // Load byte
    case 34: ppc_sem_lbz(s, opc); return s.fx;
    case 35: ppc_sem_lbzu(s, opc); return s.fx;

    // Store word
    case 36: ppc_sem_stw(s, opc); return s.fx;
    case 37: ppc_sem_stwu(s, opc); return s.fx;

    // Store byte
    case 38: ppc_sem_stb(s, opc); return s.fx;
    case 39: ppc_sem_stbu(s, opc); return s.fx;

    // Load half word
    case 40: ppc_sem_lhz(s, opc); return s.fx;
    case 41: ppc_sem_lhzu(s, opc); return s.fx;
    case 42: ppc_sem_lha(s, opc); return s.fx;
    case 43: ppc_sem_lhau(s, opc); return s.fx;

    // Store half word
    case 44: ppc_sem_sth(s, opc); return s.fx;
    case 45: ppc_sem_sthu(s, opc); return s.fx;

    // FPU load
    case 48: ppc_sem_lfs(s, opc); return s.fx;
    case 49: ppc_sem_lfsu(s, opc); return s.fx;
    case 50: ppc_sem_lfd(s, opc); return s.fx;
    case 51: ppc_sem_lfdu(s, opc); return s.fx;

    // FPU store
    case 52: ppc_sem_stfs(s, opc); return s.fx;
    case 53: ppc_sem_stfsu(s, opc); return s.fx;
    case 54: ppc_sem_stfd(s, opc); return s.fx;
    case 55: ppc_sem_stfdu(s, opc); return s.fx;

    // FPU arithmetic (group 59: single-precision)
    case 59: ppc_sem_fpu_arith(s, opc); return s.fx;

    // FPU (group 63: double-precision + fcmp + FPSCR ops)
    case 63: {
        uint32 ext = PPC_OPC_EXT(opc);
        switch (ext) {
        case 0:  // fcmpu
        case 32: // fcmpo
            ppc_sem_fcmpx(s, opc);
            return s.fx;
        default: ppc_sem_fpu_arith(s, opc); return s.fx;
        }
    }

    default: return InsnEffect::everything();
    }
}

#endif
