/*
 *	PearPC
 *	jitc_tools.s
 *
 *	Copyright (C) 2003-2006 Sebastian Biallas (sb@biallas.net)
 *	Copyright (C) 2004 Daniel Foesch (dfoesch@cs.nmsu.edu)
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

#ifndef PREFIX
#define PREFIX
#endif

#define EXPORT(sym) EXPORT2(PREFIX, sym)
#define EXPORT2(p, sym) EXPORT3(p, sym)
#define EXPORT3(p, sym) .globl p##sym; p##sym

#define EXTERN(sym) EXTERN2(PREFIX, sym)
#define EXTERN2(p, sym) EXTERN3(p, sym)
#define EXTERN3(p, sym) p##sym

#define STRUCT .struct 0
#define MEMBER(m, s) m:;.struct m+s

.intel_syntax prefix

#	Define this if you want exact handling of the SO bit.
/* #define EXACT_SO */

STRUCT    #PPC_CPU_State
	MEMBER(dummy, 4)
	MEMBER(gpr, 32*4)
	MEMBER(fpr, 32*8)
	MEMBER(cr, 4)
	MEMBER(fpscr, 4)
	MEMBER(xer, 4)
	MEMBER(xer_ca, 4)
	MEMBER(lt, 4)
	MEMBER(ctr, 4)

	MEMBER(msr, 4)
	MEMBER(pvr, 4)

	MEMBER(ibatu, 4*4)
	MEMBER(ibatl, 4*4)
	MEMBER(ibat_bl, 4*4)
	MEMBER(ibat_nbl, 4*4)
	MEMBER(ibat_bepi, 4*4)
	MEMBER(ibat_brpn, 4*4)

	MEMBER(dbatu, 4*4)
	MEMBER(dbatl, 4*4)
	MEMBER(dbat_bl, 4*4)
	MEMBER(dbat_nbl, 4*4)
	MEMBER(dbat_bepi, 4*4)
	MEMBER(dbat_brpn, 4*4)

	MEMBER(sdr1, 4)

	MEMBER(sr, 16*4)

	MEMBER(dar, 4)
	MEMBER(dsisr, 4)
	MEMBER(sprg, 4*4)
	MEMBER(srr0, 4)
	MEMBER(srr1, 4)

	MEMBER(decr, 4)
	MEMBER(ear, 4)
	MEMBER(pir, 4)
	MEMBER(tb, 8)

	MEMBER(hid, 16*4)

	MEMBER(pc, 4)
	MEMBER(npc, 4)
	MEMBER(current_opc, 4)

	MEMBER(exception_pending, 1)
	MEMBER(dec_exception, 1)
	MEMBER(ext_exception, 1)
	MEMBER(stop_exception, 1)
	MEMBER(singlestep_ignore, 1)
	MEMBER(align1, 1)
	MEMBER(align2, 1)
	MEMBER(align3, 1)

	MEMBER(pagetable_base, 4)
	MEMBER(pagetable_hashmask, 4)
	MEMBER(reserve, 4)
	MEMBER(have_reservation, 4)

	MEMBER(tlb_last, 4)
	MEMBER(tlb_pa, 4*4)
	MEMBER(tlb_va, 4*4)
	MEMBER(effective_code_page, 4)
	MEMBER(physical_code_page, 4)
	MEMBER(pdec, 2*4)
	MEMBER(ptb, 2*4)

	MEMBER(temp, 4)
	MEMBER(temp2, 4)
	MEMBER(x87cw, 4)
	MEMBER(pc_ofs, 4)
	MEMBER(current_code_base, 4)

STRUCT	#JITC
	MEMBER(clientPages, 4)

STRUCT	#ClientPage
	MEMBER(entrypoints, 1024*4)
	MEMBER(baseaddress, 4)
	MEMBER(tcf_current, 4)
	MEMBER(bytesLeft, 4)
	MEMBER(tcp, 4)
	MEMBER(moreRU, 4)
	MEMBER(lessRU, 4)

#define gCPU(r) EXTERN(gCPU)+r

.text

.balign 16
##############################################################################################
##
EXPORT(ppc_flush_carry_and_flags_asm):
	jc	1f
	call	EXTERN(ppc_flush_flags_asm)
	and	byte ptr [gCPU(xer+3)], ~(1<<5)
	ret
1:
	call	EXTERN(ppc_flush_flags_asm)
	or	byte ptr [gCPU(xer+3)], (1<<5)
	ret

##############################################################################################
##

#ifndef EXACT_SO
#define HANDLE_SO
#else
#define HANDLE_SO test byte ptr [gCPU(xer+3)], 1<<7; jnz 4f
#endif

.balign 16
##############################################################################################
##
EXPORT(ppc_flush_flags_asm):
	js	3f
	jnz	2f
1:
	and	byte ptr [gCPU(cr+3)], 0x0f
	or	byte ptr [gCPU(cr+3)], 1<<5
	HANDLE_SO
	ret
2:
	and	byte ptr [gCPU(cr+3)], 0x0f
	or	byte ptr [gCPU(cr+3)], 1<<6
	HANDLE_SO
	ret
3:
	and	byte ptr [gCPU(cr+3)], 0x0f
	or	byte ptr [gCPU(cr+3)], 1<<7
	HANDLE_SO
	ret
#ifdef EXACT_SO
4:
	or	byte ptr [gCPU(cr+3)], 1<<4
	ret
#endif

.balign 16
##############################################################################################
##	called after "cmp cr0, ..", with X even
EXPORT(ppc_flush_flags_signed_0_asm):
	jl	3f
	jg	2f
1:
	and	byte ptr [gCPU(cr+3)], 0x0f
	or	byte ptr [gCPU(cr+3)], 1<<5
	HANDLE_SO
	ret
2:
	and	byte ptr [gCPU(cr+3)], 0x0f
	or	byte ptr [gCPU(cr+3)], 1<<6
	HANDLE_SO
	ret
3:
	and	byte ptr [gCPU(cr+3)], 0x0f
	or	byte ptr [gCPU(cr+3)], 1<<7
	HANDLE_SO
	ret
#ifdef EXACT_SO
4:
	or	byte ptr [gCPU(cr+3)], 1<<4
	ret
#endif

.balign 16
##############################################################################################
##	called after "cmpl cr0, ..", with X even
EXPORT(ppc_flush_flags_unsigned_0_asm):
	jb	3f
	ja	2f
1:
	and	byte ptr [gCPU(cr+3)], 0x0f
	or	byte ptr [gCPU(cr+3)], 1<<5
	HANDLE_SO
	ret
2:
	and	byte ptr [gCPU(cr+3)], 0x0f
	or	byte ptr [gCPU(cr+3)], 1<<6
	HANDLE_SO
	ret
3:
	and	byte ptr [gCPU(cr+3)], 0x0f
	or	byte ptr [gCPU(cr+3)], 1<<7
	HANDLE_SO
	ret
#ifdef EXACT_SO
.so:
	or	byte ptr [gCPU(cr+3)], 1<<4
	ret
#endif

.balign 16
##############################################################################################
##	called after "cmp crX, ..", with X even
ppc_flush_flags_signed_even_asm:
	jl	3f
	jg	2f
1:
	and	byte ptr [gCPU(cr+%eax)], 0x0f
	or	byte ptr [gCPU(cr+%eax)], 1<<5
	HANDLE_SO
	ret
2:
	and	byte ptr [gCPU(cr+%eax)], 0x0f
	or	byte ptr [gCPU(cr+%eax)], 1<<6
	HANDLE_SO
	ret
3:
	and	byte ptr [gCPU(cr+%eax)], 0x0f
	or	byte ptr [gCPU(cr+%eax)], 1<<7
	HANDLE_SO
	ret
#ifdef EXACT_SO
4:
	or	byte ptr [gCPU(cr+%eax)], 1<<4
	ret
#endif

.balign 16
##############################################################################################
##	called after "cmpl crX, ..", with X even
ppc_flush_flags_unsigned_even_asm:
	jb	3f
	ja	2f
1:
	and	byte ptr [gCPU(cr+%eax)], 0x0f
	or	byte ptr [gCPU(cr+%eax)], 1<<5
	HANDLE_SO
	ret
2:
	and	byte ptr [gCPU(cr+%eax)], 0x0f
	or	byte ptr [gCPU(cr+%eax)], 1<<6
	HANDLE_SO
	ret
3:
	and	byte ptr [gCPU(cr+%eax)], 0x0f
	or	byte ptr [gCPU(cr+%eax)], 1<<7
	HANDLE_SO
	ret
#ifdef EXACT_SO
4:
	or	byte ptr [gCPU(cr+%eax)], 1<<4
	ret
#endif

.balign 16
##############################################################################################
##	called after "cmp crX, ..", with X odd
EXPORT(ppc_flush_flags_signed_odd_asm):
	jl	3f
	jg	2f
1:
	and	byte ptr [gCPU(cr+%eax)], 0xf0
	or	byte ptr [gCPU(cr+%eax)], 1<<1
	HANDLE_SO
	ret
2:
	and	byte ptr [gCPU(cr+%eax)], 0xf0
	or	byte ptr [gCPU(cr+%eax)], 1<<2
	HANDLE_SO
	ret
3:
	and	byte ptr [gCPU(cr+%eax)], 0xf0
	or	byte ptr [gCPU(cr+%eax)], 1<<3
	HANDLE_SO
	ret
#ifdef EXACT_SO
4:
	or	byte ptr [gCPU(cr+%eax)], 1<<4
	ret
#endif

.balign 16
##############################################################################################
##	called after "cmpl crX, ..", with X odd
EXPORT(ppc_flush_flags_unsigned_odd_asm):
	jb	3f
	ja	2f
1:
	and	byte ptr [gCPU(cr+%eax)], 0xf0
	or	byte ptr [gCPU(cr+%eax)], 1<<1
	HANDLE_SO
	ret
2:
	and	byte ptr [gCPU(cr+%eax)], 0xf0
	or	byte ptr [gCPU(cr+%eax)], 1<<2
	HANDLE_SO
	ret
3:
	and	byte ptr [gCPU(cr+%eax)], 0xf0
	or	byte ptr [gCPU(cr+%eax)], 1<<3
	HANDLE_SO
	ret
#ifdef EXACT_SO
4:
	or	byte ptr [gCPU(cr+%eax)], 1<<4
	ret
#endif

##############################################################################################
##	ppc_set_msr_asm
##
##	IN: %eax: new msr
##
singlestep_error: .asciz "Singlestep support not implemented yet\n"
.balign 16

EXPORT(ppc_set_msr_asm):
	mov	%ecx, [gCPU(msr)]
	test	%eax, (1<<10)	# MSR_SE
	jnz	4f
	test	%eax, ~((1<<30)|(1<<27)|(1<<25)|(1<<18)|(1<<15)|(1<<14)|(1<<13)|(1<<12)|(1<<11)|(1<<10)|(1<<8)|(1<<5)|(1<<4)|(1<<1))
	jnz	5f
	test	%eax, (1<<18)	# MSR_POW
	jnz	2f
1:
		## Do this first so the invalidate can clobber %eax and
		## we won''t care
 	mov	[gCPU(msr)], %eax
	xor	%eax, %ecx
	
		## See if the privilege level (MSR_PR), data address
		## translation (MSR_DR) or code address translation (MSR_IR)
		## is changing, in which case we need to invalidate the tlb
	test	%eax, (1<<14) | (1<<4) | (1<<5)

	jnz	EXTERN(ppc_mmu_tlb_invalidate_all_asm)
	ret

2:
	push	%eax
	call	EXTERN(cpu_doze)
	pop	%eax
	mov	%ecx, [gCPU(msr)]
	and	%eax, ~(1<<18)
	jmp	1b

4:
	mov	%eax, singlestep_error
	jmp	EXTERN(jitc_error)

5:
	jmp	EXTERN(jitc_error_msr_unsupported_bits)

##############################################################################################
.macro ppc_atomic_raise_ext_exception_macro
	lock or	dword ptr [gCPU(exception_pending)], 0x00010001
.endm

##############################################################################################
.macro ppc_atomic_cancel_ext_exception_macro
	mov	%eax, [gCPU(exception_pending)]
9:
	test	%eax, 0x00000100			# dec_exception
	mov	%ebx, %eax
	setnz	%bl
	and	%ebx, 0x00000101
	lock cmpxchg dword ptr [gCPU(exception_pending)], %ebx
	jne	9b
.endm

##############################################################################################
.macro ppc_atomic_raise_dec_exception_macro
	lock or	dword ptr [gCPU(exception_pending)], 0x00000101
.endm

##############################################################################################
.macro ppc_atomic_cancel_dec_exception_macro
	mov	%eax, [gCPU(exception_pending)]
9:
	test	%eax, 0x00010000			# ext_exception
	mov	%ebx, %eax
	setnz	%bl
	and	%ebx, 0x00010001
	lock cmpxchg dword ptr [gCPU(exception_pending)], %ebx
	jne	9b
.endm

.balign 16
##############################################################################################
EXPORT(ppc_cpu_atomic_raise_dec_exception):
	ppc_atomic_raise_dec_exception_macro
	ret
	
.balign 16
##############################################################################################
EXPORT(ppc_cpu_atomic_raise_ext_exception):
	ppc_atomic_raise_ext_exception_macro
	ret

.balign 16
##############################################################################################
EXPORT(ppc_cpu_atomic_cancel_ext_exception):
	ppc_atomic_cancel_ext_exception_macro
	ret

.balign 16
ppc_jitc_new_pc:
#	db	0xcc
	mov	%ecx, [EXTERN(gJITC)+clientPages]
	mov	%ebx, %eax
	shr	%eax, 12
	mov	%eax, [%ecx+%eax*4]
	test	%eax, %eax
	jnz	1f

	mov	%eax, %ebx
	and	%eax, 0xfffff000
	call	EXTERN(jitcCreateClientPage)

1:      # have client page
	call	EXTERN(jitcTouchClientPage)
	cmp	dword ptr [%eax+tcf_current], 0
	je	3f
	mov	%ecx, %ebx
	mov	%esi, %eax
	and	%ecx, 0x00000ffc
	mov	%eax, [%eax + entrypoints + %ecx]
	test	%eax, %eax
	jz	2f
	ret

2:
	mov	%eax, %esi
	mov	%edx, %ebx
	and	%edx, 0xfffff000
	jmp	EXTERN(jitcNewEntrypoint)

3:
	mov	%edx, %ebx
	mov	%ecx, %ebx
	and	%edx, 0xfffff000
	and	%ecx, 0x00000fff
	jmp	EXTERN(jitcStartTranslation)
	
##############################################################################################
##
##	IN: %eax new client pc (physical address)
##
.macro ppc_new_pc_intern
	call	EXTERN(jitcNewPC)
#	call	ppc_jitc_new_pc
	jmp	%eax
.endm

##############################################################################################
##
.macro exception_epilogue entry
	xor	%eax, %eax
	mov	[gCPU(msr)], %eax
	mov	[gCPU(current_code_base)], %eax
	call	EXTERN(ppc_mmu_tlb_invalidate_all_asm)
	mov	%eax, \entry
	ppc_new_pc_intern
.endm

.balign 16
##############################################################################################
##	ppc_dsi_exception
##
##	IN: %eax fault addr
##	    %ecx dsisr bits
##
##	does not return, so call this per JMP
EXPORT(ppc_dsi_exception_asm):
	mov	[gCPU(dar)], %eax
	mov	[gCPU(dsisr)], %ecx
EXPORT(ppc_dsi_exception_special_asm):
	mov	%edx, [gCPU(pc_ofs)]
	mov	%eax, [gCPU(msr)]
	add	%edx, [gCPU(current_code_base)]
	and	%eax, 0x87c0ffff
	mov	[gCPU(srr1)], %eax
	mov	[gCPU(srr0)], %edx
	exception_epilogue 0x300

.balign 16
##############################################################################################
##	ppc_isi_exception_asm
##
##	IN: %eax: fault addr
##	    %ecx: srr1 bits
##
##	does not return, so call this per JMP
EXPORT(ppc_isi_exception_asm):
	mov	[gCPU(srr0)], %eax
	mov	%eax, [gCPU(msr)]
	and	%eax, 0x87c0ffff
	or	%eax, %ecx
	mov	[gCPU(srr1)], %eax
	exception_epilogue 0x400
	
.balign 16
##############################################################################################
##
##	IN: 
##          %eax: current pc
##
##	this is only called indirectly
EXPORT(ppc_ext_exception_asm):
	mov	[gCPU(srr0)], %eax
	mov	%edx, [gCPU(msr)]
	ppc_atomic_cancel_ext_exception_macro
	and	%edx, 0x87c0ffff
	mov	[gCPU(srr1)], %edx
	exception_epilogue 0x500

.balign 16
##############################################################################################
##
##	IN: %ecx: srr1 bits
##          %esi: pc_ofs
##
##	does not return, so call this per JMP
EXPORT(ppc_program_exception_asm):

	# debug
#	pusha
#	mov	%eax, %ecx
#	call	EXTERN(jitc_error_program)
#	popa

	mov	[gCPU(pc_ofs)], %esi
	mov	%eax, [gCPU(msr)]
	mov	%edx, %esi
	and	%eax, 0x87c0ffff
	add	%edx, [gCPU(current_code_base)]
	or	%eax, %ecx
	mov	[gCPU(srr0)], %edx
	mov	[gCPU(srr1)], %eax
	exception_epilogue 0x700

.balign 16
##############################################################################################
##
##	IN:
##          %esi: pc_ofs
##
##	does not return, so call this per JMP
EXPORT(ppc_no_fpu_exception_asm):
	mov	%edx, %esi
	mov	[gCPU(pc_ofs)], %esi
	mov	%eax, [gCPU(msr)]
	add	%edx, [gCPU(current_code_base)]
	and	%eax, 0x87c0ffff
	mov	[gCPU(srr0)], %edx
	mov	[gCPU(srr1)], %eax
	exception_epilogue 0x800

.balign 16
##############################################################################################
##
##	IN:
##          %esi: pc_ofs
##
##	does not return, so call this per JMP
EXPORT(ppc_no_vec_exception_asm):
	mov	%edx, %esi
	mov	[gCPU(pc_ofs)], %esi
	mov	%eax, [gCPU(msr)]
	add	%edx, [gCPU(current_code_base)]
	and	%eax, 0x87c0ffff
	mov	[gCPU(srr0)], %edx
	mov	[gCPU(srr1)], %eax
	exception_epilogue 0xf20

.balign 16
##############################################################################################
##
##	IN:
##          %eax: current pc
##
##	this is only called indirectly
EXTERN(ppc_dec_exception_asm):
	mov	[gCPU(srr0)], %eax
	mov	%edx, [gCPU(msr)]
	ppc_atomic_cancel_dec_exception_macro
	and	%edx, 0x87c0ffff
	mov	[gCPU(srr1)], %edx
	exception_epilogue 0x900

.balign 16
##############################################################################################
##
##	IN:
##          %esi: pc_ofs
##
##	does not return, so call this per JMP
EXPORT(ppc_sc_exception_asm):
	mov	%edx, %esi
	mov	[gCPU(pc_ofs)], %esi
	mov	%eax, [gCPU(msr)]
	add	%edx, [gCPU(current_code_base)]
	and	%eax, 0x87c0ffff
	mov	[gCPU(srr0)], %edx
	mov	[gCPU(srr1)], %eax
	xor	%eax, %eax
	call	EXTERN(ppc_set_msr_asm)
	xor	%eax, %eax
	mov	[gCPU(current_code_base)], %eax
	mov	%eax, 0xc00	# entry of SC exception
	ppc_new_pc_intern
	
.balign 16
##############################################################################################
##	ppc_heartbeat_ext_rel_asm
##
##
EXPORT(ppc_heartbeat_ext_rel_asm):
	test	byte ptr [gCPU(exception_pending)], 1
	jnz	1f
2:
	ret
1:
	test	byte ptr [gCPU(stop_exception)], 1
	jnz	3f
	test	byte ptr [gCPU(msr+1)], 1<<7		# MSR_EE
	jz	2b
	add	%esp, 4
	add	%eax, [gCPU(current_code_base)]
	test	byte ptr [gCPU(ext_exception)], 1
	jnz	EXTERN(ppc_ext_exception_asm)
	test	byte ptr [gCPU(dec_exception)], 1
	jnz	EXTERN(ppc_dec_exception_asm)
	mov	%eax, exception_error
	jmp	EXTERN(jitc_error)
3:
	add	%esp, 4
	jmp	ppc_stop_jitc_asm
	
.balign 16
##############################################################################################
##	ppc_heartbeat_ext_asm
##	%eax -- new pc
##
EXPORT(ppc_heartbeat_ext_asm):
	mov	%edx, %eax
	and	%edx, 0xfffff000
	test	byte ptr [gCPU(exception_pending)], 1
	mov	[gCPU(current_code_base)], %edx
	jnz	1f
2:
	ret
1:
	test	byte ptr [gCPU(stop_exception)], 1
	jnz	3f
	test	byte ptr [gCPU(msr+1)], 1<<7		# MSR_EE
	jz	2b
	add	%esp, 4
	test	byte ptr [gCPU(ext_exception)], 1
	jnz	EXTERN(ppc_ext_exception_asm)
	test	byte ptr [gCPU(dec_exception)], 1
	jnz	EXTERN(ppc_dec_exception_asm)
	mov	%eax, exception_error
	jmp	EXTERN(jitc_error)
3:
	add	%esp, 4
	jmp	ppc_stop_jitc_asm

exception_error: .asciz	"Unknown exception signaled?!\n"


.balign 16
##############################################################################################
##	ppc_new_pc_rel_asm
##
##	IN: %eax new client pc relative
##
##	does not return, so call this per JMP
EXPORT(ppc_new_pc_rel_asm):
	add	%eax, [gCPU(current_code_base)]
	call	EXTERN(ppc_heartbeat_ext_asm)
	push	0				# bytes to unwind
	call	EXTERN(ppc_effective_to_physical_code)
	ppc_new_pc_intern

.balign 16
##############################################################################################
##	ppc_new_pc_asm
## 
##	IN: %eax new client pc (effective address)
##
##	does not return, so call this per JMP
EXPORT(ppc_new_pc_asm):
	call	EXTERN(ppc_heartbeat_ext_asm)
	push	0
	call	EXTERN(ppc_effective_to_physical_code)
	ppc_new_pc_intern

.balign 16
##############################################################################################
##
##
EXPORT(ppc_new_pc_this_page_asm):
#	mov	%esi, [%esp]
#	mov	[%esi-6], %eax			# patch it now, later we don''t have the value
	add	%eax, [gCPU(current_code_base)]
	
	push	4
	call	EXTERN(ppc_effective_to_physical_code)
	call	EXTERN(jitcNewPC)

#if 0
	pop	%esi	
	#	now %eax and %esi are both native addresses
	#	%eax is dest and %esi is source
	#
	#	we assume that we can overwrite 15 bytes before the call
	#	and 3 bytes after the call and the 5 bytes of the call instruction
	mov	%edx, %eax
	sub	%eax, %esi
	
	mov	byte ptr [%esi-20], 0xf6   # test [gCPU(exception_pending)], 1
	mov	byte ptr [%esi-19], 0x05
	mov	dword ptr [%esi-18], gCPU(exception_pending)
	mov	byte ptr [%esi-14], 1
	
	add	%eax, 7
	
	mov	byte ptr [%esi-13], 0x0f   # jz	dest (%edx)
	mov	byte ptr [%esi-12], 0x84
	mov	dword [%esi-11], %eax	   # the jz is relative to (%esi-7)
	
	mov	%eax, ppc_heartbeat_ext_rel_asm - 3
	sub	%eax, %esi
	
	mov	byte ptr [%esi-7], 0xb8	   # mov	%eax, offset
##	mov	dword ptr [%esi-6], ...	   # see above, this is already patched!

	mov	byte ptr [%esi-2], 0xe8	   # call	ppc_heartbeat_ext_rel_asm
	mov	dword ptr [%esi-1], %eax   # the call is relative to (%esi+3)
	jmp	%edx
#endif	
	pop	%edi
	#	now %eax and %edi are both native addresses
	#	%eax is dest and %edi is source
	#
	#	we assume that there is a "mov %eax, xxx" instruction before
	#	calling this function, and note that 5 is also the length of a jmp xxx
	#	so we patch %edi-10
	mov	%edx, %eax
	sub	%edi, 5
	mov	byte ptr [%edi-5], 0xe9
	sub	%eax, %edi
	mov	dword ptr [%edi-4], %eax
	jmp	%edx

.balign 2
ppc_start_fpu_cw: .short 0x37f

.balign 16
##############################################################################################
##
##	IN: %eax new client pc (effective address)
##
EXPORT(ppc_start_jitc_asm):
	push	%ebx
	push	%ebp
	push	%esi
	push	%edi
	fldcw	[ppc_start_fpu_cw]
	jmp	EXTERN(ppc_new_pc_asm)

.balign 16
##############################################################################################
##
##	call per JMP
##
ppc_stop_jitc_asm:
	pop	%edi
	pop	%esi
	pop	%ebp
	pop	%ebx
	ret

##############################################################################################
##
##	IN: %eax cpuid level
##	    %edx dest
##

EXPORT(ppc_cpuid_asm):
	push	%ebx

	pushfd
	pop	%ebx
	mov	%ecx, %ebx
	xor	%ebx, 0x00200000
	push	%ebx
	popfd
	pushfd
	pop	%ebx
	cmp	%ebx, %ecx
	jne	1f

	pop	%ebx
	xor	%eax, %eax
	ret

1:
	push	%edi
	mov	%edi, %edx
	cpuid
	mov	[%edi], %eax
	mov	[%edi+4], %ecx
	mov	[%edi+8], %edx
	mov	[%edi+12], %ebx
	pop	%edi
	pop	%ebx
	mov	%eax, 1
	ret
