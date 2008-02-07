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


#include "jitc_common.h"

.text

.balign 16
##############################################################################################
##
EXPORT(ppc_flush_carry_and_flags_asm):
#	getCurCPU 1
	lea	%rdi, [curCPUoffset(1)]
	jc	1f
	call	EXTERN(ppc_flush_flags_asm_cpu)
	and	byte ptr [%rdi+xer+3], ~(1<<5)
	ret
1:
	call	EXTERN(ppc_flush_flags_asm_cpu)
	or	byte ptr [%rdi+xer+3], (1<<5)
	ret

##############################################################################################
##

#ifndef EXACT_SO
#define HANDLE_SO
#else
#define HANDLE_SO test byte ptr [%rdi+xer+3], 1<<7; jnz 4f
#endif

.balign 16
##############################################################################################
##
EXPORT(ppc_flush_flags_asm):
	lea	%rdi, [curCPUoffset(1)]
#	getCurCPU 1
EXPORT(ppc_flush_flags_asm_cpu):
	js	3f
	jnz	2f
1:
	and	byte ptr [%rdi+cr+3], 0x0f
	or	byte ptr [%rdi+cr+3], 1<<5
	HANDLE_SO
	ret
2:
	and	byte ptr [%rdi+cr+3], 0x0f
	or	byte ptr [%rdi+cr+3], 1<<6
	HANDLE_SO
	ret
3:
	and	byte ptr [%rdi+cr+3], 0x0f
	or	byte ptr [%rdi+cr+3], 1<<7
	HANDLE_SO
	ret
#ifdef EXACT_SO
4:
	or	byte ptr [%rdi+cr+3], 1<<4
	ret
#endif

.balign 16
##############################################################################################
##	called after "cmp cr0, ..", with X even
EXPORT(ppc_flush_flags_signed_0_asm):
	jl	3f
	jg	2f
1:
	and	byte ptr [%rdi+cr+3], 0x0f
	or	byte ptr [%rdi+cr+3], 1<<5
	HANDLE_SO
	ret
2:
	and	byte ptr [%rdi+cr+3], 0x0f
	or	byte ptr [%rdi+cr+3], 1<<6
	HANDLE_SO
	ret
3:
	and	byte ptr [%rdi+cr+3], 0x0f
	or	byte ptr [%rdi+cr+3], 1<<7
	HANDLE_SO
	ret
#ifdef EXACT_SO
4:
	or	byte ptr [%rdi+cr+3], 1<<4
	ret
#endif

.balign 16
##############################################################################################
##	called after "cmpl cr0, ..", with X even
EXPORT(ppc_flush_flags_unsigned_0_asm):
	jb	3f
	ja	2f
1:
	and	byte ptr [%rdi+cr+3], 0x0f
	or	byte ptr [%rdi+cr+3], 1<<5
	HANDLE_SO
	ret
2:
	and	byte ptr [%rdi+cr+3], 0x0f
	or	byte ptr [%rdi+cr+3], 1<<6
	HANDLE_SO
	ret
3:
	and	byte ptr [%rdi+cr+3], 0x0f
	or	byte ptr [%rdi+cr+3], 1<<7
	HANDLE_SO
	ret
#ifdef EXACT_SO
.so:
	or	byte ptr [%rdi+cr+3], 1<<4
	ret
#endif

.balign 16
##############################################################################################
##	called after "cmp crX, ..", with X even
ppc_flush_flags_signed_even_asm:
	jl	3f
	jg	2f
1:
	and	byte ptr [%rdi+cr+%rax], 0x0f
	or	byte ptr [%rdi+cr+%rax], 1<<5
	HANDLE_SO
	ret
2:
	and	byte ptr [%rdi+cr+%rax], 0x0f
	or	byte ptr [%rdi+cr+%rax], 1<<6
	HANDLE_SO
	ret
3:
	and	byte ptr [%rdi+cr+%rax], 0x0f
	or	byte ptr [%rdi+cr+%rax], 1<<7
	HANDLE_SO
	ret
#ifdef EXACT_SO
4:
	or	byte ptr [%rdi+cr+%rax], 1<<4
	ret
#endif

.balign 16
##############################################################################################
##	called after "cmpl crX, ..", with X even
ppc_flush_flags_unsigned_even_asm:
	jb	3f
	ja	2f
1:
	and	byte ptr [%rdi+cr+%rax], 0x0f
	or	byte ptr [%rdi+cr+%rax], 1<<5
	HANDLE_SO
	ret
2:
	and	byte ptr [%rdi+cr+%rax], 0x0f
	or	byte ptr [%rdi+cr+%rax], 1<<6
	HANDLE_SO
	ret
3:
	and	byte ptr [%rdi+cr+%rax], 0x0f
	or	byte ptr [%rdi+cr+%rax], 1<<7
	HANDLE_SO
	ret
#ifdef EXACT_SO
4:
	or	byte ptr [%rdi+cr+%rax], 1<<4
	ret
#endif

.balign 16
##############################################################################################
##	called after "cmp crX, ..", with X odd
EXPORT(ppc_flush_flags_signed_odd_asm):
	jl	3f
	jg	2f
1:
	and	byte ptr [%rdi+cr+%rax], 0xf0
	or	byte ptr [%rdi+cr+%rax], 1<<1
	HANDLE_SO
	ret
2:
	and	byte ptr [%rdi+cr+%rax], 0xf0
	or	byte ptr [%rdi+cr+%rax], 1<<2
	HANDLE_SO
	ret
3:
	and	byte ptr [%rdi+cr+%rax], 0xf0
	or	byte ptr [%rdi+cr+%rax], 1<<3
	HANDLE_SO
	ret
#ifdef EXACT_SO
4:
	or	byte ptr [%rdi+cr+%rax], 1<<4
	ret
#endif

.balign 16
##############################################################################################
##	called after "cmpl crX, ..", with X odd
EXPORT(ppc_flush_flags_unsigned_odd_asm):
	jb	3f
	ja	2f
1:
	and	byte ptr [%rdi+cr+%rax], 0xf0
	or	byte ptr [%rdi+cr+%rax], 1<<1
	HANDLE_SO
	ret
2:
	and	byte ptr [%rdi+cr+%rax], 0xf0
	or	byte ptr [%rdi+cr+%rax], 1<<2
	HANDLE_SO
	ret
3:
	and	byte ptr [%rdi+cr+%rax], 0xf0
	or	byte ptr [%rdi+cr+%rax], 1<<3
	HANDLE_SO
	ret
#ifdef EXACT_SO
4:
	or	byte ptr [%rdi+cr+%rax], 1<<4
	ret
#endif

##############################################################################################
##	ppc_set_msr_asm
##
##	IN: %eax: new msr
##	    %rdi: cpu
##
singlestep_error: .asciz "Singlestep support not implemented yet\n"
.balign 16

EXPORT(ppc_set_msr_asm):
	mov	%ecx, [curCPU(msr)]
	test	%eax, (1<<10)	# MSR_SE
	jnz	4f
	test	%eax, ~((1<<30)|(1<<27)|(1<<25)|(1<<18)|(1<<15)|(1<<14)|(1<<13)|(1<<12)|(1<<11)|(1<<10)|(1<<8)|(1<<5)|(1<<4)|(1<<1))
	jnz	5f
	test	%eax, (1<<18)	# MSR_POW
	jnz	2f
1:
		## Do this first so the invalidate can clobber %eax and
		## we won''t care
 	mov	[curCPU(msr)], %eax
	xor	%eax, %ecx
	
		## See if the privilege level (MSR_PR), data address
		## translation (MSR_DR) or code address translation (MSR_IR)
		## is changing, in which case we need to invalidate the tlb
	test	%eax, (1<<14) | (1<<4) | (1<<5)

	jnz	EXTERN(ppc_mmu_tlb_invalidate_all_asm)
	rep;	ret

2:
	mov	%ebx, %eax
	mov	%rbp, %rdi
	call	EXTERN(cpu_doze)
	mov	%eax, %ebx
	mov	%rdi, %rbp
	mov	%ecx, [curCPU(msr)]
	and	%eax, ~(1<<18)
	jmp	1b

4:
	mov	%rdi, singlestep_error
	jmp	EXTERN(jitc_error)

5:
	mov	%edi, %eax
	jmp	EXTERN(jitc_error_msr_unsupported_bits)

##############################################################################################
##	
##	IN: %rdi cpu
##
.macro ppc_atomic_raise_ext_exception_macro
	lock or	dword ptr [%rdi+exception_pending], 0x00010001
.endm

##############################################################################################
##	
##	IN: %rdi cpu
##
.macro ppc_atomic_cancel_ext_exception_macro
	mov	%eax, [%rdi+exception_pending]
9:
	test	%eax, 0x00000100			# dec_exception
	mov	%ecx, %eax
	setnz	%cl
	and	%ecx, 0x00000101
	lock cmpxchg dword ptr [%rdi+exception_pending], %ecx
	jne	9b
.endm

##############################################################################################
##	
##	IN: %rdi cpu
##
.macro ppc_atomic_raise_dec_exception_macro
	lock or	dword ptr [%rdi+exception_pending], 0x00000101
.endm

##############################################################################################
##	
##	IN: %rdi cpu
##
.macro ppc_atomic_cancel_dec_exception_macro
	mov	%eax, [%rdi+exception_pending]
9:
	test	%eax, 0x00010000			# ext_exception
	mov	%ebx, %eax
	setnz	%bl
	and	%ebx, 0x00010001
	lock cmpxchg dword ptr [%rdi+exception_pending], %ebx
	jne	9b
.endm

.balign 16
##############################################################################################
##	
##	IN: %rdi cpu
##
EXPORT(ppc_cpu_atomic_raise_dec_exception):
	ppc_atomic_raise_dec_exception_macro
	ret
	
.balign 16
##############################################################################################
##	
##	IN: %rdi cpu
##
EXPORT(ppc_cpu_atomic_raise_ext_exception):
	ppc_atomic_raise_ext_exception_macro
	ret

.balign 16
##############################################################################################
##	
##	IN: %rdi cpu
##
EXPORT(ppc_cpu_atomic_raise_stop_exception):
	lock or	dword ptr [%rdi+exception_pending], 0x01000001
	ret

.balign 16
##############################################################################################
##	
##	IN: %rdi cpu
##
EXPORT(ppc_cpu_atomic_cancel_ext_exception):
	ppc_atomic_cancel_ext_exception_macro
	ret

.balign 16
ppc_jitc_new_pc:
#
#	db	0xcc
#	mov	%ecx, [EXTERN(gJITC)+clientPages]
	mov	%ebx, %eax
	shr	%eax, 12
	mov	%eax, [%ecx+%eax*4]
	test	%eax, %eax
	jnz	1f

	mov	%eax, %ebx
	and	%eax, 0xfffff000
#	call	EXTERN(jitcCreateClientPage)

1:      # have client page
#	call	EXTERN(jitcTouchClientPage)
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
#	jmp	EXTERN(jitcNewEntrypoint)

3:
	mov	%edx, %ebx
	mov	%ecx, %ebx
	and	%edx, 0xfffff000
	and	%ecx, 0x00000fff
#	jmp	EXTERN(jitcStartTranslation)
	
##############################################################################################
##
##	IN: %rdi jitc
##	    %esi new client pc (physical address)
##
.macro ppc_new_pc_intern
	call	EXTERN(jitcNewPC)
#	call	ppc_jitc_new_pc
	jmp	%rax
.endm

##############################################################################################
##	    %rdi: cpu
##
.macro exception_epilogue entry
	xor	%eax, %eax
	mov	[curCPU(msr)], %eax
	mov	[curCPU(current_code_base)], %eax
	call	EXTERN(ppc_mmu_tlb_invalidate_all_asm)
	mov	%rdi, [curCPU(jitc)]
	mov	%esi, \entry
	ppc_new_pc_intern
.endm

.balign 16
##############################################################################################
##	ppc_dsi_exception
##
##	IN: %eax fault addr
##	    %ecx dsisr bits
##	    %rdi: cpu
##
##	does not return, so call this per JMP
EXPORT(ppc_dsi_exception_asm):
	checkCurCPU
	mov	[curCPU(dar)], %eax
	mov	[curCPU(dsisr)], %ecx
EXPORT(ppc_dsi_exception_special_asm):
	checkCurCPU
	mov	%edx, [curCPU(pc_ofs)]
	mov	%eax, [curCPU(msr)]
	add	%edx, [curCPU(current_code_base)]
#	mov	%edi, %eax
#	call	jitc_error
	and	%eax, 0x87c0ffff
	mov	[curCPU(srr1)], %eax
	mov	[curCPU(srr0)], %edx
	exception_epilogue 0x300

.balign 16
##############################################################################################
##	ppc_isi_exception_asm
##
##	IN: %eax: fault addr
##	    %ecx: srr1 bits
##	    %rdi: cpu
##
##	does not return, so call this per JMP
EXPORT(ppc_isi_exception_asm):
	checkCurCPU
	mov	[curCPU(srr0)], %eax
	mov	%eax, [curCPU(msr)]
	and	%eax, 0x87c0ffff
	or	%eax, %ecx
	mov	[curCPU(srr1)], %eax
	exception_epilogue 0x400
	
.balign 16
##############################################################################################
##
##	IN: 
##          %eax: current pc
##	    %rdi: cpu
##
##	this is only called indirectly
EXPORT(ppc_ext_exception_asm):
	checkCurCPU
	mov	[curCPU(srr0)], %eax
	mov	%edx, [curCPU(msr)]
#	ppc_atomic_cancel_ext_exception_macro
	and	%edx, 0x87c0ffff
	mov	[curCPU(srr1)], %edx
	exception_epilogue 0x500

.balign 16
##############################################################################################
##
##	IN: %ecx: srr1 bits
##          %esi: pc_ofs
##
##	does not return, so call this per JMP (Frame 0)
##
EXPORT(ppc_program_exception_asm):

	getCurCPU 0

	# debug
#	pusha
#	mov	%edi, %ecx
#	call	EXTERN(jitc_error_program)
#	popa

	mov	[curCPU(pc_ofs)], %esi
	mov	%eax, [curCPU(msr)]
	mov	%edx, %esi
	and	%eax, 0x87c0ffff
	add	%edx, [curCPU(current_code_base)]
	or	%eax, %ecx
	mov	[curCPU(srr0)], %edx
	mov	[curCPU(srr1)], %eax
	exception_epilogue 0x700

.balign 16
##############################################################################################
##
##	IN:
##          %esi: pc_ofs
##
##	does not return, so call this per JMP (Frame 0)
EXPORT(ppc_no_fpu_exception_asm):
	getCurCPU 0
	
	mov	%edx, %esi
	mov	[curCPU(pc_ofs)], %esi
	mov	%eax, [curCPU(msr)]
	add	%edx, [curCPU(current_code_base)]
	and	%eax, 0x87c0ffff
	mov	[curCPU(srr0)], %edx
	mov	[curCPU(srr1)], %eax
	exception_epilogue 0x800

.balign 16
##############################################################################################
##
##	IN:
##          %esi: pc_ofs
##
##	does not return, so call this per JMP (Frame 0)
EXPORT(ppc_no_vec_exception_asm):
	getCurCPU 0
	mov	%edx, %esi
	mov	[curCPU(pc_ofs)], %esi
	mov	%eax, [curCPU(msr)]
	add	%edx, [curCPU(current_code_base)]
	and	%eax, 0x87c0ffff
	mov	[curCPU(srr0)], %edx
	mov	[curCPU(srr1)], %eax
	exception_epilogue 0xf20

.balign 16
##############################################################################################
##
##	IN:
##          %eax: current pc
##	    %rdi: cpu
##
##	this is only called indirectly
EXTERN(ppc_dec_exception_asm):
	checkCurCPU
	mov	[curCPU(srr0)], %eax
	mov	%edx, [curCPU(msr)]
	ppc_atomic_cancel_dec_exception_macro
	and	%edx, 0x87c0ffff
	mov	[curCPU(srr1)], %edx
	exception_epilogue 0x900

.balign 16
##############################################################################################
##
##	IN:
##          %esi: pc_ofs
##
##	does not return, so call this per JMP (frame 0)
EXPORT(ppc_sc_exception_asm):
	getCurCPU 0
	mov	%edx, %esi
	mov	[%rdi+pc_ofs], %esi
	mov	%eax, [%rdi+msr]
	add	%edx, [%rdi+current_code_base]
	and	%eax, 0x87c0ffff
	mov	[%rdi+srr0], %edx
	mov	[%rdi+srr1], %eax
	exception_epilogue 0xc00
	
.balign 16
##############################################################################################
##	ppc_heartbeat_ext_rel_asm
##
##	This must always be stack-frame 1
##
EXPORT(ppc_heartbeat_ext_rel_asm):

	getCurCPU 1

	test	byte ptr [curCPU(exception_pending)], 1
	jnz	1f
2:
	rep;	ret
1:
	test	byte ptr [curCPU(stop_exception)], 1
	jnz	3f
	test	byte ptr [curCPU(msr)+1], 1<<7		# MSR_EE
	jz	2b
	add	%rsp, 8
	add	%eax, [curCPU(current_code_base)]
	test	byte ptr [curCPU(ext_exception)], 1
	jnz	EXTERN(ppc_ext_exception_asm)
	test	byte ptr [curCPU(dec_exception)], 1
	jnz	EXTERN(ppc_dec_exception_asm)
	mov	%rdi, exception_error
	jmp	EXTERN(jitc_error)
3:
	add	%rsp, 8
	jmp	ppc_stop_jitc_asm
	
.balign 16
##############################################################################################
##	ppc_heartbeat_ext_asm
##	%eax -- new pc
##
##	This must always be stack-frame 1
##
EXPORT(ppc_heartbeat_ext_asm):

	getCurCPU 1
#	mov	%r14d, %eax
#	mov	%r13, %rdi
#	call	EXTERN(jitc_error)
#	mov	%rdi, %r13
#	mov	%eax, %r14d
#	
#	mov	%r14d, %eax
#	mov	%r13, %rdi
#	call	EXTERN(jitc_error2)
#	mov	%rdi, %r13
#	mov	%eax, %r14d

	mov	%edx, %eax
	and	%edx, 0xfffff000
	test	byte ptr [curCPU(exception_pending)], 1
	mov	[curCPU(current_code_base)], %edx
	jnz	1f
2:
	ret
1:
	test	byte ptr [curCPU(stop_exception)], 1
	jnz	3f
	test	byte ptr [curCPU(msr)+1], 1<<7		# MSR_EE
	jz	2b
	add	%rsp, 8
	test	byte ptr [curCPU(ext_exception)], 1
	jnz	EXTERN(ppc_ext_exception_asm)
	test	byte ptr [curCPU(dec_exception)], 1
	jnz	EXTERN(ppc_dec_exception_asm)
	mov	%rdi, exception_error
	jmp	EXTERN(jitc_error)
3:
	add	%rsp, 8
	jmp	ppc_stop_jitc_asm

exception_error: .asciz	"Unknown exception signaled?!\n"


.balign 16
##############################################################################################
##	ppc_new_pc_rel_asm
##
##	IN: %eax new client pc relative
##	    %rdi cpu
##
##	does not return, so call this per JMP (frame 0)
##
EXPORT(ppc_new_pc_rel_asm):
	getCurCPU 0
	add	%eax, [curCPU(current_code_base)]
	call	EXTERN(ppc_heartbeat_ext_asm)
	push	0				# bytes to unwind
	call	EXTERN(ppc_effective_to_physical_code)
	mov	%rdi, [curCPU(jitc)]
	mov	%esi, %eax
	ppc_new_pc_intern

.balign 16
##############################################################################################
##	ppc_new_pc_asm
## 
##	IN: %eax new client pc (effective address)
##
##	does not return, so call this per JMP
EXPORT(ppc_new_pc_asm):
#	mov	%r13d, %eax
#	mov	%edi, %eax
#call EXTERN(jitc_error2)
#	mov	%eax, %r13d
	call	EXTERN(ppc_heartbeat_ext_asm)
	
#	now %rdi contains cpuptr
	
	push	0
	call	EXTERN(ppc_effective_to_physical_code)
	mov	%rdi, [curCPU(jitc)]
	mov	%esi, %eax
	ppc_new_pc_intern

.balign 16
##############################################################################################
##	IN: %eax new client pc relative
##	Frame 1
##
EXPORT(ppc_new_pc_this_page_asm):
#	mov	%esi, [%rsp]
#	mov	[%esi-6], %eax			# patch it now, we don''t have the value later

	## FIXME: needed?
	getCurCPU 1
	add	%eax, [curCPU(current_code_base)]
	push	8				# roll back 8 bytes
	call	EXTERN(ppc_effective_to_physical_code)
	mov	%rdi, [curCPU(jitc)]
	mov	%esi, %eax
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
	jmp	%rdx
#endif	
	pop	%rdi
	#	now %rax and %rdi are both native addresses
	#	%rax is dest and %rdi is source
	#
	#	we assume that there is a "mov %eax, xxx" instruction before
	#	calling this function, and note that 5 is also the length of a jmp xxx
	#	so we patch %rdi-10
	mov	%rdx, %rax
	sub	%rdi, 5
	mov	byte ptr [%rdi-5], 0xe9
	sub	%rax, %rdi
	#
	#	FIXME?? Test (rax > 0xffffffff)
	#	I''m quite sure we don''t need this, since our TC is one thunk
	#
	mov	dword ptr [%rdi-4], %eax
	jmp	%rdx

.balign 16
##############################################################################################
##
##	IN: %edi new client pc (effective address)
##	    %rsi **cpu
##	    %edx cpusize
##
EXPORT(ppc_start_jitc_asm):
	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15
	

	mov	%rax, [%rsi]
	sub	%rsp, %rdx
	mov	%rcx, %rsp
	mov	[%rsi], %rsp
	
	mov	%r15, %rdi
	mov	%r14, %rdx
	mov	%r13, %rcx
	mov	%r12, %rax
	lea	%rdi, [%rsp-8]
call EXTERN(jitc_error2)
	mov	%rdi, %r15
	mov	%rdx, %r14
	mov	%rcx, %r13
	mov	%rax, %r12
	
	push	%rdx
	1:
		mov	%r8, [%rax]
		mov	[%rcx], %r8
		add	%rax, 8
		add	%rcx, 8
		sub	%edx, 8
	jnz	1b
	
#	mov	%r14, %rsi
#	mov	%r13, %rdi	
#	mov	%rdi, %rsp
#call EXTERN(jitc_error2)
#	mov	%rdi, %r13
#	mov	%rsi, %r14

#	mov	%rdi, %rsi

#	mov	%r13, %rdi	
#	mov	%rdi, %rsp
#call EXTERN(jitc_error)
#	mov	%rdi, %r13

	mov	%eax, %edi
	jmp	EXTERN(ppc_new_pc_asm)

.balign 16
##############################################################################################
##
##	call per JMP
##
ppc_stop_jitc_asm:
	pop	%rcx
	add	%rsp, %rcx	
	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbp
	pop	%rbx
	ret

##############################################################################################
##
##	IN: %edi cpuid level
##	    %rsi dest
##

EXPORT(ppc_cpuid_asm):
	push	%rbx
	mov	%eax, %edi
	cpuid
	mov	[%rsi], %eax
	mov	[%rsi+4], %ecx
	mov	[%rsi+8], %edx
	mov	[%rsi+12], %ebx
	pop	%rbx
	mov	%eax, 1
	ret
