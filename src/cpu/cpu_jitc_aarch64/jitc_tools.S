/*
 *  PearPC
 *  jitc_tools.S
 *
 *  AArch64 assembly bootstrap for JITC
 *
 *  Copyright (C) 2026 AArch64 port
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

#ifndef PREFIX
#define PREFIX
#endif

#define GLUE(a, b) GLUE2(a, b)
#define GLUE2(a, b) a##b
#define EXTERN(sym) GLUE(PREFIX, sym)

.macro do_export name
    .globl \name
\name:
.endm

#include "jitc_common.h"

/*
 * Register conventions:
 *   X20 = pointer to PPC_CPU_State (preserved across calls)
 *   X19 = pointer to JITC (preserved across calls)
 *   W0  = used for passing effective PC values
 *
 * AArch64 calling convention:
 *   X0-X7   = arguments / return values
 *   X8      = indirect result location
 *   X9-X15  = temporaries (caller-saved)
 *   X16-X17 = intra-procedure-call temporaries
 *   X18     = platform register (reserved on macOS)
 *   X19-X28 = callee-saved
 *   X29     = frame pointer
 *   X30     = link register
 *   SP      = stack pointer
 */

.text

//////////////////////////////////////////////////////////////////////////////////////////////
//
//  ppc_start_jitc_asm
//
//  IN: w0 = newpc (effective address)
//      x1 = &gCPU (pointer to PPC_CPU_State*)
//      w2 = sizeof(PPC_CPU_State)  (unused on aarch64, CPU stays in place)
//
.p2align 4
do_export EXTERN(ppc_start_jitc_asm)
    // Save callee-saved registers
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    stp x19, x20, [sp, #-16]!
    stp x21, x22, [sp, #-16]!
    stp x23, x24, [sp, #-16]!
    stp x25, x26, [sp, #-16]!
    stp x27, x28, [sp, #-16]!

    // Save the initial PC
    mov w21, w0

    // X20 = *cpu (the PPC_CPU_State pointer)
    ldr x20, [x1]

    // X19 = cpu->jitc
    ldr x19, [x20, #jitc]

    // Invalidate all TLB entries
    mov x0, x20
    bl  EXTERN(ppc_mmu_tlb_invalidate_all_asm)

    // Jump to ppc_new_pc_asm with initial PC
    mov w0, w21
    b   EXTERN(ppc_new_pc_asm)


//////////////////////////////////////////////////////////////////////////////////////////////
//
//  ppc_new_pc_asm
//
//  IN: w0 = new client pc (effective address)
//
//  Does not return to caller (tail-calls into translated code)
//
.p2align 4
do_export EXTERN(ppc_new_pc_asm)
    // Call heartbeat (checks exceptions, sets current_code_base)
    // w0 = effective PC on entry and preserved through heartbeat
    bl  EXTERN(ppc_heartbeat_ext_asm)

    // Store effective PC for validation
    str w0, [x20, #pc]

    // Save effective PC in w21 (callee-saved) for ISI handler
    mov     w21, w0

    // Translate effective address to physical
    // w0 = effective address, w1 = 0 (bytes to unwind, not used on aarch64)
    mov w1, #0
    bl  EXTERN(ppc_effective_to_physical_code)

    // Check for ISI exception
    ldrb    w16, [x20, #exception_pending]
    tbnz    w16, #0, .Lnewpc_isi

    // Call jitcNewPC(jitc, physical_addr)
    // x0 = jitc, w1 = physical address from translation
    mov w1, w0
    mov x0, x19
    bl  EXTERN(jitcNewPC)

    // Jump to translated code
    br  x0

.Lnewpc_isi:
    // ISI exception — ppc_exception() already set up SRR0/SRR1,
    // cleared MSR, invalidated TLB, and set npc = exception vector.
    // Just clear exception_pending and dispatch to npc.
    strb    wzr, [x20, #exception_pending]
    ldr     w0, [x20, #npc]
    b       EXTERN(ppc_new_pc_asm)


//////////////////////////////////////////////////////////////////////////////////////////////
//
//  ppc_new_pc_rel_asm
//
//  IN: w0 = new client pc relative to current_code_base
//
//  Does not return to caller
//
.p2align 4
do_export EXTERN(ppc_new_pc_rel_asm)
    // Add current_code_base to get effective address
    ldr w1, [x20, #current_code_base]
    add w0, w0, w1
    b   EXTERN(ppc_new_pc_asm)


//////////////////////////////////////////////////////////////////////////////////////////////
//
//  ppc_new_pc_this_page_asm
//
//  IN: w0 = pc offset relative to current_code_base
//
//  This is called from translated code (frame 1).
//
.p2align 4
do_export EXTERN(ppc_new_pc_this_page_asm)
    // IN: w0 = pc offset within current page (0..4092)
    //
    // Fast same-page dispatch: translate EA to PA, look up the entrypoint
    // directly from the ClientPage array, and jump to it.
    // Falls back to jitcNewPC if entrypoint not found.

    // Save offset for later
    mov     w21, w0

    // EA = current_code_base + offset
    ldr     w1, [x20, #current_code_base]
    add     w0, w0, w1

    // Translate EA to PA
    mov     w1, #0
    bl      EXTERN(ppc_effective_to_physical_code)
    // w0 = physical address

    // Look up ClientPage: clientPages[PA >> 12]
    // X19 = JITC pointer, clientPages is at offset 0
    ldr     x1, [x19, #clientPages]     // x1 = clientPages array
    lsr     w2, w0, #12                 // w2 = page index
    ldr     x3, [x1, x2, lsl #3]       // x3 = ClientPage pointer
    cbz     x3, .Lthispage_fallback     // no ClientPage? fallback

    // Look up entrypoint: entrypoints[(PA & 0xFFF) >> 2]
    and     w4, w0, #0xFFF
    lsr     w4, w4, #2                  // w4 = instruction index (0..1023)
    ldr     x5, [x3, x4, lsl #3]       // x5 = native entry point
    cbz     x5, .Lthispage_fallback     // not translated? fallback

    // Direct jump to translated code
    br      x5

.Lthispage_fallback:
    // Fall back to full jitcNewPC dispatch
    mov     w1, w0
    mov     x0, x19
    bl      EXTERN(jitcNewPC)
    br      x0


//////////////////////////////////////////////////////////////////////////////////////////////
//
//  ppc_heartbeat_ext_asm
//
//  IN: w0 = new effective PC
//  OUT: w0 = same effective PC (preserved)
//
//  Checks for pending exceptions. If none, returns.
//  If stop_exception: jumps to ppc_stop_jitc_asm.
//  If ext/dec exception and MSR_EE: jumps to handler.
//
.p2align 4
do_export EXTERN(ppc_heartbeat_ext_asm)
    // Store current_code_base = w0 & 0xFFFFF000
    and w1, w0, #0xFFFFF000
    str w1, [x20, #current_code_base]

    // Check exception_pending byte
    ldrb    w2, [x20, #exception_pending]
    tst w2, #1
    b.ne    .Lheartbeat_exception
    ret

.Lheartbeat_exception:
    // Check stop_exception
    ldrb    w2, [x20, #stop_exception]
    tst w2, #1
    b.ne    .Lheartbeat_stop

    // Check MSR_EE (bit 15 of msr => byte offset msr+1, bit 7)
    ldrh    w3, [x20, #msr]
    tst w3, #(1 << 15)
    b.eq    .Lheartbeat_no_exception

    // Check ext_exception
    ldrb    w2, [x20, #ext_exception]
    tst w2, #1
    b.eq    1f
    b   EXTERN(ppc_ext_exception_asm)
1:
    // Check dec_exception
    ldrb    w2, [x20, #dec_exception]
    tst w2, #1
    b.eq    2f
    b   EXTERN(ppc_dec_exception_asm)
2:
    // Check for DSI (page fault) — raised by TLB slow path
    ldr     w2, [x20, #dsisr]
    cbnz    w2, .Lheartbeat_dsi
    // Unknown exception
    b   EXTERN(jitc_error_unknown_exception)

.Lheartbeat_dsi:
    // DSI exception: DAR and DSISR already set by slow path.
    // Clear exception_pending flag.
    strb    wzr, [x20, #exception_pending]
    // Load DAR and DSISR for the handler
    ldr     w0, [x20, #dar]
    ldr     w1, [x20, #dsisr]
    // Clear DSISR so we don't re-trigger
    str     wzr, [x20, #dsisr]
    b   EXTERN(ppc_dsi_exception_asm)

.Lheartbeat_no_exception:
    ret

.Lheartbeat_stop:
    b   ppc_stop_jitc_asm


//////////////////////////////////////////////////////////////////////////////////////////////
//
//  ppc_heartbeat_ext_rel_asm
//
//  IN: w0 = pc offset relative to current_code_base
//
.p2align 4
do_export EXTERN(ppc_heartbeat_ext_rel_asm)
    // Check exception_pending byte
    ldrb    w2, [x20, #exception_pending]
    tst w2, #1
    b.ne    .Lheartbeat_rel_exception
    ret

.Lheartbeat_rel_exception:
    // Check stop_exception
    ldrb    w2, [x20, #stop_exception]
    tst w2, #1
    b.ne    .Lheartbeat_rel_stop

    // Check MSR_EE
    ldrh    w3, [x20, #msr]
    tst w3, #(1 << 15)
    b.eq    .Lheartbeat_rel_no_exception

    // Compute full effective PC
    ldr w1, [x20, #current_code_base]
    add w0, w0, w1

    // Check ext_exception
    ldrb    w2, [x20, #ext_exception]
    tst w2, #1
    b.eq    3f
    b   EXTERN(ppc_ext_exception_asm)
3:
    // Check dec_exception
    ldrb    w2, [x20, #dec_exception]
    tst w2, #1
    b.eq    4f
    b   EXTERN(ppc_dec_exception_asm)
4:
    // Check for DSI
    ldr     w2, [x20, #dsisr]
    cbnz    w2, .Lheartbeat_rel_dsi
    b   EXTERN(jitc_error_unknown_exception)

.Lheartbeat_rel_dsi:
    strb    wzr, [x20, #exception_pending]
    ldr     w0, [x20, #dar]
    ldr     w1, [x20, #dsisr]
    str     wzr, [x20, #dsisr]
    b   EXTERN(ppc_dsi_exception_asm)

.Lheartbeat_rel_no_exception:
    ret

.Lheartbeat_rel_stop:
    b   ppc_stop_jitc_asm


//////////////////////////////////////////////////////////////////////////////////////////////
//
//  ppc_stop_jitc_asm
//
//  Restore callee-saved registers and return to C++ (ppc_cpu_run)
//
.p2align 4
ppc_stop_jitc_asm:
    ldp x27, x28, [sp], #16
    ldp x25, x26, [sp], #16
    ldp x23, x24, [sp], #16
    ldp x21, x22, [sp], #16
    ldp x19, x20, [sp], #16
    ldp x29, x30, [sp], #16
    ret


//////////////////////////////////////////////////////////////////////////////////////////////
//
//  Exception handlers
//
//  These store SRR0/SRR1, clear MSR, invalidate TLB, and jump to
//  the exception vector via ppc_new_pc_asm.
//

.macro exception_epilogue entry
    // Clear MSR
    str wzr, [x20, #msr]
    // Clear current_code_base
    str wzr, [x20, #current_code_base]
    // Invalidate TLB
    mov x0, x20
    bl  EXTERN(ppc_mmu_tlb_invalidate_all_asm)
    // Load JITC pointer
    ldr x19, [x20, #jitc]
    // Jump to exception vector
    mov w0, #\entry
    mov w1, #0
    bl  EXTERN(ppc_effective_to_physical_code)
    mov w1, w0
    mov x0, x19
    bl  EXTERN(jitcNewPC)
    br  x0
.endm

//////////////////////////////////////////////////////////////////////////////////////////////
//  ppc_dsi_exception_asm
//
//  IN: w0 = fault addr, w1 = dsisr bits
//
.p2align 4
do_export EXTERN(ppc_dsi_exception_asm)
    str w0, [x20, #dar]
    str w1, [x20, #dsisr]
do_export EXTERN(ppc_dsi_exception_special_asm)
    ldr w2, [x20, #pc_ofs]
    ldr w3, [x20, #msr]
    ldr w4, [x20, #current_code_base]
    add w2, w2, w4
    mov w16, #0xffff
    movk    w16, #0x87c0, lsl #16
    and w3, w3, w16
    str w3, [x20, #srr1]
    str w2, [x20, #srr0]
    exception_epilogue 0x300

//////////////////////////////////////////////////////////////////////////////////////////////
//  ppc_isi_exception_asm
//
//  IN: w0 = fault addr, w1 = srr1 bits
//
.p2align 4
do_export EXTERN(ppc_isi_exception_asm)
    str w0, [x20, #srr0]
    ldr w3, [x20, #msr]
    mov w16, #0xffff
    movk    w16, #0x87c0, lsl #16
    and w3, w3, w16
    orr w3, w3, w1
    str w3, [x20, #srr1]
    exception_epilogue 0x400

//////////////////////////////////////////////////////////////////////////////////////////////
//  ppc_ext_exception_asm
//
//  IN: w0 = current effective PC
//
.p2align 4
do_export EXTERN(ppc_ext_exception_asm)
    str w0, [x20, #srr0]
    ldr w3, [x20, #msr]
    mov w16, #0xffff
    movk    w16, #0x87c0, lsl #16
    and w3, w3, w16
    str w3, [x20, #srr1]
    exception_epilogue 0x500

//////////////////////////////////////////////////////////////////////////////////////////////
//  ppc_dec_exception_asm (internal)
//
//  IN: w0 = current effective PC
//
.p2align 4
do_export EXTERN(ppc_dec_exception_asm)
    str w0, [x20, #srr0]
    ldr w3, [x20, #msr]
    mov w16, #0xffff
    movk    w16, #0x87c0, lsl #16
    and w3, w3, w16
    str w3, [x20, #srr1]
    exception_epilogue 0x900

//////////////////////////////////////////////////////////////////////////////////////////////
//  ppc_program_exception_asm
//
//  IN: w0 = pc_ofs, w1 = srr1 bits (exception type)
//
.p2align 4
do_export EXTERN(ppc_program_exception_asm)
    str w0, [x20, #pc_ofs]
    ldr w3, [x20, #msr]
    ldr w4, [x20, #current_code_base]
    add w2, w0, w4
    mov w16, #0xffff
    movk    w16, #0x87c0, lsl #16
    and w3, w3, w16
    orr w3, w3, w1
    str w2, [x20, #srr0]
    str w3, [x20, #srr1]
    exception_epilogue 0x700

//////////////////////////////////////////////////////////////////////////////////////////////
//  ppc_no_fpu_exception_asm
//
//  IN: w0 = pc_ofs
//
.p2align 4
do_export EXTERN(ppc_no_fpu_exception_asm)
    str w0, [x20, #pc_ofs]
    ldr w3, [x20, #msr]
    ldr w4, [x20, #current_code_base]
    add w2, w0, w4
    mov w16, #0xffff
    movk    w16, #0x87c0, lsl #16
    and w3, w3, w16
    str w2, [x20, #srr0]
    str w3, [x20, #srr1]
    exception_epilogue 0x800

//////////////////////////////////////////////////////////////////////////////////////////////
//  ppc_no_vec_exception_asm
//
//  IN: w0 = pc_ofs
//
.p2align 4
do_export EXTERN(ppc_no_vec_exception_asm)
    str w0, [x20, #pc_ofs]
    ldr w3, [x20, #msr]
    ldr w4, [x20, #current_code_base]
    add w2, w0, w4
    mov w16, #0xffff
    movk    w16, #0x87c0, lsl #16
    and w3, w3, w16
    str w2, [x20, #srr0]
    str w3, [x20, #srr1]
    exception_epilogue 0xf20

//////////////////////////////////////////////////////////////////////////////////////////////
//  ppc_sc_exception_asm
//
//  IN: w0 = pc_ofs
//
.p2align 4
do_export EXTERN(ppc_sc_exception_asm)
    str w0, [x20, #pc_ofs]
    ldr w3, [x20, #msr]
    ldr w4, [x20, #current_code_base]
    add w2, w0, w4
    mov w16, #0xffff
    movk    w16, #0x87c0, lsl #16
    and w3, w3, w16
    str w2, [x20, #srr0]
    str w3, [x20, #srr1]
    exception_epilogue 0xc00


//////////////////////////////////////////////////////////////////////////////////////////////
//
//  ppc_flush_flags_asm (stub)
//  TODO: implement proper CR0 flag flushing for aarch64
//
.p2align 4
do_export EXTERN(ppc_flush_flags_asm)
    ret

.p2align 4
do_export EXTERN(ppc_flush_flags_signed_0_asm)
    ret

.p2align 4
do_export EXTERN(ppc_flush_flags_unsigned_0_asm)
    ret

.p2align 4
do_export EXTERN(ppc_flush_flags_signed_even_asm)
    ret

.p2align 4
do_export EXTERN(ppc_flush_flags_unsigned_even_asm)
    ret

.p2align 4
do_export EXTERN(ppc_flush_flags_signed_odd_asm)
    ret

.p2align 4
do_export EXTERN(ppc_flush_flags_unsigned_odd_asm)
    ret


//////////////////////////////////////////////////////////////////////////////////////////////
//
//  ppc_set_msr_asm (stub)
//
//  IN: w0 = new msr value
//
.p2align 4
do_export EXTERN(ppc_set_msr_asm)
    str w0, [x20, #msr]
    ret


//////////////////////////////////////////////////////////////////////////////////////////////
//
//  ppc_mmu_tlb_invalidate_all_asm
//
//  IN: x0 = pointer to PPC_CPU_State
//
//  Zeros all TLB entries.
//
.p2align 4
do_export EXTERN(ppc_mmu_tlb_invalidate_all_asm)
    // Fill all TLB eff entries with 0xFFFFFFFF (guaranteed TLB miss).
    // Valid page tags have bits 0-11 clear, so 0xFFFFFFFF never matches.
    // The three eff arrays (code, data_read, data_write) are contiguous.
    add     x1, x0, #tlb_code_0_eff
    mvn     w2, wzr                     // w2 = 0xFFFFFFFF
    mov     w3, #(TLB_ENTRIES * 3)      // 3 arrays × 32 entries
1:
    str     w2, [x1], #4
    subs    w3, w3, #1
    b.ne    1b
    ret


//////////////////////////////////////////////////////////////////////////////////////////////
//
//  Atomic exception raising functions (called from C)
//
//  IN: x0 = pointer to PPC_CPU_State
//

.p2align 4
do_export EXTERN(ppc_cpu_atomic_raise_dec_exception)
    add x1, x0, #exception_pending
1:
    ldxr    w2, [x1]
    mov w16, #0x0101
    orr w2, w2, w16
    stxr    w3, w2, [x1]
    cbnz    w3, 1b
    ret

.p2align 4
do_export EXTERN(ppc_cpu_atomic_raise_ext_exception)
    add x1, x0, #exception_pending
1:
    ldxr    w2, [x1]
    mov w16, #0x0001
    movk    w16, #0x0001, lsl #16
    orr w2, w2, w16
    stxr    w3, w2, [x1]
    cbnz    w3, 1b
    ret

.p2align 4
do_export EXTERN(ppc_cpu_atomic_raise_stop_exception)
    add x1, x0, #exception_pending
1:
    ldxr    w2, [x1]
    mov w16, #0x0001
    movk    w16, #0x0100, lsl #16
    orr w2, w2, w16
    stxr    w3, w2, [x1]
    cbnz    w3, 1b
    ret

.p2align 4
do_export EXTERN(ppc_cpu_atomic_cancel_ext_exception)
    add x1, x0, #exception_pending
1:
    ldxr    w2, [x1]
    // Keep only dec_exception bit and pending flag if dec is set
    tst w2, #0x00000100
    mov w3, #0
    cset    w3, ne
    orr w3, w3, w3, lsl #8      // if dec: 0x00000101, else 0x00000000
    stxr    w4, w3, [x1]
    cbnz    w4, 1b
    ret


//////////////////////////////////////////////////////////////////////////////////////////////
//
//  ppc_mmu_tlb_invalidate_entry_asm (stub)
//
.p2align 4
do_export EXTERN(ppc_mmu_tlb_invalidate_entry_asm)
    // IN: w0 = effective address to invalidate
    //     x20 = PPC_CPU_State pointer (JIT context)
    mvn     w1, wzr                         // 0xFFFFFFFF
    lsr     w2, w0, #12
    and     w2, w2, #(TLB_ENTRIES - 1)      // TLB index
    add     x3, x20, #tlb_code_0_eff
    str     w1, [x3, x2, lsl #2]
    add     x3, x20, #tlb_data_0_eff
    str     w1, [x3, x2, lsl #2]
    add     x3, x20, #tlb_data_8_eff
    str     w1, [x3, x2, lsl #2]
    ret
