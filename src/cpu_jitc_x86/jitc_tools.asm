;
;	PearPC
;	jitc_tools.asm
;
;	Copyright (C) 2003, 2004 Sebastian Biallas (sb@biallas.net)
;
;	This program is free software; you can redistribute it and/or modify
;	it under the terms of the GNU General Public License version 2 as
;	published by the Free Software Foundation.
;
;	This program is distributed in the hope that it will be useful,
;	but WITHOUT ANY WARRANTY; without even the implied warranty of
;	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;	GNU General Public License for more details.
;
;	You should have received a copy of the GNU General Public License
;	along with this program; if not, write to the Free Software
;	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
;

;;	Define this if you want exact handling of the SO bit.
;;
;%define EXACT_SO

struc PPC_CPU_State
	dummy:	resd  1
        gpr:	resd 32
	fpr:	resq 32
	cr:	resd  1
	fpscr:	resd  1
	xer:	resd  1
	xer_ca:	resd  1
	lr:	resd  1
	ctr:	resd  1

	msr:	resd  1
	pvr:	resd  1
	
	ibatu:	resd  4
	ibatl:	resd  4
	ibat_bl17:	resd  4
	
	dbatu:	resd  4
	dbatl:	resd  4
	dbat_bl17:	resd  4
	
	sdr1:	resd  1
	
	sr:	resd 16

	dar:	resd  1
	dsisr:	resd  1
	sprg:	resd  4
	srr0:	resd  1
	srr1:	resd  1

	decr:	resd  1
	ear:	resd  1
	pir:	resd  1
	tb:	resq  1

	hid:	resd  16

	pc:	resd  1
	npc:	resd  1
	current_opc: resd 1
	
	exception_pending: resb 1
	dec_exception: resb 1
	ext_exception: resb 1
	stop_exception: resb 1
	singlestep_ignore: resb 1
	align1: resb 1
	align2: resb 1
	align3: resb 1
	
	pagetable_base: resd 1
	pagetable_hashmask: resd 1
	reserve: resd 1
	have_reservation: resd 1
	
	tlb_last: resd 1
	tlb_pa: resd 4
	tlb_va: resd 4
	
	effective_code_page: resd 1
	physical_code_page: resd 1

	temp: resd 1
	temp2: resd 1
	pc_ofs: resd 1
	start_pc_ofs: resd 1
	current_code_base: resd 1
	check_intr: resd 1
endstruc

extern gCPU, gMemory, jitc_error, jitc_error_program
extern jitcNewPC
extern ppc_mmu_tlb_invalidate_all_asm
extern jitc_error_msr_unsupported_bits
extern ppc_effective_to_physical_code
extern pic_check_interrupt
extern ppc_display_jitc_stats

global ppc_isi_exception_asm, ppc_dsi_exception_asm
global ppc_sc_exception_asm, ppc_no_fpu_exception_asm
global ppc_program_exception_asm
global ppc_flush_carry_and_flags_asm, ppc_flush_flags_asm
global ppc_flush_flags_signed_even_asm
global ppc_flush_flags_signed_odd_asm
global ppc_flush_flags_unsigned_even_asm
global ppc_flush_flags_unsigned_odd_asm
global ppc_new_pc_asm
global ppc_new_pc_rel_asm
global ppc_set_msr_asm
global ppc_start_jitc_asm
global ppc_cpu_atomic_raise_dec_exception
global ppc_cpu_atomic_raise_ext_exception
global ppc_cpu_atomic_cancel_ext_exception
global ppc_new_pc_this_page_asm
global ppc_heartbeat_ext_asm
global ppc_heartbeat_ext_rel_asm
global ppc_cpuid_asm

align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
ppc_flush_carry_and_flags_asm:
	jc	.carry
	call	ppc_flush_flags_asm
	and	byte [gCPU+xer+3], ~(1<<5)
	ret
.carry
	call	ppc_flush_flags_asm
	or	byte [gCPU+xer+3], (1<<5)
	ret

align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
%macro handle_so 0
%ifdef EXACT_SO
	test	byte [gCPU+xer+3], 1<<7
	jnz	.so
%endif
%endmacro

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
ppc_flush_flags_asm:
	js	.lt
	jnz	.gt
.eq:
	and	byte [gCPU+cr+3], 0x0f
	or	byte [gCPU+cr+3], 1<<5
	handle_so	
	ret
.gt:
	and	byte [gCPU+cr+3], 0x0f
	or	byte [gCPU+cr+3], 1<<6
	handle_so	
	ret
.lt:
	and	byte [gCPU+cr+3], 0x0f
	or	byte [gCPU+cr+3], 1<<7
	handle_so	
	ret
%ifdef EXACT_SO
.so:
	or	byte [gCPU+cr+3], 1<<4
	ret
%endif

align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	called after "cmp crX, ..", with X even
ppc_flush_flags_signed_even_asm:
	jl	.lt
	jg	.gt
.eq:
	and	byte [gCPU+cr+eax], 0x0f
	or	byte [gCPU+cr+eax], 1<<5
	handle_so	
	ret
.gt:
	and	byte [gCPU+cr+eax], 0x0f
	or	byte [gCPU+cr+eax], 1<<6
	handle_so	
	ret
.lt:
	and	byte [gCPU+cr+eax], 0x0f
	or	byte [gCPU+cr+eax], 1<<7
	handle_so	
	ret
%ifdef EXACT_SO
.so:
	or	byte [gCPU+cr+3], 1<<4
	ret
%endif

align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	called after "cmpl crX, ..", with X even
ppc_flush_flags_unsigned_even_asm:
	jb	.lt
	ja	.gt
.eq:
	and	byte [gCPU+cr+eax], 0x0f
	or	byte [gCPU+cr+eax], 1<<5
	handle_so	
	ret
.gt:
	and	byte [gCPU+cr+eax], 0x0f
	or	byte [gCPU+cr+eax], 1<<6
	handle_so	
	ret
.lt:
	and	byte [gCPU+cr+eax], 0x0f
	or	byte [gCPU+cr+eax], 1<<7
	handle_so	
	ret
%ifdef EXACT_SO
.so:
	or	byte [gCPU+cr+3], 1<<4
	ret
%endif

align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	called after "cmp crX, ..", with X odd
ppc_flush_flags_signed_odd_asm:
	jl	.lt
	jg	.gt
.eq:
	and	byte [gCPU+cr+eax], 0xf0
	or	byte [gCPU+cr+eax], 1<<1
	handle_so	
	ret
.gt:
	and	byte [gCPU+cr+eax], 0xf0
	or	byte [gCPU+cr+eax], 1<<2
	handle_so	
	ret
.lt:
	and	byte [gCPU+cr+eax], 0xf0
	or	byte [gCPU+cr+eax], 1<<3
	handle_so	
	ret
%ifdef EXACT_SO
.so:
	or	byte [gCPU+cr+3], 1<<4
	ret
%endif

align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	called after "cmpl crX, ..", with X odd
ppc_flush_flags_unsigned_odd_asm:
	jb	.lt
	ja	.gt
.eq:
	and	byte [gCPU+cr+eax], 0xf0
	or	byte [gCPU+cr+eax], 1<<1
	handle_so	
	ret
.gt:
	and	byte [gCPU+cr+eax], 0xf0
	or	byte [gCPU+cr+eax], 1<<2
	handle_so	
	ret
.lt:
	and	byte [gCPU+cr+eax], 0xf0
	or	byte [gCPU+cr+eax], 1<<3
	handle_so	
	ret
%ifdef EXACT_SO
.so:
	or	byte [gCPU+cr+3], 1<<4
	ret
%endif

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	ppc_set_msr_asm
;;
;;	IN: eax: new msr
;;
singlestep_error: db	'Singlestep support not implemented yet',10,0
align 16
ppc_set_msr_asm:
	test	eax, (1<<10)	; MSR_SE
	jnz	.singlestep
	test	eax, ~((1<<18)|(1<<30)|(1<<27)|(1<<25)|(1<<15)|(1<<14)|(1<<13)|(1<<12)|(1<<11)|(1<<10)|(1<<8)|(1<<5)|(1<<4)|(1<<1))
	jnz	.unsupported_msr_bits
	test	eax, (1<<18)	; MSR_POW
	jnz	.power
.power_back:
		;; Do this first so the invalidate can clobber eax and
		;; we won't care
 	mov	[gCPU+msr], eax
		;; See if the privilege level is changing (MSR_PR) to
		;; non-privileged, in which case we need to inval the tlb
	test	eax, (1<<14)
	jnz	.msr_pr_change
.msr_pr_change_back:
	ret
	
.power:
	;;call	doze
	and	eax, ~(1<<18)
	jmp	.power_back
	
.msr_pr_change:
	call ppc_mmu_tlb_invalidate_all_asm
	jmp .msr_pr_change_back

.singlestep:
	mov	eax, singlestep_error
	jmp	jitc_error

.unsupported_msr_bits:
	jmp	jitc_error_msr_unsupported_bits

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%macro ppc_atomic_raise_ext_exception_macro 0
	lock or	dword [gCPU+exception_pending], 0x00010001
%endmacro

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%macro ppc_atomic_cancel_ext_exception_macro 0
	mov	eax, [gCPU+exception_pending]
%%retry:
	test	eax, 0x00000100			; dec_exception
	mov	ebx, eax
	setnz	bl
	and	ebx, 0x00000101
	lock cmpxchg	dword [gCPU+exception_pending], ebx
	jne	%%retry
%endmacro

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%macro ppc_atomic_raise_dec_exception_macro 0
	lock or	dword [gCPU+exception_pending], 0x00000101
%endmacro

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
%macro ppc_atomic_cancel_dec_exception_macro 0
	mov	eax, [gCPU+exception_pending]
%%retry:
	test	eax, 0x00010000			; ext_exception
	mov	ebx, eax
	setnz	bl
	and	ebx, 0x00010001
	lock cmpxchg	dword [gCPU+exception_pending], ebx
	jne	%%retry
%%ext_exc:
%endmacro

align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ppc_cpu_atomic_raise_dec_exception:
	ppc_atomic_raise_dec_exception_macro
	ret
	
align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ppc_cpu_atomic_raise_ext_exception:
	ppc_atomic_raise_ext_exception_macro
	ret

align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
ppc_cpu_atomic_cancel_ext_exception:
	ppc_atomic_cancel_ext_exception_macro
	ret
	
align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	ppc_dsi_exception
;;
;;	IN: eax fault addr
;;	    ecx dsisr bits
;;
;;	does not return, so call this per JMP
ppc_dsi_exception_asm:
	mov	[gCPU+dar], eax
	call	ppc_heartbeat_asm
	mov	edx, [gCPU+pc_ofs]
	mov	eax, [gCPU+msr]
	add	edx, [gCPU+current_code_base]
	and	eax, 0x87c0ffff
	mov	[gCPU+dsisr], ecx
	mov	[gCPU+srr1], eax
	mov	[gCPU+srr0], edx
	xor	eax, eax
	call	ppc_set_msr_asm
	xor	eax, eax
	mov	[gCPU+current_code_base], eax
	mov	eax, 0x300	; entry of DSI exception
	mov	[gCPU+start_pc_ofs], eax
	jmp	ppc_new_pc_intern

align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	ppc_isi_exception_asm
;;
;;	IN: eax: fault addr
;;	    ecx: srr1 bits
;;
;;	does not return, so call this per JMP
ppc_isi_exception_asm:
	mov	[gCPU+srr0], eax
	; this can only be called from new_pc
	; so no heartbeat here
	mov	eax, [gCPU+msr]
	and	eax, 0x87c0ffff
	or	eax, ecx
	mov	[gCPU+srr1], eax
	xor	eax, eax
	call	ppc_set_msr_asm
	xor	eax, eax
	mov	[gCPU+current_code_base], eax
	mov	eax, 0x400	; entry of ISI exception
	mov	[gCPU+start_pc_ofs], eax
	jmp	ppc_new_pc_intern
	
align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;;	IN: 
;;          eax: current pc
;;
;;	this is only called indirectly
ppc_ext_exception_asm:
	; no heartbeat here 
	mov	[gCPU+srr0], eax
	mov	edx, [gCPU+msr]
	ppc_atomic_cancel_ext_exception_macro
	and	edx, 0x87c0ffff
	xor	eax, eax
	mov	[gCPU+srr1], edx
	call	ppc_set_msr_asm
	xor	eax, eax
	mov	[gCPU+current_code_base], eax
	mov	eax, 0x500	; entry of ext int exception
	mov	[gCPU+start_pc_ofs], eax
	jmp	ppc_new_pc_intern

align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;;	IN: ecx: srr1 bits
;;          esi: pc_ofs
;;
;;	does not return, so call this per JMP
ppc_program_exception_asm:
	pusha
	mov	eax, ecx
	call	jitc_error_program
	popa
	mov	[gCPU+pc_ofs], esi
	call	ppc_heartbeat_asm
	mov	eax, [gCPU+msr]
	mov	edx, esi
	and	eax, 0x87c0ffff
	add	edx, [gCPU+current_code_base]
	or	eax, ecx
	mov	[gCPU+srr0], edx
	mov	[gCPU+srr1], eax
	xor	eax, eax
	call	ppc_set_msr_asm
	xor	eax, eax
	mov	[gCPU+current_code_base], eax
	mov	eax, 0x700	; entry of program exception
	mov	[gCPU+start_pc_ofs], eax
	jmp	ppc_new_pc_intern

align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;;	IN:
;;          esi: pc_ofs
;;
;;	does not return, so call this per JMP
ppc_no_fpu_exception_asm:
	mov	edx, esi
	mov	[gCPU+pc_ofs], esi
	call	ppc_heartbeat_asm
	mov	eax, [gCPU+msr]
	add	edx, [gCPU+current_code_base]
	and	eax, 0x87c0ffff
	mov	[gCPU+srr0], edx
	mov	[gCPU+srr1], eax
	xor	eax, eax
	call	ppc_set_msr_asm
	xor	eax, eax
	mov	[gCPU+current_code_base], eax
	mov	eax, 0x800	; entry of no fpu exception
	mov	[gCPU+start_pc_ofs], eax
	jmp	ppc_new_pc_intern

align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;;	IN:
;;          eax: current pc
;;
;;	this is only called indirectly
ppc_dec_exception_asm:
	; no heartbeat here
	mov	[gCPU+srr0], eax
	mov	edx, [gCPU+msr]
	ppc_atomic_cancel_dec_exception_macro
	and	edx, 0x87c0ffff
	xor	eax, eax
	mov	[gCPU+srr1], edx
	call	ppc_set_msr_asm
	xor	eax, eax
	mov	[gCPU+current_code_base], eax
	mov	eax, 0x900	; entry of decrementer exception
	mov	[gCPU+start_pc_ofs], eax
	jmp	ppc_new_pc_intern

align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;;	IN:
;;          esi: pc_ofs
;;
;;	does not return, so call this per JMP
ppc_sc_exception_asm:
	mov	edx, esi
	mov	[gCPU+pc_ofs], esi
	call	ppc_heartbeat_asm
	mov	eax, [gCPU+msr]
	add	edx, [gCPU+current_code_base]
	and	eax, 0x87c0ffff
	mov	[gCPU+srr0], edx
	mov	[gCPU+srr1], eax
	xor	eax, eax
	call	ppc_set_msr_asm
	xor	eax, eax
	mov	[gCPU+current_code_base], eax
	mov	eax, 0xc00	; entry of SC exception
	mov	[gCPU+start_pc_ofs], eax
	jmp	ppc_new_pc_intern

align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	ppc_heartbeat_asm
;;
;;
ppc_heartbeat_asm:
	ret
;	mov	eax, [gCPU+pc_ofs]
;	sub	eax, [gCPU+start_pc_ofs]
;	shr	eax, 2
;	inc	eax
;	add	[gCPU+tb], eax
;	adc	dword [gCPU+tb+4], 0
;	sub	[gCPU+decr], eax
;	jb	.set_dec_exc
;	ret
;.set_dec_exc:
;	ppc_atomic_raise_dec_exception_macro
;	ret
	
align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	ppc_heartbeat_ext_rel_asm
;;
;;
ppc_heartbeat_ext_rel_asm:
	mov	ecx, esi
	sub	ecx, [gCPU+start_pc_ofs]
	shr	ecx, 2
	inc	ecx
	sub	[gCPU+check_intr], ecx
	mov	[gCPU+start_pc_ofs], eax
	jb	.check_intr
.back2:
;	add	[gCPU+tb], ecx
	mov	[gCPU+pc_ofs], eax
;	adc	dword [gCPU+tb+4], 0
;	sub	[gCPU+decr], ecx
;	jb	.set_dec_exception
	test	byte [gCPU+exception_pending], 1
	jnz	.handle_exception
.back:
	ret
.check_intr:
	pusha
;	call	ppc_display_jitc_stats
	call	pic_check_interrupt
	mov	dword [gCPU+check_intr], 0x3ffff
	test	eax, eax
	popa
	jz	.back2
;	ppc_atomic_raise_ext_exception_macro
	jmp	.back2
;.set_dec_exception:
;	ppc_atomic_raise_dec_exception_macro
.handle_exception:
	test	byte [gCPU+stop_exception], 1
	jnz	.stop
	test	byte [gCPU+msr+1], 1<<7		; MSR_EE
	jz	.back
	add	esp, 4
	add	eax, [gCPU+current_code_base]
	test	byte [gCPU+ext_exception], 1
	jnz	ppc_ext_exception_asm
	test	byte [gCPU+dec_exception], 1
	jnz	ppc_dec_exception_asm
	mov	eax, exception_error
	jmp	jitc_error
.stop:
	mov	eax, stop
	jmp	jitc_error
	
align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	ppc_heartbeat_ext_asm
;;
;;
ppc_heartbeat_ext_asm:
	mov	ecx, esi
	mov	ebx, eax
	sub	ecx, [gCPU+start_pc_ofs]
	mov	edx, eax
	shr	ecx, 2
	and	ebx, 0x00000fff
	inc	ecx
	and	edx, 0xfffff000
	sub	[gCPU+check_intr], ecx
	mov	[gCPU+start_pc_ofs], ebx
	jb	.check_intr
.back2:
;	add	[gCPU+tb], ecx
	mov	[gCPU+pc_ofs], ebx
;	adc	dword [gCPU+tb+4], 0
;	sub	[gCPU+decr], ecx
	mov	[gCPU+current_code_base], edx
;	jb	.set_dec_exception
	test	byte [gCPU+exception_pending], 1
	jnz	.handle_exception
.back:
	ret
.check_intr:
	pusha
;	call	ppc_display_jitc_stats
	call	pic_check_interrupt
	mov	dword [gCPU+check_intr], 0x3ffff
	test	eax, eax
	popa
	jz	.back2
;	ppc_atomic_raise_ext_exception_macro
	jmp	.back2
;.set_dec_exception:
;	ppc_atomic_raise_dec_exception_macro
.handle_exception:
	test	byte [gCPU+stop_exception], 1
	jnz	.stop
	test	byte [gCPU+msr+1], 1<<7		; MSR_EE
	jz	.back
	add	esp, 4
	test	byte [gCPU+ext_exception], 1
	jnz	ppc_ext_exception_asm
	test	byte [gCPU+dec_exception], 1
	jnz	ppc_dec_exception_asm
	mov	eax, exception_error
	jmp	jitc_error
.stop:
	mov	eax, stop
	jmp	jitc_error
stop: db 'Stopped',10,0
exception_error: db	'Unknown exception signaled?!',10,0


align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	ppc_new_pc_rel_asm
;;
;;	IN: eax new client pc relative
;;	    esi current client pc offset
;;
;;	does not return, so call this per JMP
ppc_new_pc_rel_asm:
	add	eax, [gCPU+current_code_base]
	call	ppc_heartbeat_ext_asm
	push	0
	call	ppc_effective_to_physical_code
	call	jitcNewPC
	jmp	eax
	
align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	ppc_new_pc_asm
;;
;;	IN: eax new client pc (effective address)
;;	    esi current client pc_ofs
;;
;;	does not return, so call this per JMP
ppc_new_pc_asm:
	call	ppc_heartbeat_ext_asm
	push	0
	call	ppc_effective_to_physical_code
	call	jitcNewPC
	jmp	eax

align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;;
ppc_new_pc_this_page_asm:
	add	eax, [gCPU+current_code_base]
	mov	esi, [gCPU+pc_ofs]
	push	4
	call	ppc_effective_to_physical_code
	call	jitcNewPC
	pop	edi
;	db	0xcc
	;	now eax and edi are both native addresses
	;	eax is dest and edi is source
	;
	;	we assume that there is a "mov eax, xxx" instruction before
	;	calling this function, and note that 5 is also the length of a jmp xxx
	;	so we patch edi-10
	mov	edx, eax
	sub	edi, 5
	mov	byte [edi-5], 0xe9
	sub	eax, edi
	mov	dword [edi-4], eax
	jmp	edx
align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;;	IN: eax new client pc (physical address)
;;
ppc_new_pc_intern:
	call	jitcNewPC
	jmp	eax

align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;;	IN: eax new client pc (effective address)
;;
ppc_start_jitc_asm:
	mov	esi, eax
	and	esi, 0xfff
	mov	[gCPU+start_pc_ofs], esi
	jmp	ppc_new_pc_asm

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;;	IN: eax cpuid level
;;	    edx dest
;;
ppc_cpuid_asm:
	push	ebx
	push	edx
	cpuid
	pop	edi
	mov	[edi], eax
	mov	[edi+4], ecx
	mov	[edi+8], edx
	mov	[edi+12], ebx
	pop	ebx
	ret

end
