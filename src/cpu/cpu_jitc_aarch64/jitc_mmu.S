/*
 *  PearPC
 *  jitc_mmu.S
 *
 *  AArch64 MMU assembly stubs for JITC
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

.text

//////////////////////////////////////////////////////////////////////////////////////////////
//
//  ppc_effective_to_physical_code
//
//  Simple identity mapping stub: physical = effective
//  (Proper TLB/BAT translation will be added later)
//
//  IN:  w0 = effective address
//       w1 = bytes to unwind (ignored on aarch64)
//  OUT: w0 = physical address
//
.p2align 4
do_export EXTERN(ppc_effective_to_physical_code)
    // For now, just return the effective address as-is (identity mapping).
    // A real implementation would do TLB lookup / page table walk here.
    // Mask with memory size for safety:
    //   physical = effective  (assumes 1:1 mapping or real mode)
    ret


//////////////////////////////////////////////////////////////////////////////////////////////
//
//  Memory read stubs
//  These are called from translated code to handle memory accesses.
//  For now they are minimal stubs.
//

.p2align 4
do_export EXTERN(ppc_read_effective_byte_asm)
    // IN: w0 = effective address
    // OUT: w0 = byte value
    // Stub: just return 0
    mov w0, #0
    ret

.p2align 4
do_export EXTERN(ppc_read_effective_half_z_asm)
    mov w0, #0
    ret

.p2align 4
do_export EXTERN(ppc_read_effective_half_s_asm)
    mov w0, #0
    ret

.p2align 4
do_export EXTERN(ppc_read_effective_word_asm)
    mov w0, #0
    ret

.p2align 4
do_export EXTERN(ppc_read_effective_dword_asm)
    mov x0, #0
    ret

//////////////////////////////////////////////////////////////////////////////////////////////
//
//  Memory write stubs
//

.p2align 4
do_export EXTERN(ppc_write_effective_byte_asm)
    // IN: w0 = effective address, w1 = value
    // Stub: do nothing
    ret

.p2align 4
do_export EXTERN(ppc_write_effective_half_asm)
    ret

.p2align 4
do_export EXTERN(ppc_write_effective_word_asm)
    ret

.p2align 4
do_export EXTERN(ppc_write_effective_dword_asm)
    ret

//////////////////////////////////////////////////////////////////////////////////////////////
//
//  String load/store stubs
//

.p2align 4
do_export EXTERN(ppc_opc_stswi_asm)
    ret

.p2align 4
do_export EXTERN(ppc_opc_lswi_asm)
    ret

.p2align 4
do_export EXTERN(ppc_opc_icbi_asm)
    ret
