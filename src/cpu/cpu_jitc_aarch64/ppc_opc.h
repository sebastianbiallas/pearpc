/*
 *	PearPC
 *	ppc_opc.h
 *
 *	Copyright (C) 2003, 2004 Sebastian Biallas (sb@biallas.net)
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

#ifndef __PPC_OPC_H__
#define __PPC_OPC_H__

#include "system/types.h"
#include "jitc_types.h"
#include "jitc.h"
#include "jitc_asm.h"
#include "aarch64asm.h"
#include "jitc.h"

static inline void ppc_update_cr0(PPC_CPU_State &aCPU, uint32 r)
{
    aCPU.cr &= 0x0fffffff;
    if (!r) {
        aCPU.cr |= CR_CR0_EQ;
    } else if (r & 0x80000000) {
        aCPU.cr |= CR_CR0_LT;
    } else {
        aCPU.cr |= CR_CR0_GT;
    }
    if (aCPU.xer & XER_SO)
        aCPU.cr |= CR_CR0_SO;
}

/*
 *  Generate a call to a C++ interpreter function from JIT code.
 *
 *  Emits:
 *    MOV W16, #current_opc       ; store opcode so interpreter can decode it
 *    STR W16, [X20, #current_opc]
 *    MOV X0, X20                 ; first arg = pointer to PPC_CPU_State
 *    MOV X16, #func_addr         ; load function address
 *    BLR X16                     ; call it
 */
// Per-instruction trace: logs pc and opcode to jitc_insn.log
extern void ppc_opc_trace_insn(PPC_CPU_State &aCPU);

extern "C" void jitc_fatal_gpr9_corrupt(PPC_CPU_State *cpu);

static inline void ppc_opc_gen_interpret_prologue(JITC &jitc)
{
    jitc.clobberAll();
    // Store current opcode to CPU state
    jitc.asmMOV(W16, jitc.current_opc);
    jitc.asmSTRw_cpu(W16, offsetof(PPC_CPU_State, current_opc));

    // Store pc = current_code_base + pc_ofs (interpreter functions need this)
    jitc.asmLDRw_cpu(W16, offsetof(PPC_CPU_State, current_code_base));
    jitc.asmMOV(W17, jitc.pc);
    jitc.asmADDw(W16, W16, W17);
    jitc.asmSTRw_cpu(W16, offsetof(PPC_CPU_State, pc));

    // Store npc = pc + 4
    jitc.asmADDw(W16, W16, (uint32)4);
    jitc.asmSTRw_cpu(W16, offsetof(PPC_CPU_State, npc));
}

static inline void ppc_opc_gen_interpret(JITC &jitc, int (*func)(PPC_CPU_State &))
{
    ppc_opc_gen_interpret_prologue(jitc);

    // X0 = &CPU state (X20)
    jitc.asmMOV(X0, X20);
    // Call interpreter function
    jitc.asmCALL((NativeAddress)func);

    // No exception check here. Non-load/store opcodes don't trigger DSI.
    // Async interrupts (DEC/ext) are handled by the heartbeat at the
    // next page dispatch (ppc_new_pc_asm).
    // Opcodes that can trigger synchronous exceptions (sc, rfi, mtmsr,
    // load/store) use GEN_INTERPRET_ENDBLOCK, GEN_INTERPRET_BRANCH,
    // or GEN_INTERPRET_LOADSTORE instead.
}

/*
 * Wrapper for load/store interpreter functions that return int.
 *
 * The interpreter function returns 0 (PPC_MMU_OK) on success, nonzero
 * on DSI/ISI. When nonzero, ppc_exception() has already set up
 * SRR0/SRR1/npc. We dispatch to npc on exception, or fall through
 * on success.
 */
static inline void ppc_opc_gen_interpret_loadstore(JITC &jitc, int (*func)(PPC_CPU_State &))
{
    ppc_opc_gen_interpret_prologue(jitc);

    // X0 = &CPU state (X20)
    jitc.asmMOV(X0, X20);
    // Call interpreter function — returns int in W0
    jitc.asmCALL((NativeAddress)func);

    // W0 != 0 means synchronous exception (DSI/ISI).
    // ppc_exception() already set SRR0/SRR1/npc.
    // W0 == 0: no exception, continue normally.
    //
    // Layout: CBNZ +8 | B skip | LDR npc | BL ppc_new_pc_asm
    uint call_size = a64_bl_size((uint64)ppc_new_pc_asm);
    uint exc_path = 4 + call_size;  // LDR + CALL
    jitc.emitAssure(4 + 4 + exc_path);  // CBNZ + B + exc_path

    jitc.emit32(a64_CBNZw(W0, 8));     // CBNZ W0, +8 (skip B, land on exc path)
    NativeAddress target = jitc.asmHERE() + 4 + exc_path;
    jitc.asmBForward(exc_path);         // B skip exception path

    // Exception path: dispatch to npc
    jitc.asmLDRw_cpu(W0, offsetof(PPC_CPU_State, npc));
    jitc.asmCALL((NativeAddress)ppc_new_pc_asm);

    jitc.asmAssertHERE(target, "interpret_loadstore");
}


int ppc_opc_bx(PPC_CPU_State &aCPU);
int ppc_opc_bcx(PPC_CPU_State &aCPU);
int ppc_opc_bcctrx(PPC_CPU_State &aCPU);
int ppc_opc_bclrx(PPC_CPU_State &aCPU);

int ppc_opc_dcba(PPC_CPU_State &aCPU);
int ppc_opc_dcbf(PPC_CPU_State &aCPU);
int ppc_opc_dcbi(PPC_CPU_State &aCPU);
int ppc_opc_dcbst(PPC_CPU_State &aCPU);
int ppc_opc_dcbt(PPC_CPU_State &aCPU);
int ppc_opc_dcbtst(PPC_CPU_State &aCPU);

int ppc_opc_eciwx(PPC_CPU_State &aCPU);
int ppc_opc_ecowx(PPC_CPU_State &aCPU);
int ppc_opc_eieio(PPC_CPU_State &aCPU);

int ppc_opc_icbi(PPC_CPU_State &aCPU);
int ppc_opc_isync(PPC_CPU_State &aCPU);

int ppc_opc_mcrf(PPC_CPU_State &aCPU);
int ppc_opc_mcrfs(PPC_CPU_State &aCPU);
int ppc_opc_mcrxr(PPC_CPU_State &aCPU);
int ppc_opc_mfcr(PPC_CPU_State &aCPU);
int ppc_opc_mffsx(PPC_CPU_State &aCPU);
int ppc_opc_mfmsr(PPC_CPU_State &aCPU);
int ppc_opc_mfspr(PPC_CPU_State &aCPU);
int ppc_opc_mfsr(PPC_CPU_State &aCPU);
int ppc_opc_mfsrin(PPC_CPU_State &aCPU);
int ppc_opc_mftb(PPC_CPU_State &aCPU);
int ppc_opc_mtcrf(PPC_CPU_State &aCPU);
int ppc_opc_mtfsb0x(PPC_CPU_State &aCPU);
int ppc_opc_mtfsb1x(PPC_CPU_State &aCPU);
int ppc_opc_mtfsfx(PPC_CPU_State &aCPU);
int ppc_opc_mtfsfix(PPC_CPU_State &aCPU);
int ppc_opc_mtmsr(PPC_CPU_State &aCPU);
int ppc_opc_mtspr(PPC_CPU_State &aCPU);
int ppc_opc_mtsr(PPC_CPU_State &aCPU);
int ppc_opc_mtsrin(PPC_CPU_State &aCPU);

int ppc_opc_rfi(PPC_CPU_State &aCPU);
int ppc_opc_sc(PPC_CPU_State &aCPU);
int ppc_opc_sync(PPC_CPU_State &aCPU);
int ppc_opc_tlbia(PPC_CPU_State &aCPU);
int ppc_opc_tlbie(PPC_CPU_State &aCPU);
int ppc_opc_tlbsync(PPC_CPU_State &aCPU);
int ppc_opc_tw(PPC_CPU_State &aCPU);
int ppc_opc_twi(PPC_CPU_State &aCPU);

/* Native ALU codegen (ppc_alu.cc) */
JITCFlow ppc_opc_gen_addi(JITC &aJITC);
JITCFlow ppc_opc_gen_addis(JITC &aJITC);
JITCFlow ppc_opc_gen_ori(JITC &aJITC);
JITCFlow ppc_opc_gen_oris(JITC &aJITC);
JITCFlow ppc_opc_gen_xori(JITC &aJITC);
JITCFlow ppc_opc_gen_xoris(JITC &aJITC);
JITCFlow ppc_opc_gen_cmpi(JITC &aJITC);
JITCFlow ppc_opc_gen_cmp(JITC &aJITC);
JITCFlow ppc_opc_gen_cmpl(JITC &aJITC);
JITCFlow ppc_opc_gen_cmpli(JITC &aJITC);
JITCFlow ppc_opc_gen_addx(JITC &aJITC);
JITCFlow ppc_opc_gen_subfx(JITC &aJITC);
JITCFlow ppc_opc_gen_andx(JITC &aJITC);
JITCFlow ppc_opc_gen_orx(JITC &aJITC);
JITCFlow ppc_opc_gen_xorx(JITC &aJITC);
JITCFlow ppc_opc_gen_negx(JITC &aJITC);
JITCFlow ppc_opc_gen_mullwx(JITC &aJITC);
JITCFlow ppc_opc_gen_slwx(JITC &aJITC);
JITCFlow ppc_opc_gen_srwx(JITC &aJITC);
JITCFlow ppc_opc_gen_rlwinmx(JITC &aJITC);
JITCFlow ppc_opc_gen_rlwnmx(JITC &aJITC);
JITCFlow ppc_opc_gen_andi_(JITC &aJITC);
JITCFlow ppc_opc_gen_mtspr(JITC &aJITC);
JITCFlow ppc_opc_gen_mfspr(JITC &aJITC);

JITCFlow ppc_opc_gen_bx(JITC &aJITC);
JITCFlow ppc_opc_gen_bcx(JITC &aJITC);
JITCFlow ppc_opc_gen_bcctrx(JITC &aJITC);
JITCFlow ppc_opc_gen_bclrx(JITC &aJITC);

JITCFlow ppc_opc_gen_dcba(JITC &aJITC);
JITCFlow ppc_opc_gen_dcbf(JITC &aJITC);
JITCFlow ppc_opc_gen_dcbi(JITC &aJITC);
JITCFlow ppc_opc_gen_dcbst(JITC &aJITC);
JITCFlow ppc_opc_gen_dcbt(JITC &aJITC);
JITCFlow ppc_opc_gen_dcbtst(JITC &aJITC);

JITCFlow ppc_opc_gen_eciwx(JITC &aJITC);
JITCFlow ppc_opc_gen_ecowx(JITC &aJITC);
JITCFlow ppc_opc_gen_eieio(JITC &aJITC);

JITCFlow ppc_opc_gen_icbi(JITC &aJITC);
JITCFlow ppc_opc_gen_isync(JITC &aJITC);

JITCFlow ppc_opc_gen_mcrf(JITC &aJITC);
JITCFlow ppc_opc_gen_mcrfs(JITC &aJITC);
JITCFlow ppc_opc_gen_mcrxr(JITC &aJITC);
JITCFlow ppc_opc_gen_mfcr(JITC &aJITC);
JITCFlow ppc_opc_gen_mffsx(JITC &aJITC);
JITCFlow ppc_opc_gen_mfmsr(JITC &aJITC);
JITCFlow ppc_opc_gen_mfspr(JITC &aJITC);
JITCFlow ppc_opc_gen_mfsr(JITC &aJITC);
JITCFlow ppc_opc_gen_mfsrin(JITC &aJITC);
JITCFlow ppc_opc_gen_mftb(JITC &aJITC);
JITCFlow ppc_opc_gen_mtcrf(JITC &aJITC);
JITCFlow ppc_opc_gen_mtfsb0x(JITC &aJITC);
JITCFlow ppc_opc_gen_mtfsb1x(JITC &aJITC);
JITCFlow ppc_opc_gen_mtfsfx(JITC &aJITC);
JITCFlow ppc_opc_gen_mtfsfix(JITC &aJITC);
JITCFlow ppc_opc_gen_mtmsr(JITC &aJITC);
JITCFlow ppc_opc_gen_mtspr(JITC &aJITC);
JITCFlow ppc_opc_gen_mtsr(JITC &aJITC);
JITCFlow ppc_opc_gen_mtsrin(JITC &aJITC);

JITCFlow ppc_opc_gen_rfi(JITC &aJITC);
JITCFlow ppc_opc_gen_sc(JITC &aJITC);
JITCFlow ppc_opc_gen_sync(JITC &aJITC);
JITCFlow ppc_opc_gen_tlbia(JITC &aJITC);
JITCFlow ppc_opc_gen_tlbie(JITC &aJITC);
JITCFlow ppc_opc_gen_tlbsync(JITC &aJITC);
JITCFlow ppc_opc_gen_tw(JITC &aJITC);
JITCFlow ppc_opc_gen_twi(JITC &aJITC);


#endif
