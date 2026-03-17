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
        // rD = SIMM
        jitc.emitMOV32((NativeReg)16, (uint32)simm);
        jitc.emitSTR32_cpu((NativeReg)16, GPR_OFS(rD));
    } else {
        // rD = gpr[rA] + SIMM
        jitc.emitLDR32_cpu((NativeReg)16, GPR_OFS(rA));
        if (simm >= 0 && simm < 4096) {
            jitc.emit32(a64_ADDw_imm(16, 16, simm));
        } else if (simm < 0 && (-simm) < 4096) {
            jitc.emit32(a64_SUBw_imm(16, 16, -simm));
        } else {
            jitc.emitMOV32((NativeReg)17, (uint32)simm);
            jitc.emit32(a64_ADDw_reg(16, 16, 17));
        }
        jitc.emitSTR32_cpu((NativeReg)16, GPR_OFS(rD));
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
        jitc.emitMOV32((NativeReg)16, shifted);
        jitc.emitSTR32_cpu((NativeReg)16, GPR_OFS(rD));
    } else {
        jitc.emitLDR32_cpu((NativeReg)16, GPR_OFS(rA));
        jitc.emitMOV32((NativeReg)17, shifted);
        jitc.emit32(a64_ADDw_reg(16, 16, 17));
        jitc.emitSTR32_cpu((NativeReg)16, GPR_OFS(rD));
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
        // nop (ori 0,0,0)
        return flowContinue;
    }
    jitc.emitLDR32_cpu((NativeReg)16, GPR_OFS(rS));
    if (imm != 0) {
        jitc.emitMOV32((NativeReg)17, imm);
        jitc.emit32(a64_ORRw_reg(16, 16, 17));
    }
    jitc.emitSTR32_cpu((NativeReg)16, GPR_OFS(rA));
    return flowContinue;
}

/*
 *  cmpi crfD, rA, SIMM  (opcode 11)
 *  Compare rA with sign-extended immediate, set CR field crfD.
 *
 *  CR field layout (4 bits): LT GT EQ SO
 */
JITCFlow ppc_opc_gen_cmpi(JITC &jitc)
{
    int crfD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(jitc.current_opc, crfD, rA, imm);
    crfD >>= 2; // field number (0-7)
    sint32 simm = (sint32)imm;

    // Load gpr[rA]
    jitc.emitLDR32_cpu((NativeReg)16, GPR_OFS(rA));
    // Load SIMM into W17
    jitc.emitMOV32((NativeReg)17, (uint32)simm);

    // For now, fall back to interpreter for CR update (complex bit packing)
    // TODO: generate native CR update
    extern void ppc_opc_cmpi(PPC_CPU_State &);
    ppc_opc_gen_interpret(jitc, ppc_opc_cmpi);
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
        jitc.emitLDR32_cpu((NativeReg)16, offsetof(PPC_CPU_State, current_code_base));
        jitc.emitMOV32((NativeReg)17, jitc.pc + 4);
        jitc.emit32(a64_ADDw_reg(16, 16, 17));
        jitc.emitSTR32_cpu((NativeReg)16, offsetof(PPC_CPU_State, lr));
    }

    uint32 target;
    if (aa) {
        target = li;
    } else {
        // PC-relative: we need current_code_base + pc + li at runtime
        // But current_code_base + pc is the current EA
        // Emit: W0 = current_code_base + pc + li
        jitc.emitLDR32_cpu((NativeReg)0, offsetof(PPC_CPU_State, current_code_base));
        jitc.emitMOV32((NativeReg)17, jitc.pc + li);
        jitc.emit32(a64_ADDw_reg(0, 0, 17));
        // Jump to ppc_new_pc_asm (W0 = new effective PC)
        jitc.emitBLR((NativeAddress)ppc_new_pc_asm);
        return flowEndBlockUnreachable;
    }

    // Absolute address
    jitc.emitMOV32((NativeReg)0, target);
    jitc.emitBLR((NativeAddress)ppc_new_pc_asm);
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
    uint32 BO, BI, BD;
    PPC_OPC_TEMPL_B(jitc.current_opc, BO, BI, BD);

    bool lk = jitc.current_opc & PPC_OPC_LK;
    bool aa = jitc.current_opc & PPC_OPC_AA;

    // For all cases, use the interpreter to evaluate the branch condition.
    // This correctly handles CTR decrement, all BO variants, and LK.
    // The optimization is in how we dispatch to the target afterwards.
    ppc_opc_gen_interpret(jitc, ppc_opc_bcx);

    // After the interpreter runs, npc holds the next PC:
    //   - If branch taken: npc = target EA
    //   - If not taken: npc = pc + 4

    if (!lk && !aa) {
        // No link, PC-relative.
        // Check if the branch was taken by comparing npc with pc + 4.
        // If npc == pc + 4 → not taken → continue to next instruction.
        // Otherwise → taken → dispatch to target.

        // Emit: load npc, compare with (current_code_base + pc + 4)
        jitc.emitLDR32_cpu((NativeReg)0, offsetof(PPC_CPU_State, npc));
        jitc.emitLDR32_cpu((NativeReg)16, offsetof(PPC_CPU_State, current_code_base));
        jitc.emitMOV32((NativeReg)17, jitc.pc + 4);
        jitc.emit32(a64_ADDw_reg(16, 16, 17)); // W16 = current_code_base + pc + 4
        jitc.emit32(a64_CMPw_reg(0, 16));       // CMP W0, W16

        // If equal (not taken), skip the dispatch
        NativeAddress fixup = jitc.emitBxxFixup();

        // Taken path:
        uint32 target_ofs = (uint32)((sint32)jitc.pc + (sint32)BD);
        if (target_ofs < 4096) {
            // Same-page branch.
            NativeAddress targetNative = jitc.currentPage->entrypoints[target_ofs >> 2];
            if (targetNative) {
                // Target already translated — heartbeat + direct jump
                jitc.emitMOV32((NativeReg)0, target_ofs);
                jitc.emitBLR((NativeAddress)ppc_heartbeat_ext_rel_asm);
                jitc.emitMOV64((NativeReg)16, (uint64)targetNative);
                jitc.emit32(a64_BR(16));
            } else {
                // Target not yet translated — use fast this_page lookup
                jitc.emitMOV32((NativeReg)0, target_ofs);
                jitc.emitBLR((NativeAddress)ppc_heartbeat_ext_rel_asm);
                jitc.emitMOV32((NativeReg)0, target_ofs);
                jitc.emitBLR((NativeAddress)ppc_new_pc_this_page_asm);
            }
        } else {
            // Cross-page or validate mode: full dispatch
            jitc.emitBLR((NativeAddress)ppc_new_pc_asm);
        }

        // Not-taken: patch fixup to B.EQ here
        {
            NativeAddress here = jitc.asmHERE();
            sint64 offset = (sint64)(here - fixup);
            sint32 imm19 = (sint32)(offset / 4);
            uint32 insn = 0x54000000 | ((imm19 & 0x7FFFF) << 5) | A64_EQ;
            *(uint32 *)fixup = insn;
        }

        return flowContinue;
    }

    // General case: always dispatch via npc
    jitc.emitLDR32_cpu((NativeReg)0, offsetof(PPC_CPU_State, npc));
    jitc.emitBLR((NativeAddress)ppc_new_pc_asm);
    return flowEndBlockUnreachable;
}

/*
 *  Helper: emit dispatch for npc with same-page optimization.
 *  Loads npc from CPU state. If npc is on the current page
 *  (same current_code_base), uses the fast ppc_new_pc_this_page_asm
 *  lookup. Otherwise falls through to ppc_new_pc_asm.
 */
static void gen_dispatch_npc(JITC &jitc)
{
    // Load npc and current_code_base
    jitc.emitLDR32_cpu((NativeReg)0, offsetof(PPC_CPU_State, npc));
    jitc.emitLDR32_cpu((NativeReg)16, offsetof(PPC_CPU_State, current_code_base));

    // Compute offset = npc - current_code_base
    jitc.emit32(a64_SUBw_reg(17, 0, 16)); // W17 = npc - ccb

    // If offset < 4096, it's same-page → use fast path
    jitc.emit32(a64_CMPw_imm(17, 4096)); // CMP W17, #4096

    // B.HS (unsigned >=) → cross-page, use full dispatch
    NativeAddress fixup = jitc.emitBxxFixup();

    // Same-page: use fast this_page lookup
    // Save offset in W21 (callee-saved, survives BLR calls)
    jitc.emit32(a64_MOVw(21, 17)); // W21 = offset
    jitc.emit32(a64_MOVw(0, 17));  // W0 = offset
    jitc.emitBLR((NativeAddress)ppc_heartbeat_ext_rel_asm);
    jitc.emit32(a64_MOVw(0, 21));  // W0 = offset (from saved W21)
    jitc.emitBLR((NativeAddress)ppc_new_pc_this_page_asm);

    // Cross-page fallback: patch fixup to B.HS here
    {
        NativeAddress here = jitc.asmHERE();
        sint64 offset = (sint64)(here - fixup);
        sint32 imm19 = (sint32)(offset / 4);
        uint32 insn = 0x54000000 | ((imm19 & 0x7FFFF) << 5) | A64_CS; // B.HS = B.CS
        *(uint32 *)fixup = insn;
    }

    // Full dispatch
    jitc.emitLDR32_cpu((NativeReg)0, offsetof(PPC_CPU_State, npc));
    jitc.emitBLR((NativeAddress)ppc_new_pc_asm);
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
    jitc.emitLDR32_cpu((NativeReg)16, GPR_OFS(rS));
    if (imm) {
        jitc.emitMOV32((NativeReg)17, imm << 16);
        jitc.emit32(a64_ORRw_reg(16, 16, 17));
    }
    jitc.emitSTR32_cpu((NativeReg)16, GPR_OFS(rA));
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
    jitc.emitLDR32_cpu((NativeReg)16, GPR_OFS(rS));
    if (imm) {
        jitc.emitMOV32((NativeReg)17, imm);
        jitc.emit32(a64_EORw_reg(16, 16, 17));
    }
    jitc.emitSTR32_cpu((NativeReg)16, GPR_OFS(rA));
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
    jitc.emitLDR32_cpu((NativeReg)16, GPR_OFS(rS));
    if (imm) {
        jitc.emitMOV32((NativeReg)17, imm << 16);
        jitc.emit32(a64_EORw_reg(16, 16, 17));
    }
    jitc.emitSTR32_cpu((NativeReg)16, GPR_OFS(rA));
    return flowContinue;
}

/*
 *  Register-register ALU helpers.
 *  Pattern: rD = gpr[rA] OP gpr[rB], store to gpr[rD]
 */
/*
 * Rc check: if the PPC opcode has Rc=1 (bit 0), it must update CR0.
 * Our native gen_ functions don't handle CR0 update yet, so fall back
 * to the interpreter when Rc is set.
 */
/* Forward declarations for interpreter functions used by RC_FALLBACK */
void ppc_opc_addx(PPC_CPU_State &);
void ppc_opc_subfx(PPC_CPU_State &);
void ppc_opc_andx(PPC_CPU_State &);
void ppc_opc_orx(PPC_CPU_State &);
void ppc_opc_xorx(PPC_CPU_State &);
void ppc_opc_negx(PPC_CPU_State &);
void ppc_opc_mullwx(PPC_CPU_State &);

#define RC_FALLBACK(interp_func) \
    if (jitc.current_opc & PPC_OPC_Rc) { \
        ppc_opc_gen_interpret(jitc, interp_func); \
        return flowContinue; \
    }

static JITCFlow gen_alu_reg(JITC &jitc, uint32 (*op)(int, int, int))
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
    jitc.emitLDR32_cpu((NativeReg)16, GPR_OFS(rA));
    jitc.emitLDR32_cpu((NativeReg)17, GPR_OFS(rB));
    jitc.emit32(op(16, 16, 17));
    jitc.emitSTR32_cpu((NativeReg)16, GPR_OFS(rD));
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
    jitc.emitLDR32_cpu((NativeReg)17, GPR_OFS(rA));
    jitc.emitLDR32_cpu((NativeReg)16, GPR_OFS(rB));
    jitc.emit32(a64_SUBw_reg(16, 16, 17));
    jitc.emitSTR32_cpu((NativeReg)16, GPR_OFS(rD));
    return flowContinue;
}
/* and rA, rS, rB */
JITCFlow ppc_opc_gen_andx(JITC &jitc)
{
    RC_FALLBACK(ppc_opc_andx);
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
    jitc.emitLDR32_cpu((NativeReg)16, GPR_OFS(rS));
    jitc.emitLDR32_cpu((NativeReg)17, GPR_OFS(rB));
    jitc.emit32(a64_ANDw_reg(16, 16, 17));
    jitc.emitSTR32_cpu((NativeReg)16, GPR_OFS(rA));
    return flowContinue;
}
/* or rA, rS, rB */
JITCFlow ppc_opc_gen_orx(JITC &jitc)
{
    RC_FALLBACK(ppc_opc_orx);
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
    jitc.emitLDR32_cpu((NativeReg)16, GPR_OFS(rS));
    jitc.emitLDR32_cpu((NativeReg)17, GPR_OFS(rB));
    jitc.emit32(a64_ORRw_reg(16, 16, 17));
    jitc.emitSTR32_cpu((NativeReg)16, GPR_OFS(rA));
    return flowContinue;
}
/* xor rA, rS, rB */
JITCFlow ppc_opc_gen_xorx(JITC &jitc)
{
    RC_FALLBACK(ppc_opc_xorx);
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
    jitc.emitLDR32_cpu((NativeReg)16, GPR_OFS(rS));
    jitc.emitLDR32_cpu((NativeReg)17, GPR_OFS(rB));
    jitc.emit32(a64_EORw_reg(16, 16, 17));
    jitc.emitSTR32_cpu((NativeReg)16, GPR_OFS(rA));
    return flowContinue;
}

/*
 *  neg rD, rA
 */
JITCFlow ppc_opc_gen_negx(JITC &jitc)
{
    RC_FALLBACK(ppc_opc_negx);
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
    jitc.emitLDR32_cpu((NativeReg)16, GPR_OFS(rA));
    // NEG Wd, Wn = SUB Wd, WZR, Wn
    jitc.emit32(a64_SUBw_reg(16, 31, 16));
    jitc.emitSTR32_cpu((NativeReg)16, GPR_OFS(rD));
    return flowContinue;
}

/*
 *  slw rA, rS, rB  (shift left word)
 */
JITCFlow ppc_opc_gen_slwx(JITC &jitc)
{
    int rA, rS, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
    // PPC slw: result = (rS << (rB & 0x3F)), zeroed if shift >= 32
    // Use interpreter for correctness (handles shift >= 32)
    extern void ppc_opc_slwx(PPC_CPU_State &);
    ppc_opc_gen_interpret(jitc, ppc_opc_slwx);
    return flowContinue;
}

/*
 *  srw rA, rS, rB  (shift right word)
 */
JITCFlow ppc_opc_gen_srwx(JITC &jitc)
{
    int rA, rS, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
    extern void ppc_opc_srwx(PPC_CPU_State &);
    ppc_opc_gen_interpret(jitc, ppc_opc_srwx);
    return flowContinue;
}

/*
 *  rlwinm rA, rS, SH, MB, ME  (rotate left word then AND with mask)
 */
JITCFlow ppc_opc_gen_rlwinmx(JITC &jitc)
{
    extern void ppc_opc_rlwinmx(PPC_CPU_State &);
    ppc_opc_gen_interpret(jitc, ppc_opc_rlwinmx);
    return flowContinue;
}

/*
 *  mullw rD, rA, rB
 */
JITCFlow ppc_opc_gen_mullwx(JITC &jitc)
{
    RC_FALLBACK(ppc_opc_mullwx);
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
    jitc.emitLDR32_cpu((NativeReg)16, GPR_OFS(rA));
    jitc.emitLDR32_cpu((NativeReg)17, GPR_OFS(rB));
    // MUL Wd, Wn, Wm = MADD Wd, Wn, Wm, WZR
    jitc.emit32(0x1B107E10 | (17 << 16) | (16 << 5) | 16);
    jitc.emitSTR32_cpu((NativeReg)16, GPR_OFS(rD));
    return flowContinue;
}

// Forward declarations for functions defined in ppc_opc.cc
extern void ppc_set_msr(PPC_CPU_State &aCPU, uint32 newmsr);
extern void FASTCALL writeDEC(PPC_CPU_State &aCPU, uint32 newdec);
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
 *	addx		Add
 *	.422
 */
void ppc_opc_addx(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    aCPU.gpr[rD] = aCPU.gpr[rA] + aCPU.gpr[rB];
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
}

/*
 *	addox		Add with Overflow
 *	.422
 */
void ppc_opc_addox(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    aCPU.gpr[rD] = aCPU.gpr[rA] + aCPU.gpr[rB];
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
    // update XER flags
    PPC_ALU_ERR("addox unimplemented\n");
}

/*
 *	addcx		Add Carrying
 *	.423
 */
void ppc_opc_addcx(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    uint32 a = aCPU.gpr[rA];
    aCPU.gpr[rD] = a + aCPU.gpr[rB];
    aCPU.xer_ca = (aCPU.gpr[rD] < a);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
}

/*
 *	addcox		Add Carrying with Overflow
 *	.423
 */
void ppc_opc_addcox(PPC_CPU_State &aCPU)
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
}

/*
 *	addex		Add Extended
 *	.424
 */
void ppc_opc_addex(PPC_CPU_State &aCPU)
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
}

/*
 *	addeox		Add Extended with Overflow
 *	.424
 */
void ppc_opc_addeox(PPC_CPU_State &aCPU)
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
}

/*
 *	addi		Add Immediate
 *	.425
 */
void ppc_opc_addi(PPC_CPU_State &aCPU)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
    aCPU.gpr[rD] = (rA ? aCPU.gpr[rA] : 0) + imm;
}

/*
 *	addic		Add Immediate Carrying
 *	.426
 */
void ppc_opc_addic(PPC_CPU_State &aCPU)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
    uint32 a = aCPU.gpr[rA];
    aCPU.gpr[rD] = a + imm;
    aCPU.xer_ca = (aCPU.gpr[rD] < a);
}

/*
 *	addic.		Add Immediate Carrying and Record
 *	.427
 */
void ppc_opc_addic_(PPC_CPU_State &aCPU)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
    uint32 a = aCPU.gpr[rA];
    aCPU.gpr[rD] = a + imm;
    aCPU.xer_ca = (aCPU.gpr[rD] < a);
    ppc_update_cr0(aCPU, aCPU.gpr[rD]);
}

/*
 *	addis		Add Immediate Shifted
 *	.428
 */
void ppc_opc_addis(PPC_CPU_State &aCPU)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_Shift16(aCPU.current_opc, rD, rA, imm);
    aCPU.gpr[rD] = (rA ? aCPU.gpr[rA] : 0) + imm;
}

/*
 *	addmex		Add to Minus One Extended
 *	.429
 */
void ppc_opc_addmex(PPC_CPU_State &aCPU)
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
}

/*
 *	addmeox		Add to Minus One Extended with Overflow
 *	.429
 */
void ppc_opc_addmeox(PPC_CPU_State &aCPU)
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
}

/*
 *	addzex		Add to Zero Extended
 *	.430
 */
void ppc_opc_addzex(PPC_CPU_State &aCPU)
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
}

/*
 *	addzeox		Add to Zero Extended with Overflow
 *	.430
 */
void ppc_opc_addzeox(PPC_CPU_State &aCPU)
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
}

/*
 *	andx		AND
 *	.431
 */
void ppc_opc_andx(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    aCPU.gpr[rA] = aCPU.gpr[rS] & aCPU.gpr[rB];
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rA]);
    }
}

/*
 *	andcx		AND with Complement
 *	.432
 */
void ppc_opc_andcx(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    aCPU.gpr[rA] = aCPU.gpr[rS] & ~aCPU.gpr[rB];
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rA]);
    }
}

/*
 *	andi.		AND Immediate
 *	.433
 */
void ppc_opc_andi_(PPC_CPU_State &aCPU)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_UImm(aCPU.current_opc, rS, rA, imm);
    aCPU.gpr[rA] = aCPU.gpr[rS] & imm;
    ppc_update_cr0(aCPU, aCPU.gpr[rA]);
}

/*
 *	andis.		AND Immediate Shifted
 *	.434
 */
void ppc_opc_andis_(PPC_CPU_State &aCPU)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_Shift16(aCPU.current_opc, rS, rA, imm);
    aCPU.gpr[rA] = aCPU.gpr[rS] & imm;
    ppc_update_cr0(aCPU, aCPU.gpr[rA]);
}

void ppc_opc_cmp(PPC_CPU_State &aCPU)
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
}

/*
 *	cmpi		Compare Immediate
 *	.443
 */
void ppc_opc_cmpi(PPC_CPU_State &aCPU)
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
}

/*
 *	cmpl		Compare Logical
 *	.444
 */
void ppc_opc_cmpl(PPC_CPU_State &aCPU)
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
}

/*
 *	cmpli		Compare Logical Immediate
 *	.445
 */
void ppc_opc_cmpli(PPC_CPU_State &aCPU)
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
}

/*
 *	cntlzwx		Count Leading Zeros Word
 *	.447
 */
void ppc_opc_cntlzwx(PPC_CPU_State &aCPU)
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
}

/*
 *	crand		Condition Register AND
 *	.448
 */
void ppc_opc_crand(PPC_CPU_State &aCPU)
{
    int crD, crA, crB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, crD, crA, crB);
    if ((aCPU.cr & (1 << (31 - crA))) && (aCPU.cr & (1 << (31 - crB)))) {
        aCPU.cr |= (1 << (31 - crD));
    } else {
        aCPU.cr &= ~(1 << (31 - crD));
    }
}

/*
 *	crandc		Condition Register AND with Complement
 *	.449
 */
void ppc_opc_crandc(PPC_CPU_State &aCPU)
{
    int crD, crA, crB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, crD, crA, crB);
    if ((aCPU.cr & (1 << (31 - crA))) && !(aCPU.cr & (1 << (31 - crB)))) {
        aCPU.cr |= (1 << (31 - crD));
    } else {
        aCPU.cr &= ~(1 << (31 - crD));
    }
}

/*
 *	creqv		Condition Register Equivalent
 *	.450
 */
void ppc_opc_creqv(PPC_CPU_State &aCPU)
{
    int crD, crA, crB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, crD, crA, crB);
    if (((aCPU.cr & (1 << (31 - crA))) && (aCPU.cr & (1 << (31 - crB)))) ||
        (!(aCPU.cr & (1 << (31 - crA))) && !(aCPU.cr & (1 << (31 - crB))))) {
        aCPU.cr |= (1 << (31 - crD));
    } else {
        aCPU.cr &= ~(1 << (31 - crD));
    }
}

/*
 *	crnand		Condition Register NAND
 *	.451
 */
void ppc_opc_crnand(PPC_CPU_State &aCPU)
{
    int crD, crA, crB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, crD, crA, crB);
    if (!((aCPU.cr & (1 << (31 - crA))) && (aCPU.cr & (1 << (31 - crB))))) {
        aCPU.cr |= (1 << (31 - crD));
    } else {
        aCPU.cr &= ~(1 << (31 - crD));
    }
}

/*
 *	crnor		Condition Register NOR
 *	.452
 */
void ppc_opc_crnor(PPC_CPU_State &aCPU)
{
    int crD, crA, crB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, crD, crA, crB);
    uint32 t = (1 << (31 - crA)) | (1 << (31 - crB));
    if (!(aCPU.cr & t)) {
        aCPU.cr |= (1 << (31 - crD));
    } else {
        aCPU.cr &= ~(1 << (31 - crD));
    }
}

/*
 *	cror		Condition Register OR
 *	.453
 */
void ppc_opc_cror(PPC_CPU_State &aCPU)
{
    int crD, crA, crB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, crD, crA, crB);
    uint32 t = (1 << (31 - crA)) | (1 << (31 - crB));
    if (aCPU.cr & t) {
        aCPU.cr |= (1 << (31 - crD));
    } else {
        aCPU.cr &= ~(1 << (31 - crD));
    }
}

/*
 *	crorc		Condition Register OR with Complement
 *	.454
 */
void ppc_opc_crorc(PPC_CPU_State &aCPU)
{
    int crD, crA, crB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, crD, crA, crB);
    if ((aCPU.cr & (1 << (31 - crA))) || !(aCPU.cr & (1 << (31 - crB)))) {
        aCPU.cr |= (1 << (31 - crD));
    } else {
        aCPU.cr &= ~(1 << (31 - crD));
    }
}

/*
 *	crxor		Condition Register XOR
 *	.448
 */
void ppc_opc_crxor(PPC_CPU_State &aCPU)
{
    int crD, crA, crB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, crD, crA, crB);
    if ((!(aCPU.cr & (1 << (31 - crA))) && (aCPU.cr & (1 << (31 - crB)))) ||
        ((aCPU.cr & (1 << (31 - crA))) && !(aCPU.cr & (1 << (31 - crB))))) {
        aCPU.cr |= (1 << (31 - crD));
    } else {
        aCPU.cr &= ~(1 << (31 - crD));
    }
}

/*
 *	divwx		Divide Word
 *	.470
 */
void ppc_opc_divwx(PPC_CPU_State &aCPU)
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
}

/*
 *	divwox		Divide Word with Overflow
 *	.470
 */
void ppc_opc_divwox(PPC_CPU_State &aCPU)
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
}

/*
 *	divwux		Divide Word Unsigned
 *	.472
 */
void ppc_opc_divwux(PPC_CPU_State &aCPU)
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
}

/*
 *	divwuox		Divide Word Unsigned with Overflow
 *	.472
 */
void ppc_opc_divwuox(PPC_CPU_State &aCPU)
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
}

/*
 *	eqvx		Equivalent
 *	.480
 */
void ppc_opc_eqvx(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    aCPU.gpr[rA] = ~(aCPU.gpr[rS] ^ aCPU.gpr[rB]);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rA]);
    }
}

/*
 *	extsbx		Extend Sign Byte
 *	.481
 */
void ppc_opc_extsbx(PPC_CPU_State &aCPU)
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
}

/*
 *	extshx		Extend Sign Half Word
 *	.482
 */
void ppc_opc_extshx(PPC_CPU_State &aCPU)
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
}

/*
 *	mulhwx		Multiply High Word
 *	.595
 */
void ppc_opc_mulhwx(PPC_CPU_State &aCPU)
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
}

/*
 *	mulhwux		Multiply High Word Unsigned
 *	.596
 */
void ppc_opc_mulhwux(PPC_CPU_State &aCPU)
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
}

/*
 *	mulli		Multiply Low Immediate
 *	.598
 */
void ppc_opc_mulli(PPC_CPU_State &aCPU)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
    // FIXME: signed / unsigned correct?
    aCPU.gpr[rD] = aCPU.gpr[rA] * imm;
}

/*
 *	mullwx		Multiply Low Word
 *	.599
 */
void ppc_opc_mullwx(PPC_CPU_State &aCPU)
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
}

/*
 *	nandx		NAND
 *	.600
 */
void ppc_opc_nandx(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    aCPU.gpr[rA] = ~(aCPU.gpr[rS] & aCPU.gpr[rB]);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rA]);
    }
}

/*
 *	negx		Negate
 *	.601
 */
void ppc_opc_negx(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    PPC_OPC_ASSERT(rB == 0);
    aCPU.gpr[rD] = -aCPU.gpr[rA];
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
}

/*
 *	negox		Negate with Overflow
 *	.601
 */
void ppc_opc_negox(PPC_CPU_State &aCPU)
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
}

/*
 *	norx		NOR
 *	.602
 */
void ppc_opc_norx(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    aCPU.gpr[rA] = ~(aCPU.gpr[rS] | aCPU.gpr[rB]);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rA]);
    }
}

/*
 *	orx		OR
 *	.603
 */
void ppc_opc_orx(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    aCPU.gpr[rA] = aCPU.gpr[rS] | aCPU.gpr[rB];
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rA]);
    }
}

/*
 *	orcx		OR with Complement
 *	.604
 */
void ppc_opc_orcx(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    aCPU.gpr[rA] = aCPU.gpr[rS] | ~aCPU.gpr[rB];
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rA]);
    }
}

/*
 *	ori		OR Immediate
 *	.605
 */
void ppc_opc_ori(PPC_CPU_State &aCPU)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_UImm(aCPU.current_opc, rS, rA, imm);
    aCPU.gpr[rA] = aCPU.gpr[rS] | imm;
}

/*
 *	oris		OR Immediate Shifted
 *	.606
 */
void ppc_opc_oris(PPC_CPU_State &aCPU)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_Shift16(aCPU.current_opc, rS, rA, imm);
    aCPU.gpr[rA] = aCPU.gpr[rS] | imm;
}

/*
 *	rlwimix		Rotate Left Word Immediate then Mask Insert
 *	.617
 */
void ppc_opc_rlwimix(PPC_CPU_State &aCPU)
{
    int rS, rA, SH, MB, ME;
    PPC_OPC_TEMPL_M(aCPU.current_opc, rS, rA, SH, MB, ME);
    uint32 v = ppc_word_rotl(aCPU.gpr[rS], SH);
    uint32 mask = ppc_mask(MB, ME);
    aCPU.gpr[rA] = (v & mask) | (aCPU.gpr[rA] & ~mask);
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rA]);
    }
}

/*
 *	rlwinmx		Rotate Left Word Immediate then AND with Mask
 *	.618
 */
void ppc_opc_rlwinmx(PPC_CPU_State &aCPU)
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
}

/*
 *	rlwnmx		Rotate Left Word then AND with Mask
 *	.620
 */
void ppc_opc_rlwnmx(PPC_CPU_State &aCPU)
{
    int rS, rA, rB, MB, ME;
    PPC_OPC_TEMPL_M(aCPU.current_opc, rS, rA, rB, MB, ME);
    uint32 v = ppc_word_rotl(aCPU.gpr[rS], aCPU.gpr[rB]);
    uint32 mask = ppc_mask(MB, ME);
    aCPU.gpr[rA] = v & mask;
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rA]);
    }
}

/*
 *	slwx		Shift Left Word
 *	.625
 */
void ppc_opc_slwx(PPC_CPU_State &aCPU)
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
}

/*
 *	srawx		Shift Right Algebraic Word
 *	.628
 */
void ppc_opc_srawx(PPC_CPU_State &aCPU)
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
}

/*
 *	srawix		Shift Right Algebraic Word Immediate
 *	.629
 */
void ppc_opc_srawix(PPC_CPU_State &aCPU)
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
}

/*
 *	srwx		Shift Right Word
 *	.631
 */
void ppc_opc_srwx(PPC_CPU_State &aCPU)
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
}

/*
 *	subfx		Subtract From
 *	.666
 */
void ppc_opc_subfx(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    aCPU.gpr[rD] = ~aCPU.gpr[rA] + aCPU.gpr[rB] + 1;
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
}

/*
 *	subfox		Subtract From with Overflow
 *	.666
 */
void ppc_opc_subfox(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
    aCPU.gpr[rD] = ~aCPU.gpr[rA] + aCPU.gpr[rB] + 1;
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rD]);
    }
    // update XER flags
    PPC_ALU_ERR("subfox unimplemented\n");
}

/*
 *	subfcx		Subtract From Carrying
 *	.667
 */
void ppc_opc_subfcx(PPC_CPU_State &aCPU)
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
}

/*
 *	subfcox		Subtract From Carrying with Overflow
 *	.667
 */
void ppc_opc_subfcox(PPC_CPU_State &aCPU)
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
}

/*
 *	subfex		Subtract From Extended
 *	.668
 */
void ppc_opc_subfex(PPC_CPU_State &aCPU)
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
}

/*
 *	subfeox		Subtract From Extended with Overflow
 *	.668
 */
void ppc_opc_subfeox(PPC_CPU_State &aCPU)
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
}

/*
 *	subfic		Subtract From Immediate Carrying
 *	.669
 */
void ppc_opc_subfic(PPC_CPU_State &aCPU)
{
    int rD, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
    uint32 a = aCPU.gpr[rA];
    aCPU.gpr[rD] = ~a + imm + 1;
    aCPU.xer_ca = (ppc_carry_3(~a, imm, 1));
}

/*
 *	subfmex		Subtract From Minus One Extended
 *	.670
 */
void ppc_opc_subfmex(PPC_CPU_State &aCPU)
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
}

/*
 *	subfmeox	Subtract From Minus One Extended with Overflow
 *	.670
 */
void ppc_opc_subfmeox(PPC_CPU_State &aCPU)
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
}

/*
 *	subfzex		Subtract From Zero Extended
 *	.671
 */
void ppc_opc_subfzex(PPC_CPU_State &aCPU)
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
}

/*
 *	subfzeox	Subtract From Zero Extended with Overflow
 *	.671
 */
void ppc_opc_subfzeox(PPC_CPU_State &aCPU)
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
}

/*
 *	xorx		XOR
 *	.680
 */
void ppc_opc_xorx(PPC_CPU_State &aCPU)
{
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    aCPU.gpr[rA] = aCPU.gpr[rS] ^ aCPU.gpr[rB];
    if (aCPU.current_opc & PPC_OPC_Rc) {
        ppc_update_cr0(aCPU, aCPU.gpr[rA]);
    }
}

/*
 *	xori		XOR Immediate
 *	.681
 */
void ppc_opc_xori(PPC_CPU_State &aCPU)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_UImm(aCPU.current_opc, rS, rA, imm);
    aCPU.gpr[rA] = aCPU.gpr[rS] ^ imm;
}

/*
 *	xoris		XOR Immediate Shifted
 *	.682
 */
void ppc_opc_xoris(PPC_CPU_State &aCPU)
{
    int rS, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_Shift16(aCPU.current_opc, rS, rA, imm);
    aCPU.gpr[rA] = aCPU.gpr[rS] ^ imm;
}

/*
 *	bx		Branch
 *	.435
 */
void ppc_opc_bx(PPC_CPU_State &aCPU)
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
}

/*
 *	bcx		Branch Conditional
 *	.436
 */
void ppc_opc_bcx(PPC_CPU_State &aCPU)
{
    uint32 BO, BI, BD;
    PPC_OPC_TEMPL_B(aCPU.current_opc, BO, BI, BD);
    if (!(BO & 4)) {
        aCPU.ctr--;
    }
    bool bo2 = (BO & 2);
    bool bo8 = (BO & 8); // branch condition true
    bool cr = (aCPU.cr & (1 << (31 - BI)));
    if (((BO & 4) || ((aCPU.ctr != 0) ^ bo2)) && ((BO & 16) || (!(cr ^ bo8)))) {
        if (!(aCPU.current_opc & PPC_OPC_AA)) {
            BD += aCPU.pc;
        }
        if (aCPU.current_opc & PPC_OPC_LK) {
            aCPU.lr = aCPU.pc + 4;
        }
        aCPU.npc = BD;
    }
}

/*
 *	bcctrx		Branch Conditional to Count Register
 *	.438
 */
void ppc_opc_bcctrx(PPC_CPU_State &aCPU)
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
}

/*
 *	bclrx		Branch Conditional to Link Register
 *	.440
 */
void ppc_opc_bclrx(PPC_CPU_State &aCPU)
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
    }
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
void ppc_opc_eciwx(PPC_CPU_State &aCPU)
{
    PPC_OPC_ERR("eciwx unimplemented.\n");
}

/*
 *	ecowx		External Control Out Word Indexed
 *	.476
 */
void ppc_opc_ecowx(PPC_CPU_State &aCPU)
{
    PPC_OPC_ERR("ecowx unimplemented.\n");
}

/*
 *	eieio		Enforce In-Order Execution of I/O
 *	.478
 */
void ppc_opc_eieio(PPC_CPU_State &aCPU)
{
    // NO-OP
}

/*
 *	icbi		Instruction Cache Block Invalidate
 *	.519
 */
void ppc_opc_icbi(PPC_CPU_State &aCPU)
{
    int rA, rD, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
    uint32 ea = (rA ? aCPU.gpr[rA] : 0) + aCPU.gpr[rB];
    uint32 pa;
    if (ppc_effective_to_physical(aCPU, ea, PPC_MMU_READ | PPC_MMU_NO_EXC, pa) != PPC_MMU_OK) {
        return;
    }
    if (pa >= gMemorySize) return;
    if (!aCPU.jitc) return;
    if ((pa >> 12) == 0) {
        JITC &jitc2 = *aCPU.jitc;
        fprintf(stderr, "[ICBI] PA page 0! ea=%08x pa=%08x clientPage=%p\n",
            ea, pa, jitc2.clientPages[0]);
    }
    uint32 pageIndex = pa >> 12;
    JITC &jitc = *aCPU.jitc;
    ClientPage *cp = jitc.clientPages[pageIndex];
    if (cp) {
        jitcDestroyAndFreeClientPage(jitc, cp);
    }
}

/*
 *	isync		Instruction Synchronize
 *	.520
 */
void ppc_opc_isync(PPC_CPU_State &aCPU)
{
    // NO-OP
}

/*
 *	mcrf		Move Condition Register Field
 *	.561
 */
void ppc_opc_mcrf(PPC_CPU_State &aCPU)
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
}

/*
 *	mcrfs		Move to Condition Register from FPSCR
 *	.562
 */
void ppc_opc_mcrfs(PPC_CPU_State &aCPU)
{
    PPC_OPC_ERR("mcrfs unimplemented.\n");
}

/*
 *	mcrxr		Move to Condition Register from XER
 *	.563
 */
void ppc_opc_mcrxr(PPC_CPU_State &aCPU)
{
    int crD, a, b;
    PPC_OPC_TEMPL_X(aCPU.current_opc, crD, a, b);
    crD >>= 2;
    crD = 7 - crD;
    aCPU.cr &= ppc_cmp_and_mask[crD];
    aCPU.cr |= (((aCPU.xer & 0xf0000000) | (aCPU.xer_ca ? XER_CA : 0)) >> 28) << (crD * 4);
    aCPU.xer = ~0xf0000000;
    aCPU.xer_ca = 0;
}

/*
 *	mfcr		Move from Condition Register
 *	.564
 */
void ppc_opc_mfcr(PPC_CPU_State &aCPU)
{
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
    PPC_OPC_ASSERT(rA == 0 && rB == 0);
    aCPU.gpr[rD] = aCPU.cr;
}

/*
 *	mffs		Move from FPSCR
 *	.565
 */
void ppc_opc_mffsx(PPC_CPU_State &aCPU)
{
    int frD, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, frD, rA, rB);
    PPC_OPC_ASSERT(rA == 0 && rB == 0);
    aCPU.fpr[frD] = aCPU.fpscr;
    if (aCPU.current_opc & PPC_OPC_Rc) {
        // update cr1 flags
        PPC_OPC_ERR("mffs. unimplemented.\n");
    }
}

/*
 *	mfmsr		Move from Machine State Register
 *	.566
 */
void ppc_opc_mfmsr(PPC_CPU_State &aCPU)
{
    if (aCPU.msr & MSR_PR) {
        //		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
        return;
    }
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
    PPC_OPC_ASSERT((rA == 0) && (rB == 0));
    aCPU.gpr[rD] = aCPU.msr;
}

/*
 *	mfspr		Move from Special-Purpose Register
 *	.567
 */
void ppc_opc_mfspr(PPC_CPU_State &aCPU)
{
    int rD, spr1, spr2;
    PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, spr1, spr2);
    switch (spr2) {
    case 0:
        switch (spr1) {
        case 1: aCPU.gpr[rD] = aCPU.xer | (aCPU.xer_ca ? XER_CA : 0); return;
        case 8: aCPU.gpr[rD] = aCPU.lr; return;
        case 9: aCPU.gpr[rD] = aCPU.ctr; return;
        }
    case 8: // altivec makes this user visible
        if (spr1 == 0) {
            aCPU.gpr[rD] = aCPU.vrsave;
            return;
        }
    }
    if (aCPU.msr & MSR_PR) {
        //		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
        return;
    }
    switch (spr2) {
    case 0:
        switch (spr1) {
        case 18: aCPU.gpr[rD] = aCPU.dsisr; return;
        case 19: aCPU.gpr[rD] = aCPU.dar; return;
        case 22: aCPU.gpr[rD] = aCPU.dec; return;
        case 25: aCPU.gpr[rD] = aCPU.sdr1; return;
        case 26: aCPU.gpr[rD] = aCPU.srr[0]; return;
        case 27: aCPU.gpr[rD] = aCPU.srr[1]; return;
        }
        break;
    case 8:
        switch (spr1) {
        case 12: aCPU.gpr[rD] = ppc_get_cpu_timebase(); return;
        case 13: aCPU.gpr[rD] = ppc_get_cpu_timebase() >> 32; return;
        case 16: aCPU.gpr[rD] = aCPU.sprg[0]; return;
        case 17: aCPU.gpr[rD] = aCPU.sprg[1]; return;
        case 18: aCPU.gpr[rD] = aCPU.sprg[2]; return;
        case 19: aCPU.gpr[rD] = aCPU.sprg[3]; return;
        case 26: aCPU.gpr[rD] = aCPU.ear; return;
        case 31: aCPU.gpr[rD] = aCPU.pvr; return;
        }
        break;
    case 16:
        switch (spr1) {
        case 16: aCPU.gpr[rD] = aCPU.ibatu[0]; return;
        case 17: aCPU.gpr[rD] = aCPU.ibatl[0]; return;
        case 18: aCPU.gpr[rD] = aCPU.ibatu[1]; return;
        case 19: aCPU.gpr[rD] = aCPU.ibatl[1]; return;
        case 20: aCPU.gpr[rD] = aCPU.ibatu[2]; return;
        case 21: aCPU.gpr[rD] = aCPU.ibatl[2]; return;
        case 22: aCPU.gpr[rD] = aCPU.ibatu[3]; return;
        case 23: aCPU.gpr[rD] = aCPU.ibatl[3]; return;
        case 24: aCPU.gpr[rD] = aCPU.dbatu[0]; return;
        case 25: aCPU.gpr[rD] = aCPU.dbatl[0]; return;
        case 26: aCPU.gpr[rD] = aCPU.dbatu[1]; return;
        case 27: aCPU.gpr[rD] = aCPU.dbatl[1]; return;
        case 28: aCPU.gpr[rD] = aCPU.dbatu[2]; return;
        case 29: aCPU.gpr[rD] = aCPU.dbatl[2]; return;
        case 30: aCPU.gpr[rD] = aCPU.dbatu[3]; return;
        case 31: aCPU.gpr[rD] = aCPU.dbatl[3]; return;
        }
        break;
    case 29:
        switch (spr1) {
        case 16: aCPU.gpr[rD] = 0; return;
        case 17: aCPU.gpr[rD] = 0; return;
        case 18: aCPU.gpr[rD] = 0; return;
        case 24: aCPU.gpr[rD] = 0; return;
        case 25: aCPU.gpr[rD] = 0; return;
        case 26: aCPU.gpr[rD] = 0; return;
        case 28: aCPU.gpr[rD] = 0; return;
        case 29: aCPU.gpr[rD] = 0; return;
        case 30: aCPU.gpr[rD] = 0; return;
        }
    case 31:
        switch (spr1) {
        case 16:
            //			PPC_OPC_WARN("read from spr %d:%d (HID0) not supported!\n", spr1, spr2);
            aCPU.gpr[rD] = aCPU.hid[0];
            return;
        case 17:
            PPC_OPC_WARN("read from spr %d:%d (HID1) not supported!\n", spr1, spr2);
            aCPU.gpr[rD] = aCPU.hid[1];
            return;
        case 22: aCPU.gpr[rD] = 0; return;
        case 23: aCPU.gpr[rD] = 0; return;
        case 25:
            PPC_OPC_WARN("read from spr %d:%d (L2CR) not supported! (from %08x)\n", spr1, spr2, aCPU.pc);
            aCPU.gpr[rD] = 0;
            return;
        case 27:
            PPC_OPC_WARN("read from spr %d:%d (ICTC) not supported!\n", spr1, spr2);
            aCPU.gpr[rD] = 0;
            return;
        case 28:
            //			PPC_OPC_WARN("read from spr %d:%d (THRM1) not supported!\n", spr1, spr2);
            aCPU.gpr[rD] = 0;
            return;
        case 29:
            //			PPC_OPC_WARN("read from spr %d:%d (THRM2) not supported!\n", spr1, spr2);
            aCPU.gpr[rD] = 0;
            return;
        case 30:
            //			PPC_OPC_WARN("read from spr %d:%d (THRM3) not supported!\n", spr1, spr2);
            aCPU.gpr[rD] = 0;
            return;
        case 31:
            //			PPC_OPC_WARN("read from spr %d:%d (???) not supported!\n", spr1, spr2);
            aCPU.gpr[rD] = 0;
            return;
        }
    }
    fprintf(stderr, "unknown mfspr: %i:%i\n", spr1, spr2);
    SINGLESTEP("invalid mfspr\n");
}

/*
 *	mfsr		Move from Segment Register
 *	.570
 */
void ppc_opc_mfsr(PPC_CPU_State &aCPU)
{
    if (aCPU.msr & MSR_PR) {
        //		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
        return;
    }
    int rD, SR, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rD, SR, rB);
    // FIXME: check insn
    aCPU.gpr[rD] = aCPU.sr[SR & 0xf];
}

/*
 *	mfsrin		Move from Segment Register Indirect
 *	.572
 */
void ppc_opc_mfsrin(PPC_CPU_State &aCPU)
{
    if (aCPU.msr & MSR_PR) {
        //		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
        return;
    }
    int rD, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
    // FIXME: check insn
    aCPU.gpr[rD] = aCPU.sr[aCPU.gpr[rB] >> 28];
}

/*
 *	mftb		Move from Time Base
 *	.574
 */
void ppc_opc_mftb(PPC_CPU_State &aCPU)
{
    int rD, spr1, spr2;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rD, spr1, spr2);
    switch (spr2) {
    case 8:
        switch (spr1) {
        case 12: aCPU.gpr[rD] = ppc_get_cpu_timebase(); return;
        case 13:
            aCPU.gpr[rD] = ppc_get_cpu_timebase() >> 32;
            return;
            /*		case 12: aCPU.gpr[rD] = aCPU.tb; return;
		case 13: aCPU.gpr[rD] = aCPU.tb >> 32; return;*/
        }
        break;
    }
    SINGLESTEP("unknown mftb\n");
}

/*
 *	mtcrf		Move to Condition Register Fields
 *	.576
 */
void ppc_opc_mtcrf(PPC_CPU_State &aCPU)
{
    int rS;
    uint32 crm;
    uint32 CRM;
    PPC_OPC_TEMPL_XFX(aCPU.current_opc, rS, crm);
    CRM = ((crm & 0x80) ? 0xf0000000 : 0) | ((crm & 0x40) ? 0x0f000000 : 0) | ((crm & 0x20) ? 0x00f00000 : 0) |
          ((crm & 0x10) ? 0x000f0000 : 0) | ((crm & 0x08) ? 0x0000f000 : 0) | ((crm & 0x04) ? 0x00000f00 : 0) |
          ((crm & 0x02) ? 0x000000f0 : 0) | ((crm & 0x01) ? 0x0000000f : 0);
    aCPU.cr = (aCPU.gpr[rS] & CRM) | (aCPU.cr & ~CRM);
}

/*
 *	mtfsb0x		Move to FPSCR Bit 0
 *	.577
 */
void ppc_opc_mtfsb0x(PPC_CPU_State &aCPU)
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
}

/*
 *	mtfsb1x		Move to FPSCR Bit 1
 *	.578
 */
void ppc_opc_mtfsb1x(PPC_CPU_State &aCPU)
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
}

/*
 *	mtfsfx		Move to FPSCR Fields
 *	.579
 */
void ppc_opc_mtfsfx(PPC_CPU_State &aCPU)
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
}

/*
 *	mtfsfix		Move to FPSCR Field Immediate
 *	.580
 */
void ppc_opc_mtfsfix(PPC_CPU_State &aCPU)
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
}

/*
 *	mtmsr		Move to Machine State Register
 *	.581
 */
void ppc_opc_mtmsr(PPC_CPU_State &aCPU)
{
    if (aCPU.msr & MSR_PR) {
        //		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
        return;
    }
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    PPC_OPC_ASSERT((rA == 0) && (rB == 0));
    ppc_set_msr(aCPU, aCPU.gpr[rS]);
}

/*
 *	mtspr		Move to Special-Purpose Register
 *	.584
 */
void ppc_opc_mtspr(PPC_CPU_State &aCPU)
{
    int rS, spr1, spr2;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, spr1, spr2);
    switch (spr2) {
    case 0:
        switch (spr1) {
        case 1:
            aCPU.xer = aCPU.gpr[rS] & ~XER_CA;
            aCPU.xer_ca = !!(aCPU.gpr[rS] & XER_CA);
            return;
        case 8: aCPU.lr = aCPU.gpr[rS]; return;
        case 9: aCPU.ctr = aCPU.gpr[rS]; return;
        }
    case 8:
        if (spr1 == 0) {
            aCPU.vrsave = aCPU.gpr[rS];
            return;
        }
    }
    if (aCPU.msr & MSR_PR) {
        //		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
        return;
    }
    switch (spr2) {
    case 0:
        switch (spr1) {
            /*		case 18: aCPU.gpr[rD] = aCPU.dsisr; return;
		case 19: aCPU.gpr[rD] = aCPU.dar; return;*/
        case 22: {
            writeDEC(aCPU, aCPU.gpr[rS]);
            return;
        }
        case 25:
            if (!ppc_mmu_set_sdr1(aCPU, aCPU.gpr[rS], true)) {
                PPC_OPC_ERR("cannot set sdr1\n");
            }
            return;
        case 26: aCPU.srr[0] = aCPU.gpr[rS]; return;
        case 27: aCPU.srr[1] = aCPU.gpr[rS]; return;
        }
        break;
    case 8:
        switch (spr1) {
        case 16: aCPU.sprg[0] = aCPU.gpr[rS]; return;
        case 17: aCPU.sprg[1] = aCPU.gpr[rS]; return;
        case 18: aCPU.sprg[2] = aCPU.gpr[rS]; return;
        case 19: aCPU.sprg[3] = aCPU.gpr[rS]; return;
        case 28: writeTBL(aCPU, aCPU.gpr[rS]); return;
        case 29:
            writeTBU(aCPU, aCPU.gpr[rS]);
            return;
            /*		case 26: aCPU.gpr[rD] = aCPU.ear; return;
		case 31: aCPU.gpr[rD] = aCPU.pvr; return;*/
        }
        break;
    case 16:
        switch (spr1) {
        case 16:
            aCPU.ibatu[0] = aCPU.gpr[rS];
            ppc_opc_batu_helper(aCPU, false, 0);
            return;
        case 17:
            aCPU.ibatl[0] = aCPU.gpr[rS];
            ppc_opc_batl_helper(aCPU, false, 0);
            return;
        case 18:
            aCPU.ibatu[1] = aCPU.gpr[rS];
            ppc_opc_batu_helper(aCPU, false, 1);
            return;
        case 19:
            aCPU.ibatl[1] = aCPU.gpr[rS];
            ppc_opc_batl_helper(aCPU, false, 1);
            return;
        case 20:
            aCPU.ibatu[2] = aCPU.gpr[rS];
            ppc_opc_batu_helper(aCPU, false, 2);
            return;
        case 21:
            aCPU.ibatl[2] = aCPU.gpr[rS];
            ppc_opc_batl_helper(aCPU, false, 2);
            return;
        case 22:
            aCPU.ibatu[3] = aCPU.gpr[rS];
            ppc_opc_batu_helper(aCPU, false, 3);
            return;
        case 23:
            aCPU.ibatl[3] = aCPU.gpr[rS];
            ppc_opc_batl_helper(aCPU, false, 3);
            return;
        case 24:
            aCPU.dbatu[0] = aCPU.gpr[rS];
            ppc_opc_batu_helper(aCPU, true, 0);
            return;
        case 25:
            aCPU.dbatl[0] = aCPU.gpr[rS];
            ppc_opc_batl_helper(aCPU, true, 0);
            return;
        case 26:
            aCPU.dbatu[1] = aCPU.gpr[rS];
            ppc_opc_batu_helper(aCPU, true, 1);
            return;
        case 27:
            aCPU.dbatl[1] = aCPU.gpr[rS];
            ppc_opc_batl_helper(aCPU, true, 1);
            return;
        case 28:
            aCPU.dbatu[2] = aCPU.gpr[rS];
            ppc_opc_batu_helper(aCPU, true, 2);
            return;
        case 29:
            aCPU.dbatl[2] = aCPU.gpr[rS];
            ppc_opc_batl_helper(aCPU, true, 2);
            return;
        case 30:
            aCPU.dbatu[3] = aCPU.gpr[rS];
            ppc_opc_batu_helper(aCPU, true, 3);
            return;
        case 31:
            aCPU.dbatl[3] = aCPU.gpr[rS];
            ppc_opc_batl_helper(aCPU, true, 3);
            return;
        }
        break;
    case 29:
        switch (spr1) {
        case 17: return;
        case 24: return;
        case 25: return;
        case 26: return;
        }
    case 31:
        switch (spr1) {
        case 16:
            //			PPC_OPC_WARN("write(%08x) to spr %d:%d (HID0) not supported! @%08x\n", aCPU.gpr[rS], spr1, spr2, aCPU.pc);
            aCPU.hid[0] = aCPU.gpr[rS];
            return;
        case 17:
            PPC_OPC_WARN("write(%08x) to spr %d:%d (HID1) not supported! @%08x\n", aCPU.gpr[rS], spr1, spr2, aCPU.pc);
            aCPU.hid[1] = aCPU.gpr[rS];
            return;
        case 18: PPC_OPC_ERR("write(%08x) to spr %d:%d (IABR) not supported!\n", aCPU.gpr[rS], spr1, spr2); return;
        case 21: PPC_OPC_ERR("write(%08x) to spr %d:%d (DABR) not supported!\n", aCPU.gpr[rS], spr1, spr2); return;
        case 22: PPC_OPC_ERR("write(%08x) to spr %d:%d (?) not supported!\n", aCPU.gpr[rS], spr1, spr2); return;
        case 23: PPC_OPC_ERR("write(%08x) to spr %d:%d (?) not supported!\n", aCPU.gpr[rS], spr1, spr2); return;
        case 27: PPC_OPC_WARN("write(%08x) to spr %d:%d (ICTC) not supported!\n", aCPU.gpr[rS], spr1, spr2); return;
        case 28:
            //			PPC_OPC_WARN("write(%08x) to spr %d:%d (THRM1) not supported!\n", aCPU.gpr[rS], spr1, spr2);
            return;
        case 29:
            //			PPC_OPC_WARN("write(%08x) to spr %d:%d (THRM2) not supported!\n", aCPU.gpr[rS], spr1, spr2);
            return;
        case 30:
            //			PPC_OPC_WARN("write(%08x) to spr %d:%d (THRM3) not supported!\n", aCPU.gpr[rS], spr1, spr2);
            return;
        case 31: return;
        }
    }
    fprintf(stderr, "unknown mtspr: %i:%i\n", spr1, spr2);
    SINGLESTEP("unknown mtspr\n");
}

/*
 *	mtsr		Move to Segment Register
 *	.587
 */
void ppc_opc_mtsr(PPC_CPU_State &aCPU)
{
    if (aCPU.msr & MSR_PR) {
        //		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
        return;
    }
    int rS, SR, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, SR, rB);
    // FIXME: check insn
    aCPU.sr[SR & 0xf] = aCPU.gpr[rS];
}

/*
 *	mtsrin		Move to Segment Register Indirect
 *	.591
 */
void ppc_opc_mtsrin(PPC_CPU_State &aCPU)
{
    if (aCPU.msr & MSR_PR) {
        //		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
        return;
    }
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    // FIXME: check insn
    aCPU.sr[aCPU.gpr[rB] >> 28] = aCPU.gpr[rS];
}

/*
 *	rfi		Return from Interrupt
 *	.607
 */
void ppc_opc_rfi(PPC_CPU_State &aCPU)
{
    if (aCPU.msr & MSR_PR) {
        //		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
        return;
    }
    ppc_set_msr(aCPU, aCPU.srr[1] & MSR_RFI_SAVE_MASK);
    aCPU.npc = aCPU.srr[0] & 0xfffffffc;
}

void ppc_opc_sc(PPC_CPU_State &aCPU)
{
    if (aCPU.gpr[3] == 0x113724fa && aCPU.gpr[4] == 0x77810f9b) {
        gcard_osi(0);
        return;
    }
    //	ppc_exception(PPC_EXC_SC);
}

/*
 *	sync		Synchronize
 *	.672
 */
void ppc_opc_sync(PPC_CPU_State &aCPU)
{
    // NO-OP
}

/*
 *	tlbia		Translation Lookaside Buffer Invalidate All
 *	.676
 */
void ppc_opc_tlbia(PPC_CPU_State &aCPU)
{
    if (aCPU.msr & MSR_PR) {
        //		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
        return;
    }
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    // FIXME: check rS.. for 0
    ppc_mmu_tlb_invalidate(aCPU);
}

/*
 *	tlbie		Translation Lookaside Buffer Invalidate Entry
 *	.676
 */
void ppc_opc_tlbie(PPC_CPU_State &aCPU)
{
    if (aCPU.msr & MSR_PR) {
        //		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
        return;
    }
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    // FIXME: check rS.. for 0
    ppc_mmu_tlb_invalidate(aCPU);
}

/*
 *	tlbsync		Translation Lookaside Buffer Syncronize
 *	.677
 */
void ppc_opc_tlbsync(PPC_CPU_State &aCPU)
{
    if (aCPU.msr & MSR_PR) {
        //		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
        return;
    }
    int rS, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
    // FIXME: check rS.. for 0
}

/*
 *	tw		Trap Word
 *	.678
 */
void ppc_opc_tw(PPC_CPU_State &aCPU)
{
    int TO, rA, rB;
    PPC_OPC_TEMPL_X(aCPU.current_opc, TO, rA, rB);
    uint32 a = aCPU.gpr[rA];
    uint32 b = aCPU.gpr[rB];
    if (((TO & 16) && ((sint32)a < (sint32)b)) || ((TO & 8) && ((sint32)a > (sint32)b)) || ((TO & 4) && (a == b)) ||
        ((TO & 2) && (a < b)) || ((TO & 1) && (a > b))) {
        //		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_TRAP);
    }
}

/*
 *	twi		Trap Word Immediate
 *	.679
 */
void ppc_opc_twi(PPC_CPU_State &aCPU)
{
    int TO, rA;
    uint32 imm;
    PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, TO, rA, imm);
    uint32 a = aCPU.gpr[rA];
    if (((TO & 16) && ((sint32)a < (sint32)imm)) || ((TO & 8) && ((sint32)a > (sint32)imm)) ||
        ((TO & 4) && (a == imm)) || ((TO & 2) && (a < imm)) || ((TO & 1) && (a > imm))) {
        //		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_TRAP);
    }
}

/*      dcba	    Data Cache Block Allocate
 *      .???
 */
