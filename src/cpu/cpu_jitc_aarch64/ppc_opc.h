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

    // Per-instruction trace (disabled — causes SIGSEGV due to code size overflow)
    // jitc.asmMOV(X0, X20);
    // jitc.asmCALL((NativeAddress)ppc_opc_trace_insn);
}

static inline void ppc_opc_gen_interpret(JITC &jitc, int (*func)(PPC_CPU_State &))
{
    ppc_opc_gen_interpret_prologue(jitc);

    // X0 = &CPU state (X20)
    jitc.asmMOV(X0, X20);
    // Call interpreter function
    jitc.asmCALL((NativeAddress)func);

    // No exception check here. Non-load/store opcodes don't trigger DSI.
    // Async interrupts (DEC/ext) that set exception_pending are handled
    // by the heartbeat at the next page dispatch (ppc_new_pc_asm).
    // Opcodes that can trigger synchronous exceptions (sc, rfi, mtmsr,
    // load/store) use GEN_INTERPRET_ENDBLOCK, GEN_INTERPRET_BRANCH,
    // or GEN_INTERPRET_LOADSTORE instead.
}

/*
 * Wrapper for load/store interpreter functions that return int.
 * Unlike ppc_opc_gen_interpret(), this checks the RETURN VALUE (W0)
 * instead of exception_pending. This avoids a race where a concurrent
 * DEC/ext interrupt sets exception_pending during the BLR call, causing
 * the wrapper to consume the async exception instead of letting the
 * heartbeat handle it.
 *
 * The interpreter function returns 0 (PPC_MMU_OK) on success, nonzero
 * on DSI/ISI. When nonzero, ppc_exception() has already set up
 * SRR0/SRR1/npc and exception_pending.
 */
static inline void ppc_opc_gen_interpret_loadstore(JITC &jitc, int (*func)(PPC_CPU_State &))
{
    ppc_opc_gen_interpret_prologue(jitc);

    // X0 = &CPU state (X20)
    jitc.asmMOV(X0, X20);
    // Call interpreter function — returns int in W0
    jitc.asmCALL((NativeAddress)func);

    // Check return value: W0 != 0 means synchronous exception (DSI/ISI).
    // ppc_exception() already set SRR0/SRR1/npc and exception_pending.
    // Clear exception_pending and dispatch to npc.
    // If W0 == 0: no exception, continue normally.
    // Any pending DEC/ext stays in dec_exception/ext_exception and will
    // be handled by the heartbeat at the next page dispatch.
    //
    // Use CBNZ W0, #exception (short forward branch)
    // + B #skip (unconditional, ±128MB range, handles fragment boundaries).
    //
    // Reserve enough contiguous space so that no fragment boundary can
    // be crossed between excBranch and the exception path target.
    // NOP(4) + B(4) + STRBw(4) + LDRw(4) + asmBL(20) = 36 bytes.
    jitc.emitAssure(36);
    NativeAddress excBranch = jitc.asmHERE();
    jitc.asmNOP(); // placeholder for CBNZ W0, #exception
    NativeAddress skipBranch = jitc.asmJxxFixup(); // B #skip (placeholder)
    // Exception path: clear exception_pending, dispatch to npc
    NativeAddress excTarget = jitc.asmHERE();
    jitc.emit32(a64_STRBw(WZR, X20, offsetof(PPC_CPU_State, exception_pending)));
    jitc.asmLDRw_cpu(W0, offsetof(PPC_CPU_State, npc));
    jitc.asmCALL((NativeAddress)ppc_new_pc_asm);
    // Patch CBNZ W0, #exception (short forward branch to excTarget)
    *(uint32 *)excBranch = a64_CBNZw(W0, (sint32)(excTarget - excBranch));
    // Patch B #skip (unconditional branch past exception path, ±128MB range)
    jitc.asmResolveFixup(skipBranch);
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
