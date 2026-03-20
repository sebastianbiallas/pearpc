/*
 *  PearPC
 *  ppc_alu.cc
 *
 *  Copyright (C) 2003-2006 Sebastian Biallas (sb@biallas.net)
 *  Copyright (C) 2004 Daniel Foesch (dfoesch@cs.nmsu.edu)
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

#include <cstdlib>
#include <pthread.h>

#include "debug/tracers.h"
#include "ppc_cpu.h"
#include "ppc_dec.h"
#include "ppc_exc.h"
#include "ppc_mmu.h"
#include "ppc_opc.h"
#include "ppc_tools.h"
#include "tools/snprintf.h"

#include "jitc.h"
#include "jitc_asm.h"
#include "aarch64asm.h"
#include "io/graphic/gcard.h"

/*
 *  ========================================================
 *  Native AArch64 code generation for PPC ALU opcodes
 *  ========================================================
 *
 *  These replace the naive GEN_INTERPRET() calls with actual
 *  AArch64 instructions emitted into the translation cache.
 *
 *  Convention:
 *    X20 = pointer to PPC_CPU_State
 *    W16, W17 = scratch registers (IP0/IP1)
 *    PPC GPRs accessed via [X20, #offsetof(gpr[n])]
 */

#define GPR_OFS(n) (offsetof(PPC_CPU_State, gpr) + (n) * 4)

/* Precompute PPC rotate-and-mask for rlwinm/rlwnm at JIT time */
static inline uint32 ppc_mask(int MB, int ME)
{
    uint32 mask;
    if (MB <= ME) {
        if (ME - MB == 31) {
            mask = 0xffffffff;
        } else {
            mask = ((1 << (ME - MB + 1)) - 1) << (31 - ME);
        }
    } else {
        mask = ppc_word_rotl((1 << (32 - MB + ME + 1)) - 1, 31 - ME);
    }
    return mask;
}

/*
 *  Emit a privilege check: if MSR_PR is set, jump to
 *  ppc_program_exception_asm (which never returns).
 *  Uses checkedPriviledge to skip redundant checks within
 *  the same block (same as x86 JIT).
 */
static void ppc_opc_gen_check_privilege(JITC &jitc)
{
    if (!jitc.checkedPriviledge) {
        jitc.clobberAll();
        // Load MSR, test MSR_PR (bit 14)
        jitc.asmLDRw_cpu(W0, offsetof(PPC_CPU_State, msr));
        // TST W0, #(1<<14) = ANDS WZR, W0, #0x4000 (MSR_PR)
        // Logical immediate encoding for (1<<14): immr=18, imms=0
        jitc.asmTSTw(W0, 18, 0);

        // Precompute body size: MOV(pc) + MOV(PRIV) + BL(exception)
        uint body = a64_movw_size(jitc.pc)
                  + a64_movw_size(PPC_EXC_PROGRAM_PRIV)
                  + a64_bl_size((uint64)ppc_program_exception_asm);
        jitc.emitAssure(4 + body);
        NativeAddress target = jitc.asmHERE() + 4 + body;
        jitc.asmBccForward(A64_EQ, body); // B.EQ skip (not user mode)

        // User mode: raise privilege exception
        jitc.asmMOV(W0, jitc.pc);
        jitc.asmMOV(W1, PPC_EXC_PROGRAM_PRIV);
        jitc.asmCALL((NativeAddress)ppc_program_exception_asm);
        // ppc_program_exception_asm does not return

        jitc.asmAssertHERE(target, "check_privilege");
        jitc.checkedPriviledge = true;
    }
}

/*
 *  Native AArch64 codegen for ALU opcodes.
 *
 *  Helper: emit inline CR field update for signed compare.
 *  W16 = value a, W17 = value b.  Clobbers W0, W1, W2.
 *  Sets CR field crfD = (7 - field_number) to: LT(8) GT(4) EQ(2) | SO(1)
 */
static void gen_cmp_cr_update(JITC &jitc, int crfD)
{
    // CMP W16, W17  (sets NZCV)
    jitc.asmCMPw(W16, W17);

    // Build CR nibble in W0:
    //   LT → 8, GT → 4, EQ → 2, then OR in SO from XER
    // Use conditional select chain:
    //   W0 = (LT) ? 8 : ((GT) ? 4 : 2)
    jitc.asmMOV(W0, (uint32)8);      // LT value
    jitc.asmMOV(W1, (uint32)4);      // GT value
    jitc.asmMOV(W2, (uint32)2);      // EQ value

    // CSEL Wd, Wn, Wm, cond = 0x1A800000 | (Wm<<16) | (cond<<12) | (Wn<<5) | Wd
    // CSEL W1, W1, W2, GT → W1 = (signed GT) ? 4 : 2
    jitc.asmCSELw(W1, W1, W2, A64_GT);
    // CSEL W0, W0, W1, LT → W0 = (signed LT) ? 8 : W1
    jitc.asmCSELw(W0, W0, W1, A64_LT);

    // OR in SO bit from XER: if (xer & XER_SO) c |= 1
    jitc.asmLDRw_cpu(W1, offsetof(PPC_CPU_State, xer));
    // TST W1, #(1<<31) — XER_SO is bit 31
    jitc.asmTSTw(W1, 1, 0); // immr=1, imms=0 encodes bit 31
    // CSINC W0, W0, W0, EQ → W0 = (Z==1) ? W0 : W0+1  (i.e. if SO set, W0++)
    // CSINC Wd, Wn, Wm, cond = 0x1A800400 | (Wm<<16) | (cond<<12) | (Wn<<5) | Wd
    jitc.asmCSINCw(W0, W0, W0, A64_EQ);

    // Insert 4-bit CR field using BFI
    int cr_field = 7 - crfD;
    int cr_shift = cr_field * 4;
    jitc.asmLDRw_cpu(W1, offsetof(PPC_CPU_State, cr));
    jitc.asmBFIw(W1, W0, cr_shift, 4);
    jitc.asmSTRw_cpu(W1, offsetof(PPC_CPU_State, cr));
}

/*
 *  Same as gen_cmp_cr_update but for unsigned comparison.
 */
static void gen_cmpl_cr_update(JITC &jitc, int crfD)
{
    // CMP W16, W17 (unsigned — same instruction, different condition codes)
    jitc.asmCMPw(W16, W17);

    jitc.asmMOV(W0, (uint32)8);      // LT value
    jitc.asmMOV(W1, (uint32)4);      // GT value
    jitc.asmMOV(W2, (uint32)2);      // EQ value

    // CSEL W1, W1, W2, HI → W1 = (unsigned >) ? 4 : 2
    jitc.asmCSELw(W1, W1, W2, A64_HI);
    // CSEL W0, W0, W1, CC → W0 = (unsigned <) ? 8 : W1
    jitc.asmCSELw(W0, W0, W1, A64_CC);

    // OR in SO
    jitc.asmLDRw_cpu(W1, offsetof(PPC_CPU_State, xer));
    jitc.asmTSTw(W1, 1, 0);
    jitc.asmCSINCw(W0, W0, W0, A64_EQ);

    // Insert 4-bit CR field using BFI
    int cr_field = 7 - crfD;
    int cr_shift = cr_field * 4;
    jitc.asmLDRw_cpu(W1, offsetof(PPC_CPU_State, cr));
    jitc.asmBFIw(W1, W0, cr_shift, 4);
    jitc.asmSTRw_cpu(W1, offsetof(PPC_CPU_State, cr));
}

/*
 *  Helper: emit CR0 update for result in W16.
 *  CR0 = bits [31:28] of cr register.
 *  LT(8) if result < 0, GT(4) if > 0, EQ(2) if == 0, SO(1) from XER.
 */
static void gen_update_cr0(JITC &jitc)
{
    // CMP W16, #0 (result must be in W16 before calling this)
    jitc.asmCMPw(W16, (uint32)0);

    // Build 4-bit CR0 field: {LT, GT, EQ, SO}
    jitc.asmMOV(W0, (uint32)8);     // LT value
    jitc.asmMOV(W1, (uint32)4);     // GT value
    jitc.asmMOV(W2, (uint32)2);     // EQ value
    jitc.asmCSELw(W1, W1, W2, A64_GT);
    jitc.asmCSELw(W0, W0, W1, A64_LT);

    // OR in SO bit from XER
    jitc.asmLDRw_cpu(W1, offsetof(PPC_CPU_State, xer));
    jitc.asmTSTw(W1, 1, 0);         // TST W1, #0x80000000
    // CSINC W0, W0, W0, EQ → if SO set (NE), W0 = W0+1
    jitc.asmCSINCw(W0, W0, W0, A64_EQ);

    // Insert CR0 field (bits 31:28) using BFI
    jitc.asmLDRw_cpu(W1, offsetof(PPC_CPU_State, cr));
    jitc.asmBFIw(W1, W0, 28, 4);
    jitc.asmSTRw_cpu(W1, offsetof(PPC_CPU_State, cr));
}

/*
 *  addi rD, rA, SIMM  (opcode 14)
 *  if rA == 0: rD = SIMM
 *  else:       rD = gpr[rA] + SIMM
 */
JITCFlow ppc_opc_gen_addi(JITC &jitc)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
    sint32 simm = (sint32)imm;

    if (rA == 0) {
        jitc.asmMOV(W16, (uint32)simm);
        jitc.asmSTRw_cpu(W16, GPR_OFS(rD));
    } else {
        jitc.asmLDRw_cpu(W16, GPR_OFS(rA));
        if (simm >= 0 && simm < 4096) {
            jitc.asmADDw(W16, W16, (uint32)simm);
        } else if (simm < 0 && (-simm) < 4096) {
            jitc.asmSUBw(W16, W16, (uint32)(-simm));
        } else {
            jitc.asmMOV(W17, (uint32)simm);
            jitc.asmADDw(W16, W16, W17);
        }
        jitc.asmSTRw_cpu(W16, GPR_OFS(rD));
    }
    return flowContinue;
}

/*
 *  addis rD, rA, SIMM  (opcode 15)
 *  if rA == 0: rD = SIMM << 16
 *  else:       rD = gpr[rA] + (SIMM << 16)
 */
JITCFlow ppc_opc_gen_addis(JITC &jitc)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
    uint32 shifted = imm << 16;

    if (rA == 0) {
        jitc.asmMOV(W16, shifted);
        jitc.asmSTRw_cpu(W16, GPR_OFS(rD));
    } else {
        jitc.asmLDRw_cpu(W16, GPR_OFS(rA));
        jitc.asmMOV(W17, shifted);
        jitc.asmADDw(W16, W16, W17);
        jitc.asmSTRw_cpu(W16, GPR_OFS(rD));
    }
    return flowContinue;
}

/*
 *  ori rA, rS, UIMM  (opcode 24)
 *  rA = gpr[rS] | UIMM
 */
JITCFlow ppc_opc_gen_ori(JITC &jitc)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_UImm(jitc.current_opc, rS, rA, imm);

    if (imm == 0 && rS == rA) {
        return flowContinue;
    }
    jitc.asmLDRw_cpu(W16, GPR_OFS(rS));
    if (imm != 0) {
        jitc.asmMOV(W17, imm);
        jitc.asmORRw(W16, W16, W17);
    }
    jitc.asmSTRw_cpu(W16, GPR_OFS(rA));
    return flowContinue;
}

/*
 *  cmpi crfD, rA, SIMM  (opcode 11)
 *  Signed compare, set CR field.
 */
JITCFlow ppc_opc_gen_cmpi(JITC &jitc)
{
    int crfD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(jitc.current_opc, crfD, rA, imm);
    crfD >>= 2;

    jitc.asmLDRw_cpu(W16, GPR_OFS(rA));
    jitc.asmMOV(W17, (uint32)imm);
    gen_cmp_cr_update(jitc, crfD);
    return flowContinue;
}

/*
 *  b/bl target  (opcode 18)
 *  Unconditional branch, optionally sets LR.
 *  Generate native code that computes target and jumps.
 */
JITCFlow ppc_opc_gen_bx(JITC &jitc)
{
    uint32 li = jitc.current_opc & 0x03FFFFFC;
    if (li & 0x02000000) {
        li |= 0xFC000000; // sign extend
    }
    bool lk = jitc.current_opc & 1;
    bool aa = jitc.current_opc & 2;

    if (lk) {
        // BL: set LR = current_code_base + pc + 4
        jitc.asmLDRw_cpu(W16, offsetof(PPC_CPU_State, current_code_base));
        jitc.asmMOV(W17, jitc.pc + 4);
        jitc.asmADDw(W16, W16, W17);
        jitc.asmSTRw_cpu(W16, offsetof(PPC_CPU_State, lr));
    }

    uint32 target;
    if (aa) {
        target = li;
    } else {
        // PC-relative: we need current_code_base + pc + li at runtime
        // But current_code_base + pc is the current EA
        // Emit: W0 = current_code_base + pc + li
        jitc.asmLDRw_cpu(W0, offsetof(PPC_CPU_State, current_code_base));
        jitc.asmMOV(W17, jitc.pc + li);
        jitc.asmADDw(W0, W0, W17);
        // Jump to ppc_new_pc_asm (W0 = new effective PC)
        jitc.asmCALL((NativeAddress)ppc_new_pc_asm);
        return flowEndBlockUnreachable;
    }

    // Absolute address
    jitc.asmMOV(W0, target);
    jitc.asmCALL((NativeAddress)ppc_new_pc_asm);
    return flowEndBlockUnreachable;
}

/*
 *  bcx - Branch Conditional  (opcode 16)
 *
 *  Handles the common case (BO & 4: condition-only, no CTR decrement)
 *  by testing the CR bit inline and emitting a conditional skip.
 *  Falls back to interpreter for CTR-decrement variants and bcl.
 */
JITCFlow ppc_opc_gen_bcx(JITC &jitc)
{
    // Use the interpreter to evaluate the branch condition.
    // This correctly handles CTR decrement, all BO variants, and LK.
    // The optimization is in how we dispatch to the target afterwards.
    ppc_opc_gen_interpret(jitc, ppc_opc_bcx);

    // The interpreter set npc: branch target if taken, pc+4 if not taken.
    // Compare npc to pc+4 to detect the not-taken case and fall through.
    uint32 next_pc = jitc.pc + 4;
    uint call_size = a64_bl_size((uint64)ppc_new_pc_asm);
    //           LDR   MOV(next_pc)            CMP  B.EQ  CALL
    uint total = 4 + a64_movw_size(next_pc) + 4 + 4 + call_size;
    jitc.emitAssure(total);

    jitc.asmLDRw_cpu(W0, offsetof(PPC_CPU_State, npc));
    jitc.asmMOV(W1, next_pc);
    jitc.asmCMPw(W0, W1);
    NativeAddress target = jitc.asmHERE() + 4 + call_size;
    jitc.asmBccForward(A64_EQ, call_size); // B.EQ skip dispatch
    // Taken: full dispatch
    jitc.asmCALL((NativeAddress)ppc_new_pc_asm);
    // Not taken: land here, continue compiling next instruction
    jitc.asmAssertHERE(target, "bcx");
    return flowContinue;
}

/*
 *  Helper: emit dispatch for npc with same-page optimization.
 *  Loads npc from CPU state. If npc is on the current page
 *  (same current_code_base), uses the fast ppc_new_pc_this_page_asm
 *  lookup. Otherwise falls through to ppc_new_pc_asm.
 */
static void gen_dispatch_npc(JITC &jitc)
{
    // Load npc and dispatch via ppc_new_pc_asm.
    // Same approach as x86 JIT — no same-page fast path, no conditional branch.
    // This avoids conditional branch range overflow when fragments are far apart.
    jitc.asmLDRw_cpu(W0, offsetof(PPC_CPU_State, npc));
    jitc.asmCALL((NativeAddress)ppc_new_pc_asm);
}

/*
 *  bclrx - Branch Conditional to Link Register  (opcode 19, XO 16)
 *  bcctrx - Branch Conditional to Count Register  (opcode 19, XO 528)
 *
 *  Use interpreter for condition eval, then dispatch via same-page
 *  fast path when possible.
 */
JITCFlow ppc_opc_gen_bclrx(JITC &jitc)
{
    ppc_opc_gen_interpret(jitc, ppc_opc_bclrx);
    gen_dispatch_npc(jitc);
    return flowEndBlockUnreachable;
}

JITCFlow ppc_opc_gen_bcctrx(JITC &jitc)
{
    ppc_opc_gen_interpret(jitc, ppc_opc_bcctrx);
    gen_dispatch_npc(jitc);
    return flowEndBlockUnreachable;
}

/*
 *  oris rA, rS, UIMM  (opcode 25)
 */
JITCFlow ppc_opc_gen_oris(JITC &jitc)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_UImm(jitc.current_opc, rS, rA, imm);
    jitc.asmLDRw_cpu(W16, GPR_OFS(rS));
    if (imm) {
        jitc.asmMOV(W17, imm << 16);
        jitc.asmORRw(W16, W16, W17);
    }
    jitc.asmSTRw_cpu(W16, GPR_OFS(rA));
    return flowContinue;
}

/*
 *  xori rA, rS, UIMM  (opcode 26)
 */
JITCFlow ppc_opc_gen_xori(JITC &jitc)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_UImm(jitc.current_opc, rS, rA, imm);
    jitc.asmLDRw_cpu(W16, GPR_OFS(rS));
    if (imm) {
        jitc.asmMOV(W17, imm);
        jitc.asmEORw(W16, W16, W17);
    }
    jitc.asmSTRw_cpu(W16, GPR_OFS(rA));
    return flowContinue;
}

/*
 *  xoris rA, rS, UIMM  (opcode 27)
 */
JITCFlow ppc_opc_gen_xoris(JITC &jitc)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_UImm(jitc.current_opc, rS, rA, imm);
    jitc.asmLDRw_cpu(W16, GPR_OFS(rS));
    if (imm) {
        jitc.asmMOV(W17, imm << 16);
        jitc.asmEORw(W16, W16, W17);
    }
    jitc.asmSTRw_cpu(W16, GPR_OFS(rA));
    return flowContinue;
}

/*
 *  Register-register ALU helpers.
 *  Rc check: if Rc=1, fall back to interpreter for CR0 update.
 */
int ppc_opc_addx(PPC_CPU_State &);
int ppc_opc_subfx(PPC_CPU_State &);
int ppc_opc_andx(PPC_CPU_State &);
int ppc_opc_orx(PPC_CPU_State &);
int ppc_opc_xorx(PPC_CPU_State &);
int ppc_opc_negx(PPC_CPU_State &);
int ppc_opc_mullwx(PPC_CPU_State &);
int ppc_opc_slwx(PPC_CPU_State &);
int ppc_opc_srwx(PPC_CPU_State &);
int ppc_opc_rlwinmx(PPC_CPU_State &);
int ppc_opc_rlwimix(PPC_CPU_State &);
int ppc_opc_rlwnmx(PPC_CPU_State &);
int ppc_opc_mulhwux(PPC_CPU_State &);
int ppc_opc_subfic(PPC_CPU_State &);
int ppc_opc_addic_(PPC_CPU_State &);
int ppc_opc_srawx(PPC_CPU_State &);
int ppc_opc_orcx(PPC_CPU_State &);
int ppc_opc_norx(PPC_CPU_State &);
int ppc_opc_cntlzwx(PPC_CPU_State &);
int ppc_opc_divwux(PPC_CPU_State &);
int ppc_opc_divwx(PPC_CPU_State &);
int ppc_opc_addex(PPC_CPU_State &);
int ppc_opc_subfex(PPC_CPU_State &);
int ppc_opc_addcx(PPC_CPU_State &);
int ppc_opc_subfcx(PPC_CPU_State &);
int ppc_opc_srawix(PPC_CPU_State &);

#define RC_FALLBACK(interp_func) \
    if (jitc.current_opc & PPC_OPC_Rc) { \
        ppc_opc_gen_interpret(jitc, interp_func); \
        return flowContinue; \
    }

static JITCFlow gen_alu_reg(JITC &jitc, uint32 (*op)(int, int, int))
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
    jitc.asmLDRw_cpu(W16, GPR_OFS(rA));
    jitc.asmLDRw_cpu(W17, GPR_OFS(rB));
    jitc.emit32(op(16, 16, 17));
    jitc.asmSTRw_cpu(W16, GPR_OFS(rD));
    return flowContinue;
}

/* add rD, rA, rB */
JITCFlow ppc_opc_gen_addx(JITC &jitc)
{
    RC_FALLBACK(ppc_opc_addx);
    return gen_alu_reg(jitc, a64_ADDw_reg);
}
/* subf rD, rA, rB  (rD = rB - rA) */
JITCFlow ppc_opc_gen_subfx(JITC &jitc)
{
    RC_FALLBACK(ppc_opc_subfx);
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
    jitc.asmLDRw_cpu(W17, GPR_OFS(rA));
    jitc.asmLDRw_cpu(W16, GPR_OFS(rB));
    jitc.asmSUBw(W16, W16, W17);
    jitc.asmSTRw_cpu(W16, GPR_OFS(rD));
    return flowContinue;
}
/* and rA, rS, rB */
JITCFlow ppc_opc_gen_andx(JITC &jitc)
{
    RC_FALLBACK(ppc_opc_andx);
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
    jitc.asmLDRw_cpu(W16, GPR_OFS(rS));
    jitc.asmLDRw_cpu(W17, GPR_OFS(rB));
    jitc.asmANDw(W16, W16, W17);
    jitc.asmSTRw_cpu(W16, GPR_OFS(rA));
    return flowContinue;
}
/* or rA, rS, rB (also: mr rA, rS when rS == rB) */
JITCFlow ppc_opc_gen_orx(JITC &jitc)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
    if (rS == rB) {
        // mr rA, rS — just copy
        jitc.asmLDRw_cpu(W16, GPR_OFS(rS));
        jitc.asmSTRw_cpu(W16, GPR_OFS(rA));
    } else {
        jitc.asmLDRw_cpu(W16, GPR_OFS(rS));
        jitc.asmLDRw_cpu(W17, GPR_OFS(rB));
        jitc.asmORRw(W16, W16, W17);
        jitc.asmSTRw_cpu(W16, GPR_OFS(rA));
    }
    if (jitc.current_opc & PPC_OPC_Rc) {
        // Result is in W16 (either copied or OR'd)
        gen_update_cr0(jitc);
    }
    return flowContinue;
}
/* xor rA, rS, rB */
JITCFlow ppc_opc_gen_xorx(JITC &jitc)
{
    RC_FALLBACK(ppc_opc_xorx);
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
    jitc.asmLDRw_cpu(W16, GPR_OFS(rS));
    jitc.asmLDRw_cpu(W17, GPR_OFS(rB));
    jitc.asmEORw(W16, W16, W17);
    jitc.asmSTRw_cpu(W16, GPR_OFS(rA));
    return flowContinue;
}

/* neg rD, rA */
JITCFlow ppc_opc_gen_negx(JITC &jitc)
{
    RC_FALLBACK(ppc_opc_negx);
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
    (void)rB;
    jitc.asmLDRw_cpu(W16, GPR_OFS(rA));
    jitc.asmNEGw(W16, W16);
    jitc.asmSTRw_cpu(W16, GPR_OFS(rD));
    return flowContinue;
}

/* mullw rD, rA, rB */
JITCFlow ppc_opc_gen_mullwx(JITC &jitc)
{
    RC_FALLBACK(ppc_opc_mullwx);
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
    jitc.asmLDRw_cpu(W16, GPR_OFS(rA));
    jitc.asmLDRw_cpu(W17, GPR_OFS(rB));
    jitc.asmMULw(W16, W16, W17);
    jitc.asmSTRw_cpu(W16, GPR_OFS(rD));
    return flowContinue;
}

/*
 *  slw rA, rS, rB  (shift left word)
 *  shift = rB & 0x3F; rA = (shift >= 32) ? 0 : rS << shift
 *  Use 64-bit LSLV: zero-extended 32-bit value shifted left, low 32 bits = correct result.
 */
JITCFlow ppc_opc_gen_slwx(JITC &jitc)
{
    RC_FALLBACK(ppc_opc_slwx);
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
    jitc.asmLDRw_cpu(W16, GPR_OFS(rS));     // X16 = zero-extend(gpr[rS])
    jitc.asmLDRw_cpu(W17, GPR_OFS(rB));
    jitc.asmANDw_imm(W17, W17, 0, 5);       // AND W17, W17, #0x3F (6-bit shift)
    jitc.asmLSLV(X16, X16, X17);
    jitc.asmSTRw_cpu(W16, GPR_OFS(rA));      // store low 32 bits
    return flowContinue;
}

/*
 *  srw rA, rS, rB  (shift right word)
 *  shift = rB & 0x3F; rA = (shift >= 32) ? 0 : rS >> shift
 *  Use 64-bit LSRV: zero-extended 32-bit value shifted right, low 32 bits = correct result.
 */
JITCFlow ppc_opc_gen_srwx(JITC &jitc)
{
    RC_FALLBACK(ppc_opc_srwx);
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
    jitc.asmLDRw_cpu(W16, GPR_OFS(rS));     // X16 = zero-extend(gpr[rS])
    jitc.asmLDRw_cpu(W17, GPR_OFS(rB));
    jitc.asmANDw_imm(W17, W17, 0, 5);       // AND W17, W17, #0x3F (6-bit shift)
    jitc.asmLSRV(X16, X16, X17);
    jitc.asmSTRw_cpu(W16, GPR_OFS(rA));      // store low 32 bits
    return flowContinue;
}

/*
 *  rlwinm rA, rS, SH, MB, ME  (rotate left word then AND with mask)
 *  Mask and shift amount known at JIT time → precompute mask.
 */
JITCFlow ppc_opc_gen_rlwinmx(JITC &jitc)
{
    RC_FALLBACK(ppc_opc_rlwinmx);
    int rS, rA, SH;
    uint32 MB, ME;
    PPC_OPC_TEMPL_M(jitc.current_opc, rS, rA, SH, MB, ME);

    uint32 mask = ppc_mask(MB, ME);

    jitc.asmLDRw_cpu(W16, GPR_OFS(rS));
    if (SH != 0) {
        // PPC rotate left by SH = AArch64 ROR by (32 - SH)
        jitc.emit32(a64_RORw_imm(16, 16, (32 - SH) & 31));
    }
    if (mask == 0xFFFFFFFF) {
        // No masking needed
    } else {
        jitc.asmMOV(W17, mask);
        jitc.asmANDw(W16, W16, W17);
    }
    jitc.asmSTRw_cpu(W16, GPR_OFS(rA));
    return flowContinue;
}

/*
 *  rlwnm rA, rS, rB, MB, ME  (rotate left word then AND with mask, variable)
 */
JITCFlow ppc_opc_gen_rlwnmx(JITC &jitc)
{
    RC_FALLBACK(ppc_opc_rlwnmx);
    int rS, rA, rB, MB, ME;
    PPC_OPC_TEMPL_M(jitc.current_opc, rS, rA, rB, MB, ME);

    uint32 mask = ppc_mask(MB, ME);

    jitc.asmLDRw_cpu(W16, GPR_OFS(rS));
    jitc.asmLDRw_cpu(W17, GPR_OFS(rB));
    // PPC rotl by rB = negate rB for ROR
    jitc.asmNEGw(W17, W17);
    jitc.emit32(a64_RORw_reg(16, 16, 17));
    if (mask != 0xFFFFFFFF) {
        jitc.asmMOV(W17, mask);
        jitc.asmANDw(W16, W16, W17);
    }
    jitc.asmSTRw_cpu(W16, GPR_OFS(rA));
    return flowContinue;
}

/*
 *  cmp crfD, rA, rB  (signed register compare)
 */
JITCFlow ppc_opc_gen_cmp(JITC &jitc)
{
    uint32 cr;
    int rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, cr, rA, rB);
    cr >>= 2;
    jitc.asmLDRw_cpu(W16, GPR_OFS(rA));
    jitc.asmLDRw_cpu(W17, GPR_OFS(rB));
    gen_cmp_cr_update(jitc, cr);
    return flowContinue;
}

/*
 *  cmpl crfD, rA, rB  (unsigned register compare)
 */
JITCFlow ppc_opc_gen_cmpl(JITC &jitc)
{
    uint32 cr;
    int rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, cr, rA, rB);
    cr >>= 2;
    jitc.asmLDRw_cpu(W16, GPR_OFS(rA));
    jitc.asmLDRw_cpu(W17, GPR_OFS(rB));
    gen_cmpl_cr_update(jitc, cr);
    return flowContinue;
}

/*
 *  cmpli crfD, rA, UIMM  (unsigned immediate compare)
 */
JITCFlow ppc_opc_gen_cmpli(JITC &jitc)
{
    uint32 cr;
    int rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_UImm(jitc.current_opc, cr, rA, imm);
    cr >>= 2;
    jitc.asmLDRw_cpu(W16, GPR_OFS(rA));
    jitc.asmMOV(W17, imm);
    gen_cmpl_cr_update(jitc, cr);
    return flowContinue;
}

/*
 *  andi. rA, rS, UIMM — always updates CR0
 */
JITCFlow ppc_opc_gen_andi_(JITC &jitc)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_UImm(jitc.current_opc, rS, rA, imm);
    jitc.asmLDRw_cpu(W16, GPR_OFS(rS));
    jitc.asmMOV(W17, imm);
    jitc.asmANDw(W16, W16, W17);
    jitc.asmSTRw_cpu(W16, GPR_OFS(rA));
    gen_update_cr0(jitc);
    return flowContinue;
}

/* andis. rA, rS, UIMM — AND Immediate Shifted, always updates CR0 */
JITCFlow ppc_opc_gen_andis_(JITC &jitc)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_UImm(jitc.current_opc, rS, rA, imm);
    uint32 mask = imm << 16;
    jitc.asmLDRw_cpu(W16, GPR_OFS(rS));
    jitc.asmMOV(W17, mask);
    jitc.asmANDw(W16, W16, W17);
    jitc.asmSTRw_cpu(W16, GPR_OFS(rA));
    gen_update_cr0(jitc);
    return flowContinue;
}

/* mulli rD, rA, SIMM */
JITCFlow ppc_opc_gen_mulli(JITC &jitc)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
    jitc.asmLDRw_cpu(W16, GPR_OFS(rA));
    jitc.asmMOV(W17, imm);
    jitc.asmMULw(W16, W16, W17);
    jitc.asmSTRw_cpu(W16, GPR_OFS(rD));
    return flowContinue;
}

/* mulhwux rD, rA, rB — Multiply High Word Unsigned */
JITCFlow ppc_opc_gen_mulhwux(JITC &jitc)
{
    RC_FALLBACK(ppc_opc_mulhwux);
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(jitc.current_opc, rD, rA, rB);
    jitc.asmLDRw_cpu(W16, GPR_OFS(rA));
    jitc.asmLDRw_cpu(W17, GPR_OFS(rB));
    jitc.asmUMULL(X16, W16, W17);
    // LSR X16, X16, #32 to get high word (64-bit UBFM)
    jitc.emit32(0xD360FC00 | (X16 << 5) | W16);
    jitc.asmSTRw_cpu(W16, GPR_OFS(rD));
    return flowContinue;
}


/* rlwimix rA, rS, SH, MB, ME — Rotate Left Word Immediate then Mask Insert */
JITCFlow ppc_opc_gen_rlwimix(JITC &jitc)
{
    RC_FALLBACK(ppc_opc_rlwimix);
    int rS, rA, SH;
    uint32 MB, ME;
    PPC_OPC_TEMPL_M(jitc.current_opc, rS, rA, SH, MB, ME);
    uint32 mask = ppc_mask(MB, ME);
    // v = ROTL(gpr[rS], SH)
    jitc.asmLDRw_cpu(W16, GPR_OFS(rS));
    if (SH != 0) {
        jitc.emit32(a64_RORw_imm(16, 16, (32 - SH) & 31));
    }
    // rA = (v & mask) | (rA & ~mask)
    jitc.asmLDRw_cpu(W17, GPR_OFS(rA));
    if (mask == 0xFFFFFFFF) {
        jitc.asmSTRw_cpu(W16, GPR_OFS(rA));
    } else if (mask == 0) {
        // No bits inserted — rA unchanged
    } else {
        // W16 = rotated value, W17 = old rA
        // Use BFI-like approach: clear mask bits in rA, OR in masked rotated value
        NativeReg tmp = W0;
        jitc.asmMOV(tmp, mask);
        // W16 = v & mask
        jitc.asmANDw(W16, W16, tmp);
        // tmp = ~mask
        jitc.asmMOV(tmp, ~mask);
        // W17 = rA & ~mask
        jitc.asmANDw(W17, W17, tmp);
        // result = (v & mask) | (rA & ~mask)
        jitc.asmORRw(W16, W16, W17);
        jitc.asmSTRw_cpu(W16, GPR_OFS(rA));
    }
    return flowContinue;
}

/*
 *  === Carry-flag opcodes ===
 *  XER[CA] is stored in cpu->xer_ca (uint32, 0 or 1).
 *  AArch64 ADDS/SUBS set the C flag; CSET can extract it.
 *  PPC carry = unsigned overflow = AArch64 C flag after ADDS.
 */

#define XER_CA_OFS offsetof(PPC_CPU_State, xer_ca)

/* addic rD, rA, SIMM — Add Immediate Carrying */
JITCFlow ppc_opc_gen_addic(JITC &jitc)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
    jitc.asmLDRw_cpu(W16, GPR_OFS(rA));
    jitc.asmMOV(W17, imm);
    // ADDS W16, W16, W17 — sets C flag on unsigned overflow
    jitc.emit32(a64_ADDSw_reg(W16, W16, W17));
    jitc.asmSTRw_cpu(W16, GPR_OFS(rD));
    // CSET W17, CS (carry set) → xer_ca = C flag
    jitc.asmCSETw(W17, A64_CS);
    jitc.asmSTRw_cpu(W17, XER_CA_OFS);
    return flowContinue;
}

/* addic. rD, rA, SIMM — Add Immediate Carrying and Record (CR0) */
JITCFlow ppc_opc_gen_addic_(JITC &jitc)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
    jitc.asmLDRw_cpu(W16, GPR_OFS(rA));
    jitc.asmMOV(W17, imm);
    jitc.emit32(a64_ADDSw_reg(W16, W16, W17));
    jitc.asmSTRw_cpu(W16, GPR_OFS(rD));
    jitc.asmCSETw(W17, A64_CS);
    jitc.asmSTRw_cpu(W17, XER_CA_OFS);
    gen_update_cr0(jitc);
    return flowContinue;
}

/* subfic rD, rA, SIMM — Subtract From Immediate Carrying
 * rD = ~rA + SIMM + 1; CA = carry_3(~rA, SIMM, 1)
 * Equivalent to: rD = SIMM - rA; CA = (SIMM >= rA) for unsigned
 * But the PPC definition uses ~a + imm + 1, so we use SUBS and invert carry sense.
 * Actually: ~a + imm + 1 = imm - a. Carry out = (imm >= a) in unsigned = no borrow.
 * AArch64 SUBS sets C=1 when no borrow (a <= imm). So CA = C flag after SUBS Wd, Wimm, Wa.
 */
JITCFlow ppc_opc_gen_subfic(JITC &jitc)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
    jitc.asmLDRw_cpu(W16, GPR_OFS(rA));
    jitc.asmMOV(W17, imm);
    // SUBS W16, W17, W16 → W16 = imm - rA, C = no borrow = (imm >= rA)
    jitc.emit32(a64_SUBSw_reg(W16, W17, W16));
    jitc.asmSTRw_cpu(W16, GPR_OFS(rD));
    // CSET W17, CS → xer_ca
    jitc.asmCSETw(W17, A64_CS);
    jitc.asmSTRw_cpu(W17, XER_CA_OFS);
    return flowContinue;
}

/* addex rD, rA, rB — Add Extended (with carry in/out)
 * rD = rA + rB + CA; new CA = carry_3(rA, rB, CA)
 */
JITCFlow ppc_opc_gen_addex(JITC &jitc)
{
    RC_FALLBACK(ppc_opc_addex);
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(jitc.current_opc, rD, rA, rB);
    jitc.asmLDRw_cpu(W16, GPR_OFS(rA));
    jitc.asmLDRw_cpu(W17, GPR_OFS(rB));
    jitc.asmLDRw_cpu(W0, XER_CA_OFS);
    // ADDS W16, W16, W17 → partial sum, C1
    jitc.emit32(a64_ADDSw_reg(W16, W16, W17));
    // CSET W1, CS → save C1
    jitc.asmCSETw(W1, A64_CS);
    // ADDS W16, W16, W0 → add carry-in, C2
    jitc.emit32(a64_ADDSw_reg(W16, W16, W0));
    // CSET W0, CS → C2
    jitc.asmCSETw(W0, A64_CS);
    // CA = C1 | C2
    jitc.asmORRw(W0, W0, W1);
    jitc.asmSTRw_cpu(W16, GPR_OFS(rD));
    jitc.asmSTRw_cpu(W0, XER_CA_OFS);
    return flowContinue;
}

/* subfex rD, rA, rB — Subtract From Extended
 * rD = ~rA + rB + CA; new CA = carry_3(~rA, rB, CA)
 * Equivalent to: rD = rB - rA - 1 + CA = rB - rA + (CA - 1)
 * When CA=1: rD = rB - rA (normal subtract, CA_out = no borrow)
 * When CA=0: rD = rB - rA - 1 (subtract with borrow)
 */
JITCFlow ppc_opc_gen_subfex(JITC &jitc)
{
    RC_FALLBACK(ppc_opc_subfex);
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(jitc.current_opc, rD, rA, rB);
    // Use the same approach as addex but with ~rA
    jitc.asmLDRw_cpu(W16, GPR_OFS(rA));
    jitc.asmMVNw(W16, W16);
    jitc.asmLDRw_cpu(W17, GPR_OFS(rB));
    jitc.asmLDRw_cpu(W0, XER_CA_OFS);
    // ADDS W16, W16, W17
    jitc.emit32(a64_ADDSw_reg(W16, W16, W17));
    jitc.asmCSETw(W1, A64_CS);
    // ADDS W16, W16, W0
    jitc.emit32(a64_ADDSw_reg(W16, W16, W0));
    jitc.asmCSETw(W0, A64_CS);
    jitc.asmORRw(W0, W0, W1);
    jitc.asmSTRw_cpu(W16, GPR_OFS(rD));
    jitc.asmSTRw_cpu(W0, XER_CA_OFS);
    return flowContinue;
}

/* addcx rD, rA, rB — Add Carrying (no carry in, carry out)
 * rD = rA + rB; CA = unsigned overflow
 */
JITCFlow ppc_opc_gen_addcx(JITC &jitc)
{
    RC_FALLBACK(ppc_opc_addcx);
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(jitc.current_opc, rD, rA, rB);
    jitc.asmLDRw_cpu(W16, GPR_OFS(rA));
    jitc.asmLDRw_cpu(W17, GPR_OFS(rB));
    jitc.emit32(a64_ADDSw_reg(W16, W16, W17));
    jitc.asmSTRw_cpu(W16, GPR_OFS(rD));
    jitc.asmCSETw(W17, A64_CS);
    jitc.asmSTRw_cpu(W17, XER_CA_OFS);
    return flowContinue;
}

/* subfcx rD, rA, rB — Subtract From Carrying
 * rD = ~rA + rB + 1 = rB - rA; CA = (rB >= rA) unsigned
 */
JITCFlow ppc_opc_gen_subfcx(JITC &jitc)
{
    RC_FALLBACK(ppc_opc_subfcx);
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(jitc.current_opc, rD, rA, rB);
    jitc.asmLDRw_cpu(W16, GPR_OFS(rB));
    jitc.asmLDRw_cpu(W17, GPR_OFS(rA));
    // SUBS W16, W16, W17 → rB - rA, C = no borrow
    jitc.emit32(a64_SUBSw_reg(W16, W16, W17));
    jitc.asmSTRw_cpu(W16, GPR_OFS(rD));
    jitc.asmCSETw(W17, A64_CS);
    jitc.asmSTRw_cpu(W17, XER_CA_OFS);
    return flowContinue;
}

/* srawix rA, rS, SH — Shift Right Algebraic Word Immediate
 * rA = ASR(rS, SH); CA = (rS < 0) && (shifted-out bits != 0)
 */
JITCFlow ppc_opc_gen_srawix(JITC &jitc)
{
    RC_FALLBACK(ppc_opc_srawix);
    int rS, rA;
    uint32 SH;
    PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, SH);
    jitc.asmLDRw_cpu(W16, GPR_OFS(rS));
    if (SH == 0) {
        // No shift — result = rS, CA = 0
        jitc.asmSTRw_cpu(W16, GPR_OFS(rA));
        jitc.asmMOV(W17, (uint32)0);
        jitc.asmSTRw_cpu(W17, XER_CA_OFS);
    } else {
        // ASR W17, W16, #SH
        jitc.emit32(a64_ASRw_imm(W17, W16, SH));
        jitc.asmSTRw_cpu(W17, GPR_OFS(rA));
        // CA = (rS < 0) && (rS & ((1<<SH)-1) != 0)
        // Test shifted-out bits: AND W0, W16, #mask
        uint32 mask = (1u << SH) - 1;
        jitc.asmMOV(W0, mask);
        jitc.asmANDw(W0, W16, W0);
        // W0 != 0 if any shifted-out bits were set
        // Also need rS < 0 (bit 31 set)
        // CA = (W16 >> 31) & (W0 != 0)
        // Use: CMP W0, #0; CSET W0, NE → W0 = (shifted-out != 0)
        jitc.asmCMPw(W0, (uint32)0);
        jitc.asmCSETw(W0, A64_NE);
        // TST W16, #0x80000000 (sign bit)
        jitc.asmTSTw(W16, 1, 0);
        // CSEL W0, W0, WZR, NE → keep W0 if negative, else 0
        jitc.asmCSELw(W0, W0, WZR, A64_NE);
        jitc.asmSTRw_cpu(W0, XER_CA_OFS);
    }
    return flowContinue;
}

/* mfcr rD — Move From Condition Register */
JITCFlow ppc_opc_gen_mfcr(JITC &jitc)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
    jitc.asmLDRw_cpu(W16, offsetof(PPC_CPU_State, cr));
    jitc.asmSTRw_cpu(W16, GPR_OFS(rD));
    return flowContinue;
}

/* mtcrf CRM, rS — Move To Condition Register Fields */
JITCFlow ppc_opc_gen_mtcrf(JITC &jitc)
{
    int rS;
    uint32 crm;
    PPC_OPC_TEMPL_XFX(jitc.current_opc, rS, crm);
    // Build CRM mask: each bit in crm selects a 4-bit CR field
    uint32 CRM = ((crm & 0x80) ? 0xf0000000 : 0) | ((crm & 0x40) ? 0x0f000000 : 0) |
                 ((crm & 0x20) ? 0x00f00000 : 0) | ((crm & 0x10) ? 0x000f0000 : 0) |
                 ((crm & 0x08) ? 0x0000f000 : 0) | ((crm & 0x04) ? 0x00000f00 : 0) |
                 ((crm & 0x02) ? 0x000000f0 : 0) | ((crm & 0x01) ? 0x0000000f : 0);
    if (CRM == 0xFFFFFFFF) {
        // All fields — just copy
        jitc.asmLDRw_cpu(W16, GPR_OFS(rS));
        jitc.asmSTRw_cpu(W16, offsetof(PPC_CPU_State, cr));
    } else {
        // cr = (rS & CRM) | (cr & ~CRM)
        jitc.asmLDRw_cpu(W16, GPR_OFS(rS));
        jitc.asmLDRw_cpu(W17, offsetof(PPC_CPU_State, cr));
        jitc.asmMOV(W0, CRM);
        jitc.asmANDw(W16, W16, W0);
        jitc.asmMOV(W0, ~CRM);
        jitc.asmANDw(W17, W17, W0);
        jitc.asmORRw(W16, W16, W17);
        jitc.asmSTRw_cpu(W16, offsetof(PPC_CPU_State, cr));
    }
    return flowContinue;
}

/* srawx rA, rS, rB — Shift Right Algebraic Word (sets XER[CA]) */
JITCFlow ppc_opc_gen_srawx(JITC &jitc)
{
    ppc_opc_gen_interpret(jitc, ppc_opc_srawx);
    return flowContinue;
}

/* divwux rD, rA, rB — Divide Word Unsigned
 * AArch64 UDIV returns 0 on division by zero (no trap), matching PPC behavior.
 */
JITCFlow ppc_opc_gen_divwux(JITC &jitc)
{
    RC_FALLBACK(ppc_opc_divwux);
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(jitc.current_opc, rD, rA, rB);
    jitc.asmLDRw_cpu(W16, GPR_OFS(rA));
    jitc.asmLDRw_cpu(W17, GPR_OFS(rB));
    jitc.asmUDIVw(W16, W16, W17);
    jitc.asmSTRw_cpu(W16, GPR_OFS(rD));
    return flowContinue;
}

/* divwx rD, rA, rB — Divide Word Signed
 * AArch64 SDIV returns 0 on division by zero, matching PPC behavior.
 */
JITCFlow ppc_opc_gen_divwx(JITC &jitc)
{
    RC_FALLBACK(ppc_opc_divwx);
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(jitc.current_opc, rD, rA, rB);
    jitc.asmLDRw_cpu(W16, GPR_OFS(rA));
    jitc.asmLDRw_cpu(W17, GPR_OFS(rB));
    jitc.asmSDIVw(W16, W16, W17);
    jitc.asmSTRw_cpu(W16, GPR_OFS(rD));
    return flowContinue;
}

/* orcx rA, rS, rB — OR with Complement */
JITCFlow ppc_opc_gen_orcx(JITC &jitc)
{
    RC_FALLBACK(ppc_opc_orcx);
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
    jitc.asmLDRw_cpu(W16, GPR_OFS(rS));
    jitc.asmLDRw_cpu(W17, GPR_OFS(rB));
    jitc.asmORNw(W16, W16, W17);
    jitc.asmSTRw_cpu(W16, GPR_OFS(rA));
    return flowContinue;
}

/* norx rA, rS, rB — NOR */
JITCFlow ppc_opc_gen_norx(JITC &jitc)
{
    RC_FALLBACK(ppc_opc_norx);
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
    jitc.asmLDRw_cpu(W16, GPR_OFS(rS));
    jitc.asmLDRw_cpu(W17, GPR_OFS(rB));
    jitc.asmORRw(W16, W16, W17);
    jitc.asmMVNw(W16, W16);
    jitc.asmSTRw_cpu(W16, GPR_OFS(rA));
    return flowContinue;
}

/* cntlzwx rA, rS — Count Leading Zeros Word */
JITCFlow ppc_opc_gen_cntlzwx(JITC &jitc)
{
    RC_FALLBACK(ppc_opc_cntlzwx);
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
    jitc.asmLDRw_cpu(W16, GPR_OFS(rS));
    jitc.asmCLZw(W16, W16);
    jitc.asmSTRw_cpu(W16, GPR_OFS(rA));
    return flowContinue;
}

/*
 *  mtspr SPR, rS — Move To Special Purpose Register
 *  Handle LR/CTR natively, fall back to interpreter for others.
 */
JITCFlow ppc_opc_gen_mtspr(JITC &jitc)
{
    int rS, spr1, spr2;
    PPC_OPC_TEMPL_X(jitc.current_opc, rS, spr1, spr2);

    if (spr2 == 0) {
        if (spr1 == 8) {
            // mtlr rS
            jitc.asmLDRw_cpu(W16, GPR_OFS(rS));
            jitc.asmSTRw_cpu(W16, offsetof(PPC_CPU_State, lr));
            return flowContinue;
        }
        if (spr1 == 9) {
            // mtctr rS
            jitc.asmLDRw_cpu(W16, GPR_OFS(rS));
            jitc.asmSTRw_cpu(W16, offsetof(PPC_CPU_State, ctr));
            return flowContinue;
        }
    }
    // All other SPRs: fall back to interpreter
    ppc_opc_gen_interpret(jitc, ppc_opc_mtspr);
    return flowContinue;
}

/*
 *  mfspr rD, SPR — Move From Special Purpose Register
 *  Handle LR/CTR natively, fall back to interpreter for others.
 */
JITCFlow ppc_opc_gen_mfspr(JITC &jitc)
{
    int rD, spr1, spr2;
    PPC_OPC_TEMPL_XO(jitc.current_opc, rD, spr1, spr2);

    if (spr2 == 0) {
        if (spr1 == 8) {
            // mflr rD
            jitc.asmLDRw_cpu(W16, offsetof(PPC_CPU_State, lr));
            jitc.asmSTRw_cpu(W16, GPR_OFS(rD));
            return flowContinue;
        }
        if (spr1 == 9) {
            // mfctr rD
            jitc.asmLDRw_cpu(W16, offsetof(PPC_CPU_State, ctr));
            jitc.asmSTRw_cpu(W16, GPR_OFS(rD));
            return flowContinue;
        }
    }
    // All other SPRs: fall back to interpreter
    ppc_opc_gen_interpret(jitc, ppc_opc_mfspr);
    return flowContinue;
}

// Forward declarations for functions defined in ppc_opc.cc
extern void ppc_set_msr(PPC_CPU_State &aCPU, uint32 newmsr);
extern void FASTCALL writeDEC(PPC_CPU_State &aCPU, uint32 newdec);
extern void readDEC(PPC_CPU_State &aCPU);
extern void FASTCALL writeTBL(PPC_CPU_State &aCPU, uint32 val);
extern void FASTCALL writeTBU(PPC_CPU_State &aCPU, uint32 val);

static inline void ppc_opc_batu_helper(PPC_CPU_State &aCPU, bool dbat, int idx)
{
    if (dbat) {
        aCPU.dbat_bl[idx] = ((~aCPU.dbatu[idx] << 15) & 0xfffe0000);
        aCPU.dbat_nbl[idx] = ~aCPU.dbat_bl[idx];
        aCPU.dbat_bepi[idx] = (aCPU.dbatu[idx] & aCPU.dbat_bl[idx]);
    } else {
        aCPU.ibat_bl[idx] = ((~aCPU.ibatu[idx] << 15) & 0xfffe0000);
        aCPU.ibat_nbl[idx] = ~aCPU.ibat_bl[idx];
        aCPU.ibat_bepi[idx] = (aCPU.ibatu[idx] & aCPU.ibat_bl[idx]);
    }
}

static inline void ppc_opc_batl_helper(PPC_CPU_State &aCPU, bool dbat, int idx)
{
    if (dbat) {
        aCPU.dbat_brpn[idx] = (aCPU.dbatl[idx] & aCPU.dbat_bl[idx]);
    } else {
        aCPU.ibat_brpn[idx] = (aCPU.ibatl[idx] & aCPU.ibat_bl[idx]);
    }
}

static uint32 ppc_cmp_and_mask[8] = {
    0xfffffff0, 0xffffff0f, 0xfffff0ff, 0xffff0fff, 0xfff0ffff, 0xff0fffff, 0xf0ffffff, 0x0fffffff,
};

/*
 *	addx		Add
 *	.422
 */
int ppc_opc_addx(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    aCPU.gpr[rD] = aCPU.gpr[rA] + aCPU.gpr[rB];
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
	return 0;
}

/*
 *	addox		Add with Overflow
 *	.422
 */
int ppc_opc_addox(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    aCPU.gpr[rD] = aCPU.gpr[rA] + aCPU.gpr[rB];
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
    // update XER flags
    PPC_ALU_ERR("addox unimplemented\n");
	return 0;
}

/*
 *	addcx		Add Carrying
 *	.423
 */
int ppc_opc_addcx(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    uint32 a = aCPU.gpr[rA];
    aCPU.gpr[rD] = a + aCPU.gpr[rB];
    aCPU.xer_ca = (aCPU.gpr[rD] < a);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
	return 0;
}

/*
 *	addcox		Add Carrying with Overflow
 *	.423
 */
int ppc_opc_addcox(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    uint32 a = aCPU.gpr[rA];
    aCPU.gpr[rD] = a + aCPU.gpr[rB];
    aCPU.xer_ca = (aCPU.gpr[rD] < a);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
    // update XER flags
    PPC_ALU_ERR("addcox unimplemented\n");
	return 0;
}

/*
 *	addex		Add Extended
 *	.424
 */
int ppc_opc_addex(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    uint32 a = aCPU.gpr[rA];
    uint32 b = aCPU.gpr[rB];
    uint32 ca = aCPU.xer_ca;
    aCPU.gpr[rD] = a + b + ca;
    aCPU.xer_ca = ppc_carry_3(a, b, ca);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
	return 0;
}

/*
 *	addeox		Add Extended with Overflow
 *	.424
 */
int ppc_opc_addeox(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    uint32 a = aCPU.gpr[rA];
    uint32 b = aCPU.gpr[rB];
    uint32 ca = aCPU.xer_ca;
    aCPU.gpr[rD] = a + b + ca;
    aCPU.xer_ca = ppc_carry_3(a, b, ca);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
    // update XER flags
    PPC_ALU_ERR("addeox unimplemented\n");
	return 0;
}

/*
 *	addi		Add Immediate
 *	.425
 */
int ppc_opc_addi(PPC_CPU_State &aCPU)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
    aCPU.gpr[rD] = (rA ? aCPU.gpr[rA] : 0) + imm;
	return 0;
}

/*
 *	addic		Add Immediate Carrying
 *	.426
 */
int ppc_opc_addic(PPC_CPU_State &aCPU)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
    uint32 a = aCPU.gpr[rA];
    aCPU.gpr[rD] = a + imm;
    aCPU.xer_ca = (aCPU.gpr[rD] < a);
	return 0;
}

/*
 *	addic.		Add Immediate Carrying and Record
 *	.427
 */
int ppc_opc_addic_(PPC_CPU_State &aCPU)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
    uint32 a = aCPU.gpr[rA];
    aCPU.gpr[rD] = a + imm;
    aCPU.xer_ca = (aCPU.gpr[rD] < a);
    ppc_update_cr0(aCPU, aCPU.gpr[rD]);
	return 0;
}

/*
 *	addis		Add Immediate Shifted
 *	.428
 */
int ppc_opc_addis(PPC_CPU_State &aCPU)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_Shift16(aCPU.current_opc, rD, rA, imm);
    aCPU.gpr[rD] = (rA ? aCPU.gpr[rA] : 0) + imm;
	return 0;
}

/*
 *	addmex		Add to Minus One Extended
 *	.429
 */
int ppc_opc_addmex(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    PPC_OPC_ASSERT(rB == 0);
    uint32 a = aCPU.gpr[rA];
    uint32 ca = aCPU.xer_ca;
    aCPU.gpr[rD] = a + ca + 0xffffffff;
    aCPU.xer_ca = a || ca;
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
	return 0;
}

/*
 *	addmeox		Add to Minus One Extended with Overflow
 *	.429
 */
int ppc_opc_addmeox(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    PPC_OPC_ASSERT(rB == 0);
    uint32 a = aCPU.gpr[rA];
    uint32 ca = (aCPU.xer_ca);
    aCPU.gpr[rD] = a + ca + 0xffffffff;
    aCPU.xer_ca = (a || ca);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
    // update XER flags
    PPC_ALU_ERR("addmeox unimplemented\n");
	return 0;
}

/*
 *	addzex		Add to Zero Extended
 *	.430
 */
int ppc_opc_addzex(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    PPC_OPC_ASSERT(rB == 0);
    uint32 a = aCPU.gpr[rA];
    uint32 ca = aCPU.xer_ca;
    aCPU.gpr[rD] = a + ca;
    aCPU.xer_ca = ((a == 0xffffffff) && ca);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
	return 0;
}

/*
 *	addzeox		Add to Zero Extended with Overflow
 *	.430
 */
int ppc_opc_addzeox(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    PPC_OPC_ASSERT(rB == 0);
    uint32 a = aCPU.gpr[rA];
    uint32 ca = aCPU.xer_ca;
    aCPU.gpr[rD] = a + ca;
    aCPU.xer_ca = ((a == 0xffffffff) && ca);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
    // update XER flags
    PPC_ALU_ERR("addzeox unimplemented\n");
	return 0;
}

/*
 *	andx		AND
 *	.431
 */
int ppc_opc_andx(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    aCPU.gpr[rA] = aCPU.gpr[rS] & aCPU.gpr[rB];
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rA]);
    }
	return 0;
}

/*
 *	andcx		AND with Complement
 *	.432
 */
int ppc_opc_andcx(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    aCPU.gpr[rA] = aCPU.gpr[rS] & ~aCPU.gpr[rB];
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rA]);
    }
	return 0;
}

/*
 *	andi.		AND Immediate
 *	.433
 */
int ppc_opc_andi_(PPC_CPU_State &aCPU)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_UImm(aCPU.current_opc, rS, rA, imm);
    aCPU.gpr[rA] = aCPU.gpr[rS] & imm;
    ppc_update_cr0(aCPU, aCPU.gpr[rA]);
	return 0;
}

/*
 *	andis.		AND Immediate Shifted
 *	.434
 */
int ppc_opc_andis_(PPC_CPU_State &aCPU)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_Shift16(aCPU.current_opc, rS, rA, imm);
    aCPU.gpr[rA] = aCPU.gpr[rS] & imm;
    ppc_update_cr0(aCPU, aCPU.gpr[rA]);
	return 0;
}

int ppc_opc_cmp(PPC_CPU_State &aCPU)
{
    uint32 cr;
    int rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, cr, rA, rB);
    cr >>= 2;
    sint32 a = aCPU.gpr[rA];
    sint32 b = aCPU.gpr[rB];
    uint32 c;
    if (a < b) {
        c = 8;
    } else if (a > b) {
        c = 4;
    } else {
        c = 2;
    }
    if (aCPU.xer & XER_SO) {
        c |= 1;
    }
    cr = 7 - cr;
    aCPU.cr &= ppc_cmp_and_mask[cr];
    aCPU.cr |= c << (cr * 4);
	return 0;
}

/*
 *	cmpi		Compare Immediate
 *	.443
 */
int ppc_opc_cmpi(PPC_CPU_State &aCPU)
{
    uint32 cr;
    int rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, cr, rA, imm);
    cr >>= 2;
    sint32 a = aCPU.gpr[rA];
    sint32 b = imm;
    uint32 c;
    if (a < b) {
        c = 8;
    } else if (a > b) {
        c = 4;
    } else {
        c = 2;
    }
    if (aCPU.xer & XER_SO) {
        c |= 1;
    }
    cr = 7 - cr;
    aCPU.cr &= ppc_cmp_and_mask[cr];
    aCPU.cr |= c << (cr * 4);
	return 0;
}

/*
 *	cmpl		Compare Logical
 *	.444
 */
int ppc_opc_cmpl(PPC_CPU_State &aCPU)
{
    uint32 cr;
    int rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, cr, rA, rB);
    cr >>= 2;
    uint32 a = aCPU.gpr[rA];
    uint32 b = aCPU.gpr[rB];
    uint32 c;
    if (a < b) {
        c = 8;
    } else if (a > b) {
        c = 4;
    } else {
        c = 2;
    }
    if (aCPU.xer & XER_SO) {
        c |= 1;
    }
    cr = 7 - cr;
    aCPU.cr &= ppc_cmp_and_mask[cr];
    aCPU.cr |= c << (cr * 4);
	return 0;
}

/*
 *	cmpli		Compare Logical Immediate
 *	.445
 */
int ppc_opc_cmpli(PPC_CPU_State &aCPU)
{
    uint32 cr;
    int rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_UImm(aCPU.current_opc, cr, rA, imm);
    cr >>= 2;
    uint32 a = aCPU.gpr[rA];
    uint32 b = imm;
    uint32 c;
    if (a < b) {
        c = 8;
    } else if (a > b) {
        c = 4;
    } else {
        c = 2;
    }
    if (aCPU.xer & XER_SO) {
        c |= 1;
    }
    cr = 7 - cr;
    aCPU.cr &= ppc_cmp_and_mask[cr];
    aCPU.cr |= c << (cr * 4);
	return 0;
}

/*
 *	cntlzwx		Count Leading Zeros Word
 *	.447
 */
int ppc_opc_cntlzwx(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    PPC_OPC_ASSERT(rB == 0);
    uint32 n = 0;
    uint32 x = 0x80000000;
    uint32 v = aCPU.gpr[rS];
    while (!(v & x)) {
        n++;
        if (n == 32) {
            break;
        }
        x >>= 1;
    }
    aCPU.gpr[rA] = n;
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rA]);
    }
	return 0;
}

/*
 *	crand		Condition Register AND
 *	.448
 */
int ppc_opc_crand(PPC_CPU_State &aCPU)
{
    int crD, crA, crB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, crD, crA, crB);
    if ((aCPU.cr & (1 << (31 - crA))) && (aCPU.cr & (1 << (31 - crB)))) {
        aCPU.cr |= (1 << (31 - crD));
    } else {
        aCPU.cr &= ~(1 << (31 - crD));
    }
	return 0;
}

/*
 *	crandc		Condition Register AND with Complement
 *	.449
 */
int ppc_opc_crandc(PPC_CPU_State &aCPU)
{
    int crD, crA, crB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, crD, crA, crB);
    if ((aCPU.cr & (1 << (31 - crA))) && !(aCPU.cr & (1 << (31 - crB)))) {
        aCPU.cr |= (1 << (31 - crD));
    } else {
        aCPU.cr &= ~(1 << (31 - crD));
    }
	return 0;
}

/*
 *	creqv		Condition Register Equivalent
 *	.450
 */
int ppc_opc_creqv(PPC_CPU_State &aCPU)
{
    int crD, crA, crB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, crD, crA, crB);
    if (((aCPU.cr & (1 << (31 - crA))) && (aCPU.cr & (1 << (31 - crB)))) ||
        (!(aCPU.cr & (1 << (31 - crA))) && !(aCPU.cr & (1 << (31 - crB))))) {
        aCPU.cr |= (1 << (31 - crD));
    } else {
        aCPU.cr &= ~(1 << (31 - crD));
    }
	return 0;
}

/*
 *	crnand		Condition Register NAND
 *	.451
 */
int ppc_opc_crnand(PPC_CPU_State &aCPU)
{
    int crD, crA, crB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, crD, crA, crB);
    if (!((aCPU.cr & (1 << (31 - crA))) && (aCPU.cr & (1 << (31 - crB))))) {
        aCPU.cr |= (1 << (31 - crD));
    } else {
        aCPU.cr &= ~(1 << (31 - crD));
    }
	return 0;
}

/*
 *	crnor		Condition Register NOR
 *	.452
 */
int ppc_opc_crnor(PPC_CPU_State &aCPU)
{
    int crD, crA, crB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, crD, crA, crB);
    uint32 t = (1 << (31 - crA)) | (1 << (31 - crB));
    if (!(aCPU.cr & t)) {
        aCPU.cr |= (1 << (31 - crD));
    } else {
        aCPU.cr &= ~(1 << (31 - crD));
    }
	return 0;
}

/*
 *	cror		Condition Register OR
 *	.453
 */
int ppc_opc_cror(PPC_CPU_State &aCPU)
{
    int crD, crA, crB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, crD, crA, crB);
    uint32 t = (1 << (31 - crA)) | (1 << (31 - crB));
    if (aCPU.cr & t) {
        aCPU.cr |= (1 << (31 - crD));
    } else {
        aCPU.cr &= ~(1 << (31 - crD));
    }
	return 0;
}

/*
 *	crorc		Condition Register OR with Complement
 *	.454
 */
int ppc_opc_crorc(PPC_CPU_State &aCPU)
{
    int crD, crA, crB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, crD, crA, crB);
    if ((aCPU.cr & (1 << (31 - crA))) || !(aCPU.cr & (1 << (31 - crB)))) {
        aCPU.cr |= (1 << (31 - crD));
    } else {
        aCPU.cr &= ~(1 << (31 - crD));
    }
	return 0;
}

/*
 *	crxor		Condition Register XOR
 *	.448
 */
int ppc_opc_crxor(PPC_CPU_State &aCPU)
{
    int crD, crA, crB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, crD, crA, crB);
    if ((!(aCPU.cr & (1 << (31 - crA))) && (aCPU.cr & (1 << (31 - crB)))) ||
        ((aCPU.cr & (1 << (31 - crA))) && !(aCPU.cr & (1 << (31 - crB))))) {
        aCPU.cr |= (1 << (31 - crD));
    } else {
        aCPU.cr &= ~(1 << (31 - crD));
    }
	return 0;
}

/*
 *	divwx		Divide Word
 *	.470
 */
int ppc_opc_divwx(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    if (!aCPU.gpr[rB]) {
        PPC_ALU_WARN("division by zero @%08x\n", aCPU.pc);
        SINGLESTEP("");
    } else {
        sint32 a = aCPU.gpr[rA];
        sint32 b = aCPU.gpr[rB];
        aCPU.gpr[rD] = a / b;
    }
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
	return 0;
}

/*
 *	divwox		Divide Word with Overflow
 *	.470
 */
int ppc_opc_divwox(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    if (!aCPU.gpr[rB]) {
        PPC_ALU_WARN("division by zero\n");
    } else {
        sint32 a = aCPU.gpr[rA];
        sint32 b = aCPU.gpr[rB];
        aCPU.gpr[rD] = a / b;
    }
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
    // update XER flags
    PPC_ALU_ERR("divwox unimplemented\n");
	return 0;
}

/*
 *	divwux		Divide Word Unsigned
 *	.472
 */
int ppc_opc_divwux(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    if (!aCPU.gpr[rB]) {
        PPC_ALU_WARN("division by zero @%08x\n", aCPU.pc);
        SINGLESTEP("");
    } else {
        aCPU.gpr[rD] = aCPU.gpr[rA] / aCPU.gpr[rB];
    }
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
	return 0;
}

/*
 *	divwuox		Divide Word Unsigned with Overflow
 *	.472
 */
int ppc_opc_divwuox(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    if (!aCPU.gpr[rB]) {
        PPC_ALU_WARN("division by zero @%08x\n", aCPU.pc);
    } else {
        aCPU.gpr[rD] = aCPU.gpr[rA] / aCPU.gpr[rB];
    }
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
    // update XER flags
    PPC_ALU_ERR("divwuox unimplemented\n");
	return 0;
}

/*
 *	eqvx		Equivalent
 *	.480
 */
int ppc_opc_eqvx(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    aCPU.gpr[rA] = ~(aCPU.gpr[rS] ^ aCPU.gpr[rB]);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rA]);
    }
	return 0;
}

/*
 *	extsbx		Extend Sign Byte
 *	.481
 */
int ppc_opc_extsbx(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    PPC_OPC_ASSERT(rB == 0);
    aCPU.gpr[rA] = aCPU.gpr[rS];
    if (aCPU.gpr[rA] & 0x80) {
        aCPU.gpr[rA] |= 0xffffff00;
    } else {
        aCPU.gpr[rA] &= ~0xffffff00;
    }
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rA]);
    }
	return 0;
}

/*
 *	extshx		Extend Sign Half Word
 *	.482
 */
int ppc_opc_extshx(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    PPC_OPC_ASSERT(rB == 0);
    aCPU.gpr[rA] = aCPU.gpr[rS];
    if (aCPU.gpr[rA] & 0x8000) {
        aCPU.gpr[rA] |= 0xffff0000;
    } else {
        aCPU.gpr[rA] &= ~0xffff0000;
    }
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rA]);
    }
	return 0;
}

/*
 *	mulhwx		Multiply High Word
 *	.595
 */
int ppc_opc_mulhwx(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    sint64 a = sint32(aCPU.gpr[rA]);
    sint64 b = sint32(aCPU.gpr[rB]);
    sint64 c = a * b;
    aCPU.gpr[rD] = uint64(c) >> 32;
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
	return 0;
}

/*
 *	mulhwux		Multiply High Word Unsigned
 *	.596
 */
int ppc_opc_mulhwux(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    uint64 a = aCPU.gpr[rA];
    uint64 b = aCPU.gpr[rB];
    uint64 c = a * b;
    aCPU.gpr[rD] = c >> 32;
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
	return 0;
}

/*
 *	mulli		Multiply Low Immediate
 *	.598
 */
int ppc_opc_mulli(PPC_CPU_State &aCPU)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
    // FIXME: signed / unsigned correct?
    aCPU.gpr[rD] = aCPU.gpr[rA] * imm;
	return 0;
}

/*
 *	mullwx		Multiply Low Word
 *	.599
 */
int ppc_opc_mullwx(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    aCPU.gpr[rD] = aCPU.gpr[rA] * aCPU.gpr[rB];
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
    if (aCPU.current_opc & PPC_OPC_OE) {
        // update XER flags
        PPC_ALU_ERR("mullwox unimplemented\n");
    }
	return 0;
}

/*
 *	nandx		NAND
 *	.600
 */
int ppc_opc_nandx(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    aCPU.gpr[rA] = ~(aCPU.gpr[rS] & aCPU.gpr[rB]);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rA]);
    }
	return 0;
}

/*
 *	negx		Negate
 *	.601
 */
int ppc_opc_negx(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    PPC_OPC_ASSERT(rB == 0);
    aCPU.gpr[rD] = -aCPU.gpr[rA];
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
	return 0;
}

/*
 *	negox		Negate with Overflow
 *	.601
 */
int ppc_opc_negox(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    PPC_OPC_ASSERT(rB == 0);
    aCPU.gpr[rD] = -aCPU.gpr[rA];
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
    // update XER flags
    PPC_ALU_ERR("negox unimplemented\n");
	return 0;
}

/*
 *	norx		NOR
 *	.602
 */
int ppc_opc_norx(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    aCPU.gpr[rA] = ~(aCPU.gpr[rS] | aCPU.gpr[rB]);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rA]);
    }
	return 0;
}

/*
 *	orx		OR
 *	.603
 */
int ppc_opc_orx(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    aCPU.gpr[rA] = aCPU.gpr[rS] | aCPU.gpr[rB];
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rA]);
    }
	return 0;
}

/*
 *	orcx		OR with Complement
 *	.604
 */
int ppc_opc_orcx(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    aCPU.gpr[rA] = aCPU.gpr[rS] | ~aCPU.gpr[rB];
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rA]);
    }
	return 0;
}

/*
 *	ori		OR Immediate
 *	.605
 */
int ppc_opc_ori(PPC_CPU_State &aCPU)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_UImm(aCPU.current_opc, rS, rA, imm);
    aCPU.gpr[rA] = aCPU.gpr[rS] | imm;
	return 0;
}

/*
 *	oris		OR Immediate Shifted
 *	.606
 */
int ppc_opc_oris(PPC_CPU_State &aCPU)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_Shift16(aCPU.current_opc, rS, rA, imm);
    aCPU.gpr[rA] = aCPU.gpr[rS] | imm;
	return 0;
}

/*
 *	rlwimix		Rotate Left Word Immediate then Mask Insert
 *	.617
 */
int ppc_opc_rlwimix(PPC_CPU_State &aCPU)
{
    int rS, rA, SH, MB, ME;
    PPC_OPC_TEMPL_M(aCPU.current_opc, rS, rA, SH, MB, ME);
    uint32 v = ppc_word_rotl(aCPU.gpr[rS], SH);
    uint32 mask = ppc_mask(MB, ME);
    aCPU.gpr[rA] = (v & mask) | (aCPU.gpr[rA] & ~mask);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rA]);
    }
	return 0;
}

/*
 *	rlwinmx		Rotate Left Word Immediate then AND with Mask
 *	.618
 */
int ppc_opc_rlwinmx(PPC_CPU_State &aCPU)
{
    int rS, rA, SH;
    uint32 MB, ME;
    PPC_OPC_TEMPL_M(aCPU.current_opc, rS, rA, SH, MB, ME);
    uint32 v = ppc_word_rotl(aCPU.gpr[rS], SH);
    uint32 mask = ppc_mask(MB, ME);
    aCPU.gpr[rA] = v & mask;
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rA]);
    }
	return 0;
}

/*
 *	rlwnmx		Rotate Left Word then AND with Mask
 *	.620
 */
int ppc_opc_rlwnmx(PPC_CPU_State &aCPU)
{
    int rS, rA, rB, MB, ME;
    PPC_OPC_TEMPL_M(aCPU.current_opc, rS, rA, rB, MB, ME);
    uint32 v = ppc_word_rotl(aCPU.gpr[rS], aCPU.gpr[rB]);
    uint32 mask = ppc_mask(MB, ME);
    aCPU.gpr[rA] = v & mask;
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rA]);
    }
	return 0;
}

/*
 *	slwx		Shift Left Word
 *	.625
 */
int ppc_opc_slwx(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    uint32 s = aCPU.gpr[rB] & 0x3f;
    if (s > 31) {
        aCPU.gpr[rA] = 0;
    } else {
        aCPU.gpr[rA] = aCPU.gpr[rS] << s;
    }
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rA]);
    }
	return 0;
}

/*
 *	srawx		Shift Right Algebraic Word
 *	.628
 */
int ppc_opc_srawx(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    uint32 SH = aCPU.gpr[rB] & 0x3f;
    aCPU.gpr[rA] = aCPU.gpr[rS];
    aCPU.xer_ca = 0;
    if (aCPU.gpr[rA] & 0x80000000) {
        uint32 ca = 0;
        for (uint i = 0; i < SH; i++) {
            if (aCPU.gpr[rA] & 1) {
                ca = 1;
            }
            aCPU.gpr[rA] >>= 1;
            aCPU.gpr[rA] |= 0x80000000;
        }
        if (ca) {
            aCPU.xer_ca = 1;
        }
    } else {
        if (SH > 31) {
            aCPU.gpr[rA] = 0;
        } else {
            aCPU.gpr[rA] >>= SH;
        }
    }
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rA]);
    }
	return 0;
}

/*
 *	srawix		Shift Right Algebraic Word Immediate
 *	.629
 */
int ppc_opc_srawix(PPC_CPU_State &aCPU)
{
    int rS, rA;
    uint32 SH;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, SH);
    aCPU.gpr[rA] = aCPU.gpr[rS];
    aCPU.xer_ca = 0;
    if (aCPU.gpr[rA] & 0x80000000) {
        uint32 ca = 0;
        for (uint i = 0; i < SH; i++) {
            if (aCPU.gpr[rA] & 1) {
                ca = 1;
            }
            aCPU.gpr[rA] >>= 1;
            aCPU.gpr[rA] |= 0x80000000;
        }
        if (ca) {
            aCPU.xer_ca = 1;
        }
    } else {
        aCPU.gpr[rA] >>= SH;
    }
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rA]);
    }
	return 0;
}

/*
 *	srwx		Shift Right Word
 *	.631
 */
int ppc_opc_srwx(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    uint32 v = aCPU.gpr[rB] & 0x3f;
    if (v > 31) {
        aCPU.gpr[rA] = 0;
    } else {
        aCPU.gpr[rA] = aCPU.gpr[rS] >> v;
    }
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rA]);
    }
	return 0;
}

/*
 *	subfx		Subtract From
 *	.666
 */
int ppc_opc_subfx(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    aCPU.gpr[rD] = ~aCPU.gpr[rA] + aCPU.gpr[rB] + 1;
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
	return 0;
}

/*
 *	subfox		Subtract From with Overflow
 *	.666
 */
int ppc_opc_subfox(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    aCPU.gpr[rD] = ~aCPU.gpr[rA] + aCPU.gpr[rB] + 1;
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
    // update XER flags
    PPC_ALU_ERR("subfox unimplemented\n");
	return 0;
}

/*
 *	subfcx		Subtract From Carrying
 *	.667
 */
int ppc_opc_subfcx(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    uint32 a = aCPU.gpr[rA];
    uint32 b = aCPU.gpr[rB];
    aCPU.gpr[rD] = ~a + b + 1;
    aCPU.xer_ca = ppc_carry_3(~a, b, 1);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
	return 0;
}

/*
 *	subfcox		Subtract From Carrying with Overflow
 *	.667
 */
int ppc_opc_subfcox(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    uint32 a = aCPU.gpr[rA];
    uint32 b = aCPU.gpr[rB];
    aCPU.gpr[rD] = ~a + b + 1;
    aCPU.xer_ca = ppc_carry_3(~a, b, 1);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
    // update XER flags
    PPC_ALU_ERR("subfcox unimplemented\n");
	return 0;
}

/*
 *	subfex		Subtract From Extended
 *	.668
 */
int ppc_opc_subfex(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    uint32 a = aCPU.gpr[rA];
    uint32 b = aCPU.gpr[rB];
    uint32 ca = (aCPU.xer_ca);
    aCPU.gpr[rD] = ~a + b + ca;
    aCPU.xer_ca = ppc_carry_3(~a, b, ca);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
	return 0;
}

/*
 *	subfeox		Subtract From Extended with Overflow
 *	.668
 */
int ppc_opc_subfeox(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    uint32 a = aCPU.gpr[rA];
    uint32 b = aCPU.gpr[rB];
    uint32 ca = aCPU.xer_ca;
    aCPU.gpr[rD] = ~a + b + ca;
    aCPU.xer_ca = (ppc_carry_3(~a, b, ca));
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
    // update XER flags
    PPC_ALU_ERR("subfeox unimplemented\n");
	return 0;
}

/*
 *	subfic		Subtract From Immediate Carrying
 *	.669
 */
int ppc_opc_subfic(PPC_CPU_State &aCPU)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
    uint32 a = aCPU.gpr[rA];
    aCPU.gpr[rD] = ~a + imm + 1;
    aCPU.xer_ca = (ppc_carry_3(~a, imm, 1));
	return 0;
}

/*
 *	subfmex		Subtract From Minus One Extended
 *	.670
 */
int ppc_opc_subfmex(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    PPC_OPC_ASSERT(rB == 0);
    uint32 a = aCPU.gpr[rA];
    uint32 ca = aCPU.xer_ca;
    aCPU.gpr[rD] = ~a + ca + 0xffffffff;
    aCPU.xer_ca = ((a != 0xffffffff) || ca);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
	return 0;
}

/*
 *	subfmeox	Subtract From Minus One Extended with Overflow
 *	.670
 */
int ppc_opc_subfmeox(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    PPC_OPC_ASSERT(rB == 0);
    uint32 a = aCPU.gpr[rA];
    uint32 ca = aCPU.xer_ca;
    aCPU.gpr[rD] = ~a + ca + 0xffffffff;
    aCPU.xer_ca = ((a != 0xffffffff) || ca);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
    // update XER flags
    PPC_ALU_ERR("subfmeox unimplemented\n");
	return 0;
}

/*
 *	subfzex		Subtract From Zero Extended
 *	.671
 */
int ppc_opc_subfzex(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    PPC_OPC_ASSERT(rB == 0);
    uint32 a = aCPU.gpr[rA];
    uint32 ca = aCPU.xer_ca;
    aCPU.gpr[rD] = ~a + ca;
    aCPU.xer_ca = (!a && ca);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
	return 0;
}

/*
 *	subfzeox	Subtract From Zero Extended with Overflow
 *	.671
 */
int ppc_opc_subfzeox(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    PPC_OPC_ASSERT(rB == 0);
    uint32 a = aCPU.gpr[rA];
    uint32 ca = aCPU.xer_ca;
    aCPU.gpr[rD] = ~a + ca;
    aCPU.xer_ca = (!a && ca);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
    // update XER flags
    PPC_ALU_ERR("subfzeox unimplemented\n");
	return 0;
}

/*
 *	xorx		XOR
 *	.680
 */
int ppc_opc_xorx(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    aCPU.gpr[rA] = aCPU.gpr[rS] ^ aCPU.gpr[rB];
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rA]);
    }
	return 0;
}

/*
 *	xori		XOR Immediate
 *	.681
 */
int ppc_opc_xori(PPC_CPU_State &aCPU)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_UImm(aCPU.current_opc, rS, rA, imm);
    aCPU.gpr[rA] = aCPU.gpr[rS] ^ imm;
	return 0;
}

/*
 *	xoris		XOR Immediate Shifted
 *	.682
 */
int ppc_opc_xoris(PPC_CPU_State &aCPU)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_Shift16(aCPU.current_opc, rS, rA, imm);
    aCPU.gpr[rA] = aCPU.gpr[rS] ^ imm;
	return 0;
}

/*
 *	bx		Branch
 *	.435
 */
int ppc_opc_bx(PPC_CPU_State &aCPU)
{
    uint32 li;
    PPC_OPC_TEMPL_I(aCPU.current_opc, li);
    if (!(aCPU.current_opc & PPC_OPC_AA)) {
        li += aCPU.pc;
    }
    if (aCPU.current_opc & PPC_OPC_LK) {
        aCPU.lr = aCPU.pc + 4;
    }
    aCPU.npc = li;
	return 0;
}

/*
 *	bcx		Branch Conditional
 *	.436
 */
int ppc_opc_bcx(PPC_CPU_State &aCPU)
{
    uint32 BO, BI, BD;
    PPC_OPC_TEMPL_B(aCPU.current_opc, BO, BI, BD);
    if (!(BO & 4)) {
        aCPU.ctr--;
    }
    bool bo2 = (BO & 2);
    bool bo8 = (BO & 8); // branch condition true
    bool cr = (aCPU.cr & (1 << (31 - BI)));
    bool taken = ((BO & 4) || ((aCPU.ctr != 0) ^ bo2)) && ((BO & 16) || (!(cr ^ bo8)));
    if (taken) {
        if (!(aCPU.current_opc & PPC_OPC_AA)) {
            BD += aCPU.pc;
        }
        if (aCPU.current_opc & PPC_OPC_LK) {
            aCPU.lr = aCPU.pc + 4;
        }
        aCPU.npc = BD;
    }
	return 0;
}

/*
 *	bcctrx		Branch Conditional to Count Register
 *	.438
 */
int ppc_opc_bcctrx(PPC_CPU_State &aCPU)
{
    uint32 BO, BI, BD;
    PPC_OPC_TEMPL_XL(aCPU.current_opc, BO, BI, BD);
    PPC_OPC_ASSERT(BD == 0);
    PPC_OPC_ASSERT(!(BO & 2));
    bool bo8 = (BO & 8);
    bool cr = (aCPU.cr & (1 << (31 - BI)));
    if ((BO & 16) || (!(cr ^ bo8))) {
        if (aCPU.current_opc & PPC_OPC_LK) {
            aCPU.lr = aCPU.pc + 4;
        }
        aCPU.npc = aCPU.ctr & 0xfffffffc;
    }
	return 0;
}

/*
 *	bclrx		Branch Conditional to Link Register
 *	.440
 */
int ppc_opc_bclrx(PPC_CPU_State &aCPU)
{
    uint32 BO, BI, BD;
    PPC_OPC_TEMPL_XL(aCPU.current_opc, BO, BI, BD);
    PPC_OPC_ASSERT(BD == 0);
    if (!(BO & 4)) {
        aCPU.ctr--;
    }
    bool bo2 = (BO & 2);
    bool bo8 = (BO & 8);
    bool cr = (aCPU.cr & (1 << (31 - BI)));
    if (((BO & 4) || ((aCPU.ctr != 0) ^ bo2)) && ((BO & 16) || (!(cr ^ bo8)))) {
        BD = aCPU.lr & 0xfffffffc;
        if (aCPU.current_opc & PPC_OPC_LK) {
            aCPU.lr = aCPU.pc + 4;
        }
        aCPU.npc = BD;
        if (aCPU.lr & 3) {
            fprintf(stderr, "[BCLRX-ALIGN] LR=%08x not aligned! pc=%08x npc=%08x\n",
                aCPU.lr, aCPU.pc, BD);
        }
        if (BD >= 0xBF000000 && BD < 0xC0000000) {
            PPC_ALU_ERR("BCLRX PROM dispatch: npc=%08x lr=%08x pc=%08x msr=%08x\n",
                BD, aCPU.lr, aCPU.pc, aCPU.msr);
        }
    }
	return 0;
}

/*
 *	dcbf		Data Cache Block Flush
 *	.458
 */

/*
 *	dcbi		Data Cache Block Invalidate
 *	.460
 */

/*
 *	dcbst		Data Cache Block Store
 *	.461
 */

/*
 *	dcbt		Data Cache Block Touch
 *	.462
 */

/*
 *	dcbtst		Data Cache Block Touch for Store
 *	.463
 */

/*
 *	eciwx		External Control In Word Indexed
 *	.474
 */
int ppc_opc_eciwx(PPC_CPU_State &aCPU)
{
    PPC_OPC_ERR("eciwx unimplemented.\n");
	return 0;
}

/*
 *	ecowx		External Control Out Word Indexed
 *	.476
 */
int ppc_opc_ecowx(PPC_CPU_State &aCPU)
{
    PPC_OPC_ERR("ecowx unimplemented.\n");
	return 0;
}

/*
 *	eieio		Enforce In-Order Execution of I/O
 *	.478
 */
int ppc_opc_eieio(PPC_CPU_State &aCPU)
{
    // NO-OP
	return 0;
}

/*
 *	icbi		Instruction Cache Block Invalidate
 *	.519
 */
int ppc_opc_icbi(PPC_CPU_State &aCPU)
{
    int rA, rD, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
    uint32 ea = (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB];
    uint32 pa;
    if (ppc_effective_to_physical(aCPU, ea, PPC_MMU_READ | PPC_MMU_NO_EXC, pa) != PPC_MMU_OK) {
        return 0;
    }
    if (pa >= gMemorySize) return 0;
    if (!aCPU.jitc) return 0;
    uint32 pageIndex = pa >> 12;
    JITC &jitc = *aCPU.jitc;
    ClientPage *cp = jitc.clientPages[pageIndex];
    if (cp) {
        jitcDestroyAndFreeClientPage(jitc, cp);
    }
	return 0;
}

/*
 *	isync		Instruction Synchronize
 *	.520
 */
int ppc_opc_isync(PPC_CPU_State &aCPU)
{
    // NO-OP
	return 0;
}

/*
 *	mcrf		Move Condition Register Field
 *	.561
 */
int ppc_opc_mcrf(PPC_CPU_State &aCPU)
{
    uint32 crD, crS, bla;
    PPC_OPC_TEMPL_X(aCPU.current_opc, crD, crS, bla);
    // FIXME: bla == 0
    crD >>= 2;
    crS >>= 2;
    crD = 7 - crD;
    crS = 7 - crS;
    uint32 c = (aCPU.cr >> (crS * 4)) & 0xf;
    aCPU.cr &= ppc_cmp_and_mask[crD];
    aCPU.cr |= c << (crD * 4);
	return 0;
}

/*
 *	mcrfs		Move to Condition Register from FPSCR
 *	.562
 */
int ppc_opc_mcrfs(PPC_CPU_State &aCPU)
{
    PPC_OPC_ERR("mcrfs unimplemented.\n");
	return 0;
}

/*
 *	mcrxr		Move to Condition Register from XER
 *	.563
 */
int ppc_opc_mcrxr(PPC_CPU_State &aCPU)
{
    int crD, a, b;
    PPC_OPC_TEMPL_X(aCPU.current_opc, crD, a, b);
    crD >>= 2;
    crD = 7 - crD;
    aCPU.cr &= ppc_cmp_and_mask[crD];
    aCPU.cr |= (((aCPU.xer & 0xf0000000) | (aCPU.xer_ca ? XER_CA : 0)) >> 28) << (crD * 4);
    aCPU.xer = ~0xf0000000;
    aCPU.xer_ca = 0;
	return 0;
}

/*
 *	mfcr		Move from Condition Register
 *	.564
 */
int ppc_opc_mfcr(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
    PPC_OPC_ASSERT(rA == 0 && rB == 0);
    aCPU.gpr[rD] = aCPU.cr;
	return 0;
}

/*
 *	mffs		Move from FPSCR
 *	.565
 */
int ppc_opc_mffsx(PPC_CPU_State &aCPU)
{
    int frD, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, frD, rA, rB);
    PPC_OPC_ASSERT(rA == 0 && rB == 0);
    aCPU.fpr[frD] = aCPU.fpscr;
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_OPC_ERR("mffs. unimplemented.\n");
    }
	return 0;
}

/*
 *	mfmsr		Move from Machine State Register
 *	.566
 */
int ppc_opc_mfmsr(PPC_CPU_State &aCPU)
{
    if (aCPU.msr & MSR_PR) {
        ppc_exception(aCPU, PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
        return 0;
    }
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
    PPC_OPC_ASSERT((rA == 0) && (rB == 0));
    aCPU.gpr[rD] = aCPU.msr;
	return 0;
}

/*
 *	mfspr		Move from Special-Purpose Register
 *	.567
 */
int ppc_opc_mfspr(PPC_CPU_State &aCPU)
{
    int rD, spr1, spr2;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, spr1, spr2);
    switch (spr2) {
    case 0:
        switch (spr1) {
        case 1: aCPU.gpr[rD] = aCPU.xer | (aCPU.xer_ca ? XER_CA : 0); return 0;
        case 8: aCPU.gpr[rD] = aCPU.lr; return 0;
        case 9: aCPU.gpr[rD] = aCPU.ctr; return 0;
        }
    case 8: // altivec makes this user visible
        if (spr1 == 0) {
            aCPU.gpr[rD] = aCPU.vrsave;
            return 0;
        }
    }
    if (aCPU.msr & MSR_PR) {
        ppc_exception(aCPU, PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
        return 0;
    }
    switch (spr2) {
    case 0:
        switch (spr1) {
        case 18: aCPU.gpr[rD] = aCPU.dsisr; return 0;
        case 19: aCPU.gpr[rD] = aCPU.dar; return 0;
        case 22: {
            readDEC(aCPU); aCPU.gpr[rD] = aCPU.dec;
            static int rc = 0; rc++;
            if (rc <= 100 || rc % 1000 == 0)
                fprintf(stderr, "[SPR] mfspr DEC #%d: dec=%08x pc=%08x\n", rc, aCPU.dec, aCPU.pc);
            return 0;
        }
        case 25: aCPU.gpr[rD] = aCPU.sdr1; return 0;
        case 26: aCPU.gpr[rD] = aCPU.srr[0]; return 0;
        case 27: aCPU.gpr[rD] = aCPU.srr[1]; return 0;
        }
        break;
    case 8:
        switch (spr1) {
        case 12: aCPU.gpr[rD] = ppc_get_cpu_timebase(); return 0;
        case 13: aCPU.gpr[rD] = ppc_get_cpu_timebase() >> 32; return 0;
        case 16: aCPU.gpr[rD] = aCPU.sprg[0]; return 0;
        case 17: aCPU.gpr[rD] = aCPU.sprg[1]; return 0;
        case 18: aCPU.gpr[rD] = aCPU.sprg[2]; return 0;
        case 19: aCPU.gpr[rD] = aCPU.sprg[3]; return 0;
        case 26: aCPU.gpr[rD] = aCPU.ear; return 0;
        case 31: aCPU.gpr[rD] = aCPU.pvr; return 0;
        }
        break;
    case 16:
        switch (spr1) {
        case 16: aCPU.gpr[rD] = aCPU.ibatu[0]; return 0;
        case 17: aCPU.gpr[rD] = aCPU.ibatl[0]; return 0;
        case 18: aCPU.gpr[rD] = aCPU.ibatu[1]; return 0;
        case 19: aCPU.gpr[rD] = aCPU.ibatl[1]; return 0;
        case 20: aCPU.gpr[rD] = aCPU.ibatu[2]; return 0;
        case 21: aCPU.gpr[rD] = aCPU.ibatl[2]; return 0;
        case 22: aCPU.gpr[rD] = aCPU.ibatu[3]; return 0;
        case 23: aCPU.gpr[rD] = aCPU.ibatl[3]; return 0;
        case 24: aCPU.gpr[rD] = aCPU.dbatu[0]; return 0;
        case 25: aCPU.gpr[rD] = aCPU.dbatl[0]; return 0;
        case 26: aCPU.gpr[rD] = aCPU.dbatu[1]; return 0;
        case 27: aCPU.gpr[rD] = aCPU.dbatl[1]; return 0;
        case 28: aCPU.gpr[rD] = aCPU.dbatu[2]; return 0;
        case 29: aCPU.gpr[rD] = aCPU.dbatl[2]; return 0;
        case 30: aCPU.gpr[rD] = aCPU.dbatu[3]; return 0;
        case 31: aCPU.gpr[rD] = aCPU.dbatl[3]; return 0;
        }
        break;
    case 29:
        switch (spr1) {
        case 16: aCPU.gpr[rD] = 0; return 0;
        case 17: aCPU.gpr[rD] = 0; return 0;
        case 18: aCPU.gpr[rD] = 0; return 0;
        case 24: aCPU.gpr[rD] = 0; return 0;
        case 25: aCPU.gpr[rD] = 0; return 0;
        case 26: aCPU.gpr[rD] = 0; return 0;
        case 28: aCPU.gpr[rD] = 0; return 0;
        case 29: aCPU.gpr[rD] = 0; return 0;
        case 30: aCPU.gpr[rD] = 0; return 0;
        }
    case 31:
        switch (spr1) {
        case 16:
            //			PPC_OPC_WARN("read from spr %d:%d (HID0) not supported!\n", spr1, spr2);
            aCPU.gpr[rD] = aCPU.hid[0];
            return 0;
        case 17:
            PPC_OPC_WARN("read from spr %d:%d (HID1) not supported!\n", spr1, spr2);
            aCPU.gpr[rD] = aCPU.hid[1];
            return 0;
        case 22: aCPU.gpr[rD] = 0; return 0;
        case 23: aCPU.gpr[rD] = 0; return 0;
        case 25:
            PPC_OPC_WARN("read from spr %d:%d (L2CR) not supported! (from %08x)\n", spr1, spr2, aCPU.pc);
            aCPU.gpr[rD] = 0;
            return 0;
        case 27:
            PPC_OPC_WARN("read from spr %d:%d (ICTC) not supported!\n", spr1, spr2);
            aCPU.gpr[rD] = 0;
            return 0;
        case 28:
            //			PPC_OPC_WARN("read from spr %d:%d (THRM1) not supported!\n", spr1, spr2);
            aCPU.gpr[rD] = 0;
            return 0;
        case 29:
            //			PPC_OPC_WARN("read from spr %d:%d (THRM2) not supported!\n", spr1, spr2);
            aCPU.gpr[rD] = 0;
            return 0;
        case 30:
            //			PPC_OPC_WARN("read from spr %d:%d (THRM3) not supported!\n", spr1, spr2);
            aCPU.gpr[rD] = 0;
            return 0;
        case 31:
            //			PPC_OPC_WARN("read from spr %d:%d (???) not supported!\n", spr1, spr2);
            aCPU.gpr[rD] = 0;
            return 0;
        }
    }
    fprintf(stderr, "unknown mfspr: %i:%i\n", spr1, spr2);
    SINGLESTEP("invalid mfspr\n");
	return 0;
}

/*
 *	mfsr		Move from Segment Register
 *	.570
 */
int ppc_opc_mfsr(PPC_CPU_State &aCPU)
{
    if (aCPU.msr & MSR_PR) {
        ppc_exception(aCPU, PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
        return 0;
    }
    int rD, SR, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rD, SR, rB);
    // FIXME: check insn
    aCPU.gpr[rD] = aCPU.sr[SR & 0xf];
	return 0;
}

/*
 *	mfsrin		Move from Segment Register Indirect
 *	.572
 */
int ppc_opc_mfsrin(PPC_CPU_State &aCPU)
{
    if (aCPU.msr & MSR_PR) {
        ppc_exception(aCPU, PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
        return 0;
    }
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
    // FIXME: check insn
    aCPU.gpr[rD] = aCPU.sr[aCPU.gpr[rB] >> 28];
	return 0;
}

/*
 *	mftb		Move from Time Base
 *	.574
 */
int ppc_opc_mftb(PPC_CPU_State &aCPU)
{
    int rD, spr1, spr2;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rD, spr1, spr2);
    switch (spr2) {
    case 8:
        switch (spr1) {
        case 12: aCPU.gpr[rD] = ppc_get_cpu_timebase(); return 0;
        case 13:
            aCPU.gpr[rD] = ppc_get_cpu_timebase() >> 32;
            return 0;
            /*		case 12: aCPU.gpr[rD] = aCPU.tb; return 0;
		case 13: aCPU.gpr[rD] = aCPU.tb >> 32; return 0;*/
        }
        break;
    }
    SINGLESTEP("unknown mftb\n");
	return 0;
}

/*
 *	mtcrf		Move to Condition Register Fields
 *	.576
 */
int ppc_opc_mtcrf(PPC_CPU_State &aCPU)
{
    int rS;
    uint32 crm;
    uint32 CRM;
    PPC_OPC_TEMPL_XFX(aCPU.current_opc, rS, crm);
    CRM = ((crm & 0x80) ? 0xf0000000 : 0) | ((crm & 0x40) ? 0x0f000000 : 0) | ((crm & 0x20) ? 0x00f00000 : 0) |
          ((crm & 0x10) ? 0x000f0000 : 0) | ((crm & 0x08) ? 0x0000f000 : 0) | ((crm & 0x04) ? 0x00000f00 : 0) |
          ((crm & 0x02) ? 0x000000f0 : 0) | ((crm & 0x01) ? 0x0000000f : 0);
    aCPU.cr = (aCPU.gpr[rS] & CRM) | (aCPU.cr & ~CRM);
	return 0;
}

/*
 *	mtfsb0x		Move to FPSCR Bit 0
 *	.577
 */
int ppc_opc_mtfsb0x(PPC_CPU_State &aCPU)
{
    int crbD, n1, n2;
    PPC_OPC_TEMPL_X(aCPU.current_opc, crbD, n1, n2);
    if (crbD != 1 && crbD != 2) {
        aCPU.fpscr &= ~(1 << (31 - crbD));
    }
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_OPC_ERR("mtfsb0. unimplemented.\n");
    }
	return 0;
}

/*
 *	mtfsb1x		Move to FPSCR Bit 1
 *	.578
 */
int ppc_opc_mtfsb1x(PPC_CPU_State &aCPU)
{
    int crbD, n1, n2;
    PPC_OPC_TEMPL_X(aCPU.current_opc, crbD, n1, n2);
    if (crbD != 1 && crbD != 2) {
        aCPU.fpscr |= 1 << (31 - crbD);
    }
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_OPC_ERR("mtfsb1. unimplemented.\n");
    }
	return 0;
}

/*
 *	mtfsfx		Move to FPSCR Fields
 *	.579
 */
int ppc_opc_mtfsfx(PPC_CPU_State &aCPU)
{
    int frB;
    uint32 fm, FM;
    PPC_OPC_TEMPL_XFL(aCPU.current_opc, frB, fm);
    FM = ((fm & 0x80) ? 0xf0000000 : 0) | ((fm & 0x40) ? 0x0f000000 : 0) | ((fm & 0x20) ? 0x00f00000 : 0) |
         ((fm & 0x10) ? 0x000f0000 : 0) | ((fm & 0x08) ? 0x0000f000 : 0) | ((fm & 0x04) ? 0x00000f00 : 0) |
         ((fm & 0x02) ? 0x000000f0 : 0) | ((fm & 0x01) ? 0x0000000f : 0);
    aCPU.fpscr = (aCPU.fpr[frB] & FM) | (aCPU.fpscr & ~FM);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_OPC_ERR("mtfsf. unimplemented.\n");
    }
	return 0;
}

/*
 *	mtfsfix		Move to FPSCR Field Immediate
 *	.580
 */
int ppc_opc_mtfsfix(PPC_CPU_State &aCPU)
{
    int crfD, n1;
    uint32 imm;
    PPC_OPC_TEMPL_X(aCPU.current_opc, crfD, n1, imm);
    crfD >>= 2;
    imm >>= 1;
    crfD = 7 - crfD;
    aCPU.fpscr &= ppc_cmp_and_mask[crfD];
    aCPU.fpscr |= imm << (crfD * 4);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_OPC_ERR("mtfsfi. unimplemented.\n");
    }
	return 0;
}

/*
 *	mtmsr		Move to Machine State Register
 *	.581
 */
int ppc_opc_mtmsr(PPC_CPU_State &aCPU)
{
    if (aCPU.msr & MSR_PR) {
        ppc_exception(aCPU, PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
        return 0;
    }
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    PPC_OPC_ASSERT((rA == 0) && (rB == 0));
    ppc_set_msr(aCPU, aCPU.gpr[rS]);
	return 0;
}

/*
 *	mtspr		Move to Special-Purpose Register
 *	.584
 */
int ppc_opc_mtspr(PPC_CPU_State &aCPU)
{
    int rS, spr1, spr2;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, spr1, spr2);
    switch (spr2) {
    case 0:
        switch (spr1) {
        case 1:
            aCPU.xer = aCPU.gpr[rS] & ~XER_CA;
            aCPU.xer_ca = !!(aCPU.gpr[rS] & XER_CA);
            return 0;
        case 8: aCPU.lr = aCPU.gpr[rS]; return 0;
        case 9: aCPU.ctr = aCPU.gpr[rS]; return 0;
        }
    case 8:
        if (spr1 == 0) {
            aCPU.vrsave = aCPU.gpr[rS];
            return 0;
        }
    }
    if (aCPU.msr & MSR_PR) {
        ppc_exception(aCPU, PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
        return 0;
    }
    switch (spr2) {
    case 0:
        switch (spr1) {
            /*		case 18: aCPU.gpr[rD] = aCPU.dsisr; return 0;
		case 19: aCPU.gpr[rD] = aCPU.dar; return 0;*/
        case 22: {
            static int wc = 0; wc++;
            if (wc <= 100 || wc % 1000 == 0)
                fprintf(stderr, "[SPR] mtspr DEC #%d: val=%08x pc=%08x\n", wc, aCPU.gpr[rS], aCPU.pc);
            writeDEC(aCPU, aCPU.gpr[rS]);
            return 0;
        }
        case 25:
            if (!ppc_mmu_set_sdr1(aCPU, aCPU.gpr[rS], true)) {
                PPC_OPC_ERR("cannot set sdr1\n");
            }
            return 0;
        case 26: aCPU.srr[0] = aCPU.gpr[rS]; return 0;
        case 27: aCPU.srr[1] = aCPU.gpr[rS]; return 0;
        }
        break;
    case 8:
        switch (spr1) {
        case 16: aCPU.sprg[0] = aCPU.gpr[rS]; return 0;
        case 17: aCPU.sprg[1] = aCPU.gpr[rS]; return 0;
        case 18: aCPU.sprg[2] = aCPU.gpr[rS]; return 0;
        case 19: aCPU.sprg[3] = aCPU.gpr[rS]; return 0;
        case 28: writeTBL(aCPU, aCPU.gpr[rS]); return 0;
        case 29:
            writeTBU(aCPU, aCPU.gpr[rS]);
            return 0;
            /*		case 26: aCPU.gpr[rD] = aCPU.ear; return 0;
		case 31: aCPU.gpr[rD] = aCPU.pvr; return 0;*/
        }
        break;
    case 16:
        switch (spr1) {
        case 16:
            aCPU.ibatu[0] = aCPU.gpr[rS];
            ppc_opc_batu_helper(aCPU, false, 0);
            return 0;
        case 17:
            aCPU.ibatl[0] = aCPU.gpr[rS];
            ppc_opc_batl_helper(aCPU, false, 0);
            return 0;
        case 18:
            aCPU.ibatu[1] = aCPU.gpr[rS];
            ppc_opc_batu_helper(aCPU, false, 1);
            return 0;
        case 19:
            aCPU.ibatl[1] = aCPU.gpr[rS];
            ppc_opc_batl_helper(aCPU, false, 1);
            return 0;
        case 20:
            aCPU.ibatu[2] = aCPU.gpr[rS];
            ppc_opc_batu_helper(aCPU, false, 2);
            return 0;
        case 21:
            aCPU.ibatl[2] = aCPU.gpr[rS];
            ppc_opc_batl_helper(aCPU, false, 2);
            return 0;
        case 22:
            aCPU.ibatu[3] = aCPU.gpr[rS];
            ppc_opc_batu_helper(aCPU, false, 3);
            return 0;
        case 23:
            aCPU.ibatl[3] = aCPU.gpr[rS];
            ppc_opc_batl_helper(aCPU, false, 3);
            return 0;
        case 24:
            aCPU.dbatu[0] = aCPU.gpr[rS];
            ppc_opc_batu_helper(aCPU, true, 0);
            return 0;
        case 25:
            aCPU.dbatl[0] = aCPU.gpr[rS];
            ppc_opc_batl_helper(aCPU, true, 0);
            return 0;
        case 26:
            aCPU.dbatu[1] = aCPU.gpr[rS];
            ppc_opc_batu_helper(aCPU, true, 1);
            return 0;
        case 27:
            aCPU.dbatl[1] = aCPU.gpr[rS];
            ppc_opc_batl_helper(aCPU, true, 1);
            return 0;
        case 28:
            aCPU.dbatu[2] = aCPU.gpr[rS];
            ppc_opc_batu_helper(aCPU, true, 2);
            return 0;
        case 29:
            aCPU.dbatl[2] = aCPU.gpr[rS];
            ppc_opc_batl_helper(aCPU, true, 2);
            return 0;
        case 30:
            aCPU.dbatu[3] = aCPU.gpr[rS];
            ppc_opc_batu_helper(aCPU, true, 3);
            return 0;
        case 31:
            aCPU.dbatl[3] = aCPU.gpr[rS];
            ppc_opc_batl_helper(aCPU, true, 3);
            return 0;
        }
        break;
    case 29:
        switch (spr1) {
        case 17: return 0;
        case 24: return 0;
        case 25: return 0;
        case 26: return 0;
        }
    case 31:
        switch (spr1) {
        case 16:
            //			PPC_OPC_WARN("write(%08x) to spr %d:%d (HID0) not supported! @%08x\n", aCPU.gpr[rS], spr1, spr2, aCPU.pc);
            aCPU.hid[0] = aCPU.gpr[rS];
            return 0;
        case 17:
            PPC_OPC_WARN("write(%08x) to spr %d:%d (HID1) not supported! @%08x\n", aCPU.gpr[rS], spr1, spr2, aCPU.pc);
            aCPU.hid[1] = aCPU.gpr[rS];
            return 0;
        case 18: PPC_OPC_ERR("write(%08x) to spr %d:%d (IABR) not supported!\n", aCPU.gpr[rS], spr1, spr2); return 0;
        case 21: PPC_OPC_ERR("write(%08x) to spr %d:%d (DABR) not supported!\n", aCPU.gpr[rS], spr1, spr2); return 0;
        case 22: PPC_OPC_ERR("write(%08x) to spr %d:%d (?) not supported!\n", aCPU.gpr[rS], spr1, spr2); return 0;
        case 23: PPC_OPC_ERR("write(%08x) to spr %d:%d (?) not supported!\n", aCPU.gpr[rS], spr1, spr2); return 0;
        case 27: PPC_OPC_WARN("write(%08x) to spr %d:%d (ICTC) not supported!\n", aCPU.gpr[rS], spr1, spr2); return 0;
        case 28:
            //			PPC_OPC_WARN("write(%08x) to spr %d:%d (THRM1) not supported!\n", aCPU.gpr[rS], spr1, spr2);
            return 0;
        case 29:
            //			PPC_OPC_WARN("write(%08x) to spr %d:%d (THRM2) not supported!\n", aCPU.gpr[rS], spr1, spr2);
            return 0;
        case 30:
            //			PPC_OPC_WARN("write(%08x) to spr %d:%d (THRM3) not supported!\n", aCPU.gpr[rS], spr1, spr2);
            return 0;
        case 31: return 0;
        }
    }
    fprintf(stderr, "unknown mtspr: %i:%i\n", spr1, spr2);
    SINGLESTEP("unknown mtspr\n");
	return 0;
}

/*
 *	mtsr		Move to Segment Register
 *	.587
 */
int ppc_opc_mtsr(PPC_CPU_State &aCPU)
{
    if (aCPU.msr & MSR_PR) {
        ppc_exception(aCPU, PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
        return 0;
    }
    int rS, SR, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, SR, rB);
    // FIXME: check insn
    aCPU.sr[SR & 0xf] = aCPU.gpr[rS];
	return 0;
}

/*
 *	mtsrin		Move to Segment Register Indirect
 *	.591
 */
int ppc_opc_mtsrin(PPC_CPU_State &aCPU)
{
    if (aCPU.msr & MSR_PR) {
        ppc_exception(aCPU, PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
        return 0;
    }
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    // FIXME: check insn
    aCPU.sr[aCPU.gpr[rB] >> 28] = aCPU.gpr[rS];
	return 0;
}

/*
 *	rfi		Return from Interrupt
 *	.607
 */
int ppc_opc_rfi(PPC_CPU_State &aCPU)
{
    if (aCPU.msr & MSR_PR) {
        ppc_exception(aCPU, PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
        return 0;
    }
    ppc_set_msr(aCPU, aCPU.srr[1] & MSR_RFI_SAVE_MASK);
    aCPU.npc = aCPU.srr[0] & 0xfffffffc;
	return 0;
}

int ppc_opc_sc(PPC_CPU_State &aCPU)
{
    if (aCPU.gpr[3] == 0x113724fa && aCPU.gpr[4] == 0x77810f9b) {
        gcard_osi(0);
        return 0;
    }
    ppc_exception(aCPU, PPC_EXC_SC);
	return 0;
}

JITCFlow ppc_opc_gen_sc(JITC &jitc)
{
    jitc.clobberAll();

    // Inline OSI check (matches x86 JIT approach).
    // If not OSI: jump to SC exception (never returns).
    // If OSI: call gcard_osi(0), fall through (flowEndBlock).

    uint osi_call_size = 4 /* MOV W0, #0 */
                       + a64_bl_size((uint64)gcard_osi);
    uint sc_exc_size = 4 /* MOV W0, pc_ofs+4 */
                     + a64_bl_size((uint64)ppc_sc_raise_asm);
    uint check2_size = 4 /* LDR gpr[4] */ + 8 /* MOV 0x77810f9b */
                     + 4 /* CMP */ + 4 /* B.NE */;
    uint skip1 = check2_size + osi_call_size + 4 /* B forward */;
    uint skip2 = osi_call_size + 4 /* B forward */;

    uint total = 4 /* LDR gpr[3] */ + 8 /* MOV 0x113724fa */
               + 4 /* CMP */ + 4 /* B.NE */
               + check2_size + osi_call_size
               + 4 /* B forward */ + sc_exc_size;
    jitc.emitAssure(total);

    // Check gpr[3] == 0x113724fa
    jitc.asmLDRw_cpu(W1, offsetof(PPC_CPU_State, gpr[3]));
    jitc.asmMOV(W2, 0x113724fa);
    jitc.asmCMPw(W1, W2);
    jitc.asmBccForward(A64_NE, skip1);

    // Check gpr[4] == 0x77810f9b
    jitc.asmLDRw_cpu(W1, offsetof(PPC_CPU_State, gpr[4]));
    jitc.asmMOV(W2, 0x77810f9b);
    jitc.asmCMPw(W1, W2);
    jitc.asmBccForward(A64_NE, skip2);

    // OSI match: call gcard_osi(0), fall through to next instruction
    jitc.asmMOV(W0, (uint32)0);
    jitc.asmCALL((NativeAddress)gcard_osi);
    jitc.asmBForward(sc_exc_size);

    // SC exception (not OSI) — never returns
    jitc.asmMOV(W0, jitc.pc + 4);
    jitc.asmCALL((NativeAddress)ppc_sc_raise_asm);

    return flowEndBlock;
}

/*
 *	sync		Synchronize
 *	.672
 */
int ppc_opc_sync(PPC_CPU_State &aCPU)
{
    // NO-OP
	return 0;
}

/*
 *	tlbia		Translation Lookaside Buffer Invalidate All
 *	.676
 */
int ppc_opc_tlbia(PPC_CPU_State &aCPU)
{
    if (aCPU.msr & MSR_PR) {
        ppc_exception(aCPU, PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
        return 0;
    }
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    // FIXME: check rS.. for 0
    ppc_mmu_tlb_invalidate(aCPU);
	return 0;
}

/*
 *	tlbie		Translation Lookaside Buffer Invalidate Entry
 *	.676
 */
int ppc_opc_tlbie(PPC_CPU_State &aCPU)
{
    if (aCPU.msr & MSR_PR) {
        ppc_exception(aCPU, PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
        return 0;
    }
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    // FIXME: check rS.. for 0
    ppc_mmu_tlb_invalidate(aCPU);
	return 0;
}

/*
 *	tlbsync		Translation Lookaside Buffer Syncronize
 *	.677
 */
int ppc_opc_tlbsync(PPC_CPU_State &aCPU)
{
    if (aCPU.msr & MSR_PR) {
        ppc_exception(aCPU, PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
        return 0;
    }
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    // FIXME: check rS.. for 0
	return 0;
}

/*
 *	tw		Trap Word
 *	.678
 */
int ppc_opc_tw(PPC_CPU_State &aCPU)
{
    int TO, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, TO, rA, rB);
    uint32 a = aCPU.gpr[rA];
    uint32 b = aCPU.gpr[rB];
    if (((TO & 16) && ((sint32)a < (sint32)b)) || ((TO & 8) && ((sint32)a > (sint32)b)) || ((TO & 4) && (a == b)) ||
        ((TO & 2) && (a < b)) || ((TO & 1) && (a > b))) {
        ppc_exception(aCPU, PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_TRAP);
    }
	return 0;
}

/*
 *	twi		Trap Word Immediate
 *	.679
 */
int ppc_opc_twi(PPC_CPU_State &aCPU)
{
    int TO, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, TO, rA, imm);
    uint32 a = aCPU.gpr[rA];
    if (((TO & 16) && ((sint32)a < (sint32)imm)) || ((TO & 8) && ((sint32)a > (sint32)imm)) ||
        ((TO & 4) && (a == imm)) || ((TO & 2) && (a < imm)) || ((TO & 1) && (a > imm))) {
        ppc_exception(aCPU, PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_TRAP);
    }
	return 0;
}

/*
 *  ========================================================
 *  Native gen_ functions for privileged and trap opcodes
 *  ========================================================
 */

JITCFlow ppc_opc_gen_mfmsr(JITC &jitc)
{
    ppc_opc_gen_check_privilege(jitc);
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
    jitc.asmLDRw_cpu(W0, offsetof(PPC_CPU_State, msr));
    jitc.asmSTRw_cpu(W0, GPR_OFS(rD));
    return flowContinue;
}

JITCFlow ppc_opc_gen_mfsr(JITC &jitc)
{
    ppc_opc_gen_check_privilege(jitc);
    int rD, SR, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rD, SR, rB);
    jitc.asmLDRw_cpu(W0, offsetof(PPC_CPU_State, sr[SR & 0xf]));
    jitc.asmSTRw_cpu(W0, GPR_OFS(rD));
    return flowContinue;
}

JITCFlow ppc_opc_gen_mfsrin(JITC &jitc)
{
    ppc_opc_gen_check_privilege(jitc);
    // Use interpreter for the indirect SR lookup
    ppc_opc_gen_interpret(jitc, ppc_opc_mfsrin);
    return flowContinue;
}

JITCFlow ppc_opc_gen_mtsr(JITC &jitc)
{
    ppc_opc_gen_check_privilege(jitc);
    // mtsr changes segment registers — use interpreter + invalidate TLB
    ppc_opc_gen_interpret(jitc, ppc_opc_mtsr);
    return flowEndBlock;
}

JITCFlow ppc_opc_gen_mtsrin(JITC &jitc)
{
    ppc_opc_gen_check_privilege(jitc);
    ppc_opc_gen_interpret(jitc, ppc_opc_mtsrin);
    return flowEndBlock;
}

JITCFlow ppc_opc_gen_tlbia(JITC &jitc)
{
    jitc.clobberAll();
    ppc_opc_gen_check_privilege(jitc);
    // Invalidate all TLB, then dispatch to next instruction
    jitc.asmMOV(X0, X20);
    jitc.asmCALL((NativeAddress)ppc_mmu_tlb_invalidate_all_asm);
    jitc.asmMOV(W0, jitc.pc + 4);
    jitc.asmCALL((NativeAddress)ppc_new_pc_rel_asm);
    return flowEndBlockUnreachable;
}

JITCFlow ppc_opc_gen_tlbie(JITC &jitc)
{
    jitc.clobberAll();
    ppc_opc_gen_check_privilege(jitc);
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
    // W0 = gpr[rB] (EA to invalidate)
    jitc.asmLDRw_cpu(W0, GPR_OFS(rB));
    jitc.asmCALL((NativeAddress)ppc_mmu_tlb_invalidate_entry_asm);
    jitc.asmMOV(W0, jitc.pc + 4);
    jitc.asmCALL((NativeAddress)ppc_new_pc_rel_asm);
    return flowEndBlockUnreachable;
}

JITCFlow ppc_opc_gen_tlbsync(JITC &jitc)
{
    ppc_opc_gen_check_privilege(jitc);
    return flowContinue;
}

JITCFlow ppc_opc_gen_tw(JITC &jitc)
{
    jitc.clobberAll();
    int TO, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, TO, rA, rB);

    if (TO == 0) {
        // Never trap
        return flowContinue;
    }

    // Load operands
    jitc.asmLDRw_cpu(W0, GPR_OFS(rA));
    jitc.asmLDRw_cpu(W1, GPR_OFS(rB));
    jitc.asmCMPw(W0, W1);

    if (TO == 0x1f) {
        // Always trap
        jitc.asmMOV(W0, jitc.pc);
        jitc.asmMOV(W1, PPC_EXC_PROGRAM_TRAP);
        jitc.asmCALL((NativeAddress)ppc_program_exception_asm);
        return flowEndBlockUnreachable;
    }

    // Conditional trap: branch to exception if any TO condition matches
    int nconds = __builtin_popcount(TO & 0x1f);
    uint trap_body = a64_movw_size(jitc.pc)
                   + a64_movw_size(PPC_EXC_PROGRAM_TRAP)
                   + a64_bl_size((uint64)ppc_program_exception_asm);
    jitc.emitAssure(nconds * 4 + 4 + trap_body);

    // Emit conditional branches to trap path
    struct { int bit; A64Cond cond; } conds[] = {
        {16, A64_LT}, {8, A64_GT}, {4, A64_EQ}, {2, A64_CC}, {1, A64_HI}
    };
    int idx = 0;
    for (auto &c : conds) {
        if (TO & c.bit) {
            // Skip remaining conds + B to reach trap path
            uint skip = (nconds - 1 - idx) * 4 + 4;
            jitc.asmBccForward(c.cond, skip);
            idx++;
        }
    }

    // B forward over trap body (no-trap path)
    NativeAddress end = jitc.asmHERE() + 4 + trap_body;
    jitc.asmBForward(trap_body);

    // Trap path
    jitc.asmMOV(W0, jitc.pc);
    jitc.asmMOV(W1, PPC_EXC_PROGRAM_TRAP);
    jitc.asmCALL((NativeAddress)ppc_program_exception_asm);

    jitc.asmAssertHERE(end, "tw");

    return flowContinue;
}

JITCFlow ppc_opc_gen_twi(JITC &jitc)
{
    jitc.clobberAll();
    int TO, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(jitc.current_opc, TO, rA, imm);

    if (TO == 0) {
        return flowContinue;
    }

    // Load operand and compare with immediate
    jitc.asmLDRw_cpu(W0, GPR_OFS(rA));
    jitc.asmMOV(W1, imm);
    jitc.asmCMPw(W0, W1);

    if (TO == 0x1f) {
        jitc.asmMOV(W0, jitc.pc);
        jitc.asmMOV(W1, PPC_EXC_PROGRAM_TRAP);
        jitc.asmCALL((NativeAddress)ppc_program_exception_asm);
        return flowEndBlockUnreachable;
    }

    // Conditional trap: branch to exception if any TO condition matches
    int nconds = __builtin_popcount(TO & 0x1f);
    uint trap_body = a64_movw_size(jitc.pc)
                   + a64_movw_size(PPC_EXC_PROGRAM_TRAP)
                   + a64_bl_size((uint64)ppc_program_exception_asm);
    jitc.emitAssure(nconds * 4 + 4 + trap_body);

    struct { int bit; A64Cond cond; } conds[] = {
        {16, A64_LT}, {8, A64_GT}, {4, A64_EQ}, {2, A64_CC}, {1, A64_HI}
    };
    int idx = 0;
    for (auto &c : conds) {
        if (TO & c.bit) {
            uint skip = (nconds - 1 - idx) * 4 + 4;
            jitc.asmBccForward(c.cond, skip);
            idx++;
        }
    }

    NativeAddress end = jitc.asmHERE() + 4 + trap_body;
    jitc.asmBForward(trap_body);

    jitc.asmMOV(W0, jitc.pc);
    jitc.asmMOV(W1, PPC_EXC_PROGRAM_TRAP);
    jitc.asmCALL((NativeAddress)ppc_program_exception_asm);

    jitc.asmAssertHERE(end, "twi");

    return flowContinue;
}

/*      dcba	    Data Cache Block Allocate
 *      .???
 */
