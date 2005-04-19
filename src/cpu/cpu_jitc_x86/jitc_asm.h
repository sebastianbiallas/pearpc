/*
 *	PearPC
 *	jitc_asm.h
 *
 *	Copyright (C) 2004 Sebastian Biallas (sb@biallas.net)
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

#ifndef __JITC_ASM_H__
#define __JITC_ASM_H__

#include "system/types.h"

extern "C" void ppc_effective_to_physical_code_asm();
extern "C" void ppc_effective_to_physical_data_asm();

extern "C" void ppc_write_effective_byte_asm();
extern "C" void ppc_write_effective_half_asm();
extern "C" void ppc_write_effective_word_asm();
extern "C" void ppc_write_effective_dword_asm();
extern "C" void ppc_write_effective_qword_asm();
extern "C" void ppc_write_effective_qword_sse_asm();

extern "C" void ppc_read_effective_byte_asm();
extern "C" void ppc_read_effective_half_z_asm();
extern "C" void ppc_read_effective_half_s_asm();
extern "C" void ppc_read_effective_word_asm();
extern "C" void ppc_read_effective_dword_asm();
extern "C" void ppc_read_effective_qword_asm();
extern "C" void ppc_read_effective_qword_sse_asm();

extern "C" void ppc_opc_stswi_asm();
extern "C" void ppc_opc_lswi_asm();
extern "C" void ppc_opc_icbi_asm();

extern "C" void ppc_isi_exception_asm();
extern "C" void ppc_dsi_exception_asm();
extern "C" void ppc_dsi_exception_special_asm();
extern "C" void ppc_program_exception_asm();
extern "C" void ppc_no_fpu_exception_asm();
extern "C" void ppc_no_vec_exception_asm();
extern "C" void ppc_sc_exception_asm();
extern "C" void ppc_flush_flags_asm();
extern "C" void ppc_flush_flags_signed_even_asm();
extern "C" void ppc_flush_flags_signed_odd_asm();
extern "C" void ppc_flush_flags_signed_0_asm();
extern "C" void ppc_flush_flags_unsigned_even_asm();
extern "C" void ppc_flush_flags_unsigned_odd_asm();
extern "C" void ppc_flush_flags_unsigned_0_asm();
extern "C" void ppc_new_pc_asm();
extern "C" void ppc_new_pc_rel_asm();
extern "C" void ppc_new_pc_this_page_asm();
extern "C" void ppc_heartbeat_ext_asm();
extern "C" void ppc_heartbeat_ext_rel_asm();


extern "C" void ppc_set_msr_asm();
extern "C" void ppc_mmu_tlb_invalidate_all_asm();
extern "C" void ppc_mmu_tlb_invalidate_entry_asm();

extern "C" void FASTCALL ppc_start_jitc_asm(uint32 newpc);
extern "C" bool FASTCALL ppc_cpuid_asm(uint32 level, void *struc);

#endif
