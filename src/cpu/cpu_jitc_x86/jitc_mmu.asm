;
;	PearPC
;	jitc_mmu.asm
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

%define TLB_ENTRIES 32


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
	pdec: resd 2
	ptb: resd 2

	temp: resd 1
	temp2: resd 1
	x87cw: resd 1
	pc_ofs: resd 1
	current_code_base: resd 1
endstruc

struc JITC
	clientPages resd 1
	
	tlb_code_0_eff resd TLB_ENTRIES
	tlb_data_0_eff resd TLB_ENTRIES
	tlb_data_8_eff resd TLB_ENTRIES
	tlb_code_0_phys resd TLB_ENTRIES
	tlb_data_0_phys resd TLB_ENTRIES
	tlb_data_8_phys resd TLB_ENTRIES
	resd 1
	tlb_code_0_hits resq 1
	tlb_data_0_hits resq 1
	tlb_data_8_hits resq 1
	tlb_code_0_misses resq 1
	tlb_data_0_misses resq 1
	tlb_data_8_misses resq 1

	nativeReg resd 8        ; FIXME: resb?
	
	nativeRegState resd 8   ; FIXME: resb?
	
	nativeFlags resd 1

	nativeFlagsState resd 1
	nativeCarryState resd 1
	
	clientReg resd 600
	
	nativeRegsList resd 8
		 
	LRUreg resd 1
	MRUreg resd 1

	LRUpage resd 1
	MRUpage resd 1

	freeFragmentsList resd 1

	freeClientPages resd 1
	
	translationCache resd 1	
endstruc

extern gCPU, gJITC, gMemory, gMemorySize, 
extern jitc_error, ppc_isi_exception_asm, ppc_dsi_exception_asm
extern jitcDestroyAndFreeClientPage
extern io_mem_read_glue
extern io_mem_write_glue
extern io_mem_read64_glue
extern io_mem_write64_glue
global ppc_effective_to_physical_code, ppc_effective_to_physical_data
global ppc_write_effective_byte_asm
global ppc_write_effective_half_asm
global ppc_write_effective_word_asm
global ppc_write_effective_dword_asm
global ppc_read_effective_byte_asm
global ppc_read_effective_half_z_asm
global ppc_read_effective_half_s_asm
global ppc_read_effective_word_asm
global ppc_read_effective_dword_asm
global ppc_mmu_tlb_invalidate_all_asm
global ppc_mmu_tlb_invalidate_entry_asm
global ppc_opc_lswi_asm
global ppc_opc_stswi_asm
global ppc_opc_icbi_asm

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
ppc_mmu_tlb_invalidate_all_asm:
	cld
	or	eax, -1
	mov	ecx, TLB_ENTRIES*4*3 / 4
	mov	edi, gJITC+tlb_code_0_eff
	rep	stosd
	ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;;	IN: 	eax: effective address to invalidate
;;
ppc_mmu_tlb_invalidate_entry_asm:
	mov	ecx, eax
	or	eax, -1
	shr	ecx, 12
	and	ecx, TLB_ENTRIES-1
	mov	[gJITC+tlb_code_0_eff+ecx*4], eax
	mov	[gJITC+tlb_data_0_eff+ecx*4], eax
	mov	[gJITC+tlb_data_8_eff+ecx*4], eax
	ret
	
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;		read_physical_word_pg
%macro read_physical_word_pg 2
	mov	%2, [gMemory]
	mov	%2, [%2+%1]
	bswap	%2
%endmacro

align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;		ppc_pte_protection
ppc_pte_protection:

	;; read(0)/write(1)*8 | key*4 | pp
	
	;; read
	db 1 ; r/w
	db 1 ; r/w
	db 1 ; r/w
	db 1 ; r
	db 0 ; -
	db 1 ; r
	db 1 ; r/w
	db 1 ; r
	
	;; write
	db 1 ; r/w
	db 1 ; r/w
	db 1 ; r/w
	db 0 ; r
	db 0 ; -
	db 0 ; r
	db 1 ; r/w
	db 0 ; r

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;		bat_lookup
%macro bat_lookup 4
	mov	edx, [gCPU + %1bat_bl17 + %2*4]
	mov	ecx, edx
	or	ecx, 0xf001ffff
	and	ecx, eax
	mov	ebx, [gCPU + %1batu + %2*4]
	and	ecx, 0xfffe0000 ; BATU_BEPI
	and	ebx, 0xfffe0000 ; BATU_BEPI
	cmp	ebx, ecx
	jne	%%bat_lookup_failed
	
	mov	ecx, [gCPU + msr]
	mov	ebx, [gCPU + %1batu + %2*4]
	test	ecx, (1<<14)  ; MSR_PR

	jz	%%npr
			test	ebx, 1 ; BATU_Vp
			jz	%%bat_lookup_failed
			jmp	%%ok
	%%npr:
			test	ebx, 2 ; BATU_Vs
			jz	%%bat_lookup_failed
	%%ok:

	mov	ecx, eax
	mov	esi, eax
	mov	edi, eax
	and	ecx, 0x1ffff ; BAT_EA_OFFSET
	not	edx
	and	eax, 0x0ffe0000 ; BAT_EA_11
	and	eax, edx
	mov	ebx, [gCPU + %1batl + %2*4]
	and	ebx, 0xfffe0000
	or	eax, ebx
	or	eax, ecx
;;; TLB-Code
	and	esi, 0xfffff000
	shr	edi, 12
	mov	edx, eax
	and	edi, TLB_ENTRIES-1
	and	edx, 0xfffff000
	mov	[gJITC+tlb_%4_%3_eff+edi*4], esi
	mov	[gJITC+tlb_%4_%3_phys+edi*4], edx
;;	add	dword [gJITC+tlb_%4_%3_misses], 1
;;	adc	dword [gJITC+tlb_%4_%3_misses+4], 0
;;;	
	ret	4
%%bat_lookup_failed:
%endmacro

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	pg_table_lookup
;;
;;	param1: PTE1_H or 0
;;	param2: 0 for read, 8 for write
;;	param3: data / code
%macro pg_table_lookup 3
	read_physical_word_pg ebx, ecx	
	; ecx = pte1
	
	mov	eax, ecx
	and	eax, (1<<6) | (1<<31)		; (PTE1_V | PTE1_H)
	cmp	eax, (%1) | (1<<31)
	jne	%%invalid
	
	mov	eax, ecx
	shr	eax, 7
	and	eax, 0xffffff	; VSID
	cmp	eax, ebp
	jne	%%invalid
	
	and	ecx, 0x3f	; API
	cmp	ecx, edi
	jne	%%invalid

	; page found
	
	add	ebx, 4
	read_physical_word_pg ebx, esi
	; esi = pte2
	
	test	dword [gCPU + msr], (1<<14) ; MSR_PR
	mov	eax, (1<<29)	; SR_Kp
	setz	cl
	shl	eax, cl		; SR_Kp <--> SR_Ks
	test	edx, eax	; SR_Kp / SR_Ks
	mov	eax, 0
	setnz	al
	shl	eax, 2
	
	mov	ecx, esi
	and	ecx, 3
	; FIXME: optimize: use eax*4:
	cmp	byte [ppc_pte_protection + (%2) + eax + ecx], 1
%if %1==0
;	add	esp, 4		; hash1, no longer needed
	pop	edx
%endif
	pop	eax		; the effective address
	jne	protection_fault_%2_%3
	
	;;	update R and C bits
	;;	FIXME: is someone using this?
	mov	edx, esi
%if %2==0
	or	edx, (1<<8)		; PTE2_R
%else
	or	edx, (1<<8) | (1<<7)	; PTE2_R | PTE2_C
%endif
	bswap	edx
	add	ebx, [gMemory]
	mov	[ebx], edx
	;;
	
	and	esi, 0xfffff000
;;; TLB-Code
	mov	edx, eax
	mov	ecx, eax
	shr	edx, 12
	and	ecx, 0xfffff000
	and	edx, TLB_ENTRIES-1
	mov	[gJITC+tlb_%3_%2_eff+edx*4], ecx
	mov	[gJITC+tlb_%3_%2_phys+edx*4], esi
;;	add	dword [gJITC+tlb_%3_%2_misses], 1
;;	adc	dword [gJITC+tlb_%3_%2_misses+4], 0
;;;
	and	eax, 0x00000fff
	or	eax, esi
	ret	4		; yipee
%%invalid:
	; advance to next pteg entry
	add	ebx, 8
%endmacro

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	protection_fault_%2_%3
protection_fault_0_code:
	; ppc_exception(PPC_EXC_ISI, PPC_EXC_SRR1_PROT, addr);
	pop	edx		; return address is no longer needed
	pop	ebx		; bytes to roll back
	add	esp, ebx
	mov	ecx, (1<<27)	; PPC_EXC_SRR1_PROT
	jmp	ppc_isi_exception_asm
protection_fault_0_data:
	; ppc_exception(PPC_EXC_DSI, PPC_EXC_DSISR_PROT, addr);
	pop	edx		; return address is no longer needed
	pop	ebx		; bytes to roll back
	add	esp, ebx
	mov	ecx, (1<<27)	; PPC_EXC_DSISR_PROT
	jmp	ppc_dsi_exception_asm
protection_fault_8_data:
	; ppc_exception(PPC_EXC_DSI, PPC_EXC_DSISR_PROT | PPC_EXC_DSISR_STORE, addr);
	pop	edx			; return address is no longer needed
	pop	ebx			; bytes to roll back
	add	esp, ebx
	mov	ecx, (1<<27) | (1<<25)	; PPC_EXC_DSISR_PROT | PPC_EXC_DSISR_STORE
	jmp	ppc_dsi_exception_asm

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	param1: 0 for read, 8 for write
;;	param2: data / code
%macro tlb_lookup 2
	mov	edx, eax
	mov	ecx, eax
	shr	edx, 12
	and	ecx, 0xfffff000
	and	edx, TLB_ENTRIES-1
	cmp	ecx, [gJITC+tlb_%2_%1_eff+edx*4]
	jne	%%tlb_lookup_failed
	;
	;	if an tlb entry is invalid, its 
	;	lower 12 bits are 1, so the cmp is guaranteed to fail.
	;
;;	add	dword [gJITC+tlb_%2_%1_hits], 1
;;	adc	dword [gJITC+tlb_%2_%1_hits+4], 0
	and	eax, 0x00000fff
	or	eax, [gJITC+tlb_%2_%1_phys+edx*4]
	ret	4
%%tlb_lookup_failed:
%endmacro

align 16
ppc_effective_to_physical_ret:
	ret	4

align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	uint32 FASTCALL ppc_effective_to_physical_code(uint32 addr)
;; 
;;	IN	eax: address to translate
;; 
;;	WILL NOT RETURN ON EXCEPTION!
;;
ppc_effective_to_physical_code:
	; if (!gCPU.msr & MSR_IR) this should be patched to "ret"
	test	byte [gCPU+msr], (1<<5)	; MSR_IR
	jz	ppc_effective_to_physical_ret

	tlb_lookup 0, code

	; FIXME: self-modifying code would be better
	bat_lookup i, 0, 0, code
	bat_lookup i, 1, 0, code
	bat_lookup i, 2, 0, code
	bat_lookup i, 3, 0, code

	mov	ebx, eax
	shr	ebx, 28			; SR
	mov	edx, [gCPU+sr+4*ebx]
	
	; test	edx, SR_T --> die
	
	test	edx, (1<<28)		; SR_N
	jnz	.noexec
	
	mov	ebx, eax
	mov 	ebp, edx
	shr	ebx, 12
	mov	edi, eax
	and	ebx, 0xffff
	shr	edi, 22
	and	ebp, 0xffffff
	and	edi, 0x3f
	
	; now:
	; eax = addr
	; ebx = page_index
	; ebp = VSID
	; edi = api
	
	xor	ebx, ebp
	
	; ebx = hash1
	
	push	eax
	push	ebx			; das brauch ich
	
	and	ebx, [gCPU+pagetable_hashmask]
	shl	ebx, 6
	or	ebx, [gCPU+pagetable_base]
	
	; ebx = pteg_addr
	
	pg_table_lookup 0, 0, code
	pg_table_lookup 0, 0, code
	pg_table_lookup 0, 0, code
	pg_table_lookup 0, 0, code
	pg_table_lookup 0, 0, code
	pg_table_lookup 0, 0, code
	pg_table_lookup 0, 0, code
	pg_table_lookup 0, 0, code
	
	; hash function number 2
	pop	ebx
	not	ebx
	and	ebx, [gCPU+pagetable_hashmask]
	shl	ebx, 6
	or	ebx, [gCPU+pagetable_base]

	pg_table_lookup (1<<6), 0, code
	pg_table_lookup (1<<6), 0, code
	pg_table_lookup (1<<6), 0, code
	pg_table_lookup (1<<6), 0, code
	pg_table_lookup (1<<6), 0, code
	pg_table_lookup (1<<6), 0, code
	pg_table_lookup (1<<6), 0, code
	pg_table_lookup (1<<6), 0, code

	; page lookup failed --> throw exception
	
	pop	eax
	pop	edx			; return address is no longer needed
	pop	ecx			; bytes to roll back
	add	esp, ecx

	mov	ecx, (1<<30)		; PPC_EXC_SRR1_PAGE
	jmp	ppc_isi_exception_asm
.noexec:
	; segment isnt executable --> throw exception
	pop	edx			; return address is no longer needed
	pop	ecx			; bytes to roll back
	add	esp, ecx

	mov	ecx, (1<<28)		; PPC_EXC_SRR1_GUARD
	jmp	ppc_isi_exception_asm

align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	uint32 FASTCALL ppc_effective_to_physical_data_read(uint32 addr)
;; 
;;	IN	eax: address to translate
;; 
;;	WILL NOT RETURN ON EXCEPTION!
;;
ppc_effective_to_physical_data_read:
	; if (!gCPU.msr & MSR_DR) this should be patched to "ret"
	
	test	byte [gCPU+msr], (1<<4)	; MSR_DR
	jz	ppc_effective_to_physical_ret
	
	tlb_lookup 0, data

	; FIXME: self-modifying code would be better
	bat_lookup d, 0, 0, data
	bat_lookup d, 1, 0, data
	bat_lookup d, 2, 0, data
	bat_lookup d, 3, 0, data

	mov	ebx, eax
	shr	ebx, 28			; SR
	mov	edx, [gCPU+sr+4*ebx]
	
	; test edx, SR_T --> die
	
	mov	ebx, eax
	mov	ebp, edx
	shr	ebx, 12
	mov	edi, eax
	and	ebx, 0xffff
	shr	edi, 22
	and	ebp, 0xffffff
	and	edi, 0x3f
	
	; now:
	; eax = addr
	; ebx = page_index
	; ebp = VSID
	; edi = api
	
	xor	ebx, ebp
	
	; ebx = hash1
	
	push	eax
	push	ebx			; das brauch ich
	
	and	ebx, [gCPU+pagetable_hashmask]
	shl	ebx, 6
	or	ebx, [gCPU+pagetable_base]
	
	; ebx = pteg_addr
	
	pg_table_lookup 0, 0, data
	pg_table_lookup 0, 0, data
	pg_table_lookup 0, 0, data
	pg_table_lookup 0, 0, data
	pg_table_lookup 0, 0, data
	pg_table_lookup 0, 0, data
	pg_table_lookup 0, 0, data
	pg_table_lookup 0, 0, data
	
	; hash function number 2
	pop	ebx
	not	ebx
	and	ebx, [gCPU+pagetable_hashmask]
	shl	ebx, 6
	or	ebx, [gCPU+pagetable_base]

	pg_table_lookup (1<<6), 0, data
	pg_table_lookup (1<<6), 0, data
	pg_table_lookup (1<<6), 0, data 
	pg_table_lookup (1<<6), 0, data
	pg_table_lookup (1<<6), 0, data
	pg_table_lookup (1<<6), 0, data
	pg_table_lookup (1<<6), 0, data
	pg_table_lookup (1<<6), 0, data

	; page lookup failed --> throw exception
	
	pop	eax
	pop	edx			; return address is no longer needed
	pop	ecx			; bytes to roll back
	add	esp, ecx
	
	mov	ecx, (1<<30)		; PPC_EXC_DSISR_PAGE
	jmp	ppc_dsi_exception_asm

align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	uint32 FASTCALL ppc_effective_to_physical_data_write(uint32 addr)
;;
;;	IN	eax: address to translate
;;
;;	WILL NOT RETURN ON EXCEPTION!
;;
ppc_effective_to_physical_data_write:
	; if (!gCPU.msr & MSR_DR) this should be patched to "ret"

	test	byte [gCPU+msr], (1<<4)	; MSR_DR
	jz	ppc_effective_to_physical_ret
	
	tlb_lookup 8, data

	; FIXME: self-modifying code would be better
	bat_lookup d, 0, 8, data
	bat_lookup d, 1, 8, data
	bat_lookup d, 2, 8, data
	bat_lookup d, 3, 8, data

	mov	ebx, eax
	shr	ebx, 28			; SR
	mov	edx, [gCPU+sr+4*ebx]
	
	; test edx, SR_T --> die
	
	mov	ebx, eax
	mov	ebp, edx
	shr	ebx, 12
	mov	edi, eax
	and	ebx, 0xffff
	shr	edi, 22
	and	ebp, 0xffffff
	and	edi, 0x3f
	
	; now:
	; eax = addr
	; ebx = page_index
	; ebp = VSID
	; edi = api
	
	xor	ebx, ebp
	
	; ebx = hash1
	
	push	eax
	push	ebx			; das brauch ich
	
	and	ebx, [gCPU+pagetable_hashmask]
	shl	ebx, 6
	or	ebx, [gCPU+pagetable_base]
	
	; ebx = pteg_addr
	
	pg_table_lookup 0, 8, data
	pg_table_lookup 0, 8, data
	pg_table_lookup 0, 8, data
	pg_table_lookup 0, 8, data
	pg_table_lookup 0, 8, data
	pg_table_lookup 0, 8, data
	pg_table_lookup 0, 8, data
	pg_table_lookup 0, 8, data
	
	; hash function number 2
	pop	ebx
	not	ebx
	and	ebx, [gCPU+pagetable_hashmask]
	shl	ebx, 6
	or	ebx, [gCPU+pagetable_base]

	pg_table_lookup (1<<6), 8, data
	pg_table_lookup (1<<6), 8, data
	pg_table_lookup (1<<6), 8, data 
	pg_table_lookup (1<<6), 8, data
	pg_table_lookup (1<<6), 8, data
	pg_table_lookup (1<<6), 8, data
	pg_table_lookup (1<<6), 8, data
	pg_table_lookup (1<<6), 8, data

	; page lookup failed --> throw exception
	
	pop	eax
	pop	edx			; return address is no longer needed
	pop	ebx			; bytes to roll back
	add	esp, ebx

	mov	ecx, (1<<30)|(1<<25)	; PPC_EXC_DSISR_PAGE | PPC_EXC_DSISR_STORE
	jmp	ppc_dsi_exception_asm

align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	uint32 FASTCALL ppc_effective_write_byte()
;;
;;	IN	eax: address to translate
;;		esi: current client pc offset
;;		 dl: byte to be written
;;
;;	WILL NOT RETURN ON EXCEPTION!
;;
ppc_write_effective_byte_asm:
	mov	[gCPU+pc_ofs], esi

	push	edx
	push	8			; roll back 8 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	cmp	eax, [gMemorySize]
	pop	edx
	jae	.mmio
	add	eax, [gMemory]
	mov	[eax], dl
	ret
.mmio:
	mov	ecx, 1
	movzx	edx, dl
	call	io_mem_write_glue
	ret
align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	uint32 FASTCALL ppc_effective_write_half()
;;
;;	IN	eax: address to translate
;;		 dx: half to be written
;;		esi: current client pc offset
;;
;;	WILL NOT RETURN ON EXCEPTION!
;;
ppc_write_effective_half_asm:
	mov	[gCPU+pc_ofs], esi
	mov	ebx, eax
	and	ebx, 0xfff
	cmp	ebx, 4095
	jae	.overlap

	push	edx
	push	8			; roll back 8 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	cmp	eax, [gMemorySize]
	pop	edx
	jae	.mmio
	xchg	dh, dl
	add	eax, [gMemory]
	mov	[eax], dx
	ret
.mmio:
	mov	ecx, 2
	movzx	edx, dx
	call	io_mem_write_glue
	ret
.overlap:
	push	edx
	push	eax
	push	12			; roll back 12 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	mov	ebx, eax
	pop	eax
	push	ebx
	inc	eax
	push	12			; roll back 12 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	pop	ebx
	pop	edx
	cmp	ebx, [gMemorySize]
	jae	.overlapped_mmio_1
	add	ebx, [gMemory]
	mov	[ebx], dh
	.overlapped_mmio_1_back:
	cmp	eax, [gMemorySize]
	jae	.overlapped_mmio_2
	add	eax, [gMemory]
	mov	[eax], dl
	ret

.overlapped_mmio_1:
	pusha
	movzx	edx, dh
	mov	eax, ebx
	mov	ecx, 1
	call	io_mem_write_glue
	popa
	jmp	.overlapped_mmio_1_back
.overlapped_mmio_2:
	movzx	edx, dl
	mov	ecx, 1
	call	io_mem_write_glue
	ret
align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	uint32 FASTCALL ppc_effective_write_word()
;;
;;	IN	eax: address to translate
;;		edx: word to be written
;;		esi: current client pc offset
;;
;;	WILL NOT RETURN ON EXCEPTION!
;;
ppc_write_effective_word_asm:
	mov	[gCPU+pc_ofs], esi
	mov	ebx, eax
	and	ebx, 0xfff
	cmp	ebx, 4093
	jae	.overlap

	push	edx
	push	8			; roll back 8 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	cmp	eax, [gMemorySize]
	pop	edx
	jae	.mmio
	bswap	edx
	add	eax, [gMemory]
	mov	[eax], edx
	ret
.mmio:
	mov	ecx, 4
	call	io_mem_write_glue
	ret
.overlap:
	push	edx
	push	eax
	push	12			; roll back 12 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	mov	ebx, eax
	pop	eax
	add	eax, 4
	push	ebx
	push	12			; roll back 12 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	pop	ebx
	pop	edx
	mov	ebp, ebx
	and	ebp, 0xfff
	neg	ebp
	add	ebp, 4096
	cmp	ebx, [gMemorySize]
	jae	.overlapped_mmio_1
	add	ebx, [gMemory]
	.loop1:
		rol	edx, 8
		mov	[ebx], dl
		inc	ebx
		dec	ebp
	jnz	.loop1
	.overlapped_mmio_1_back:
	mov	ebp, eax
	and	eax, 0xfffff000
	and	ebp, 0x00000fff
	cmp	eax, [gMemorySize]
	jae	.overlapped_mmio_2
	add	eax, [gMemory]
	.loop2:
		rol	edx, 8
		mov	[eax], dl
		inc	eax
		dec	ebp
	jnz	.loop2
	ret

.overlapped_mmio_1:
	.overlapped_mmio_1_loop:
		rol	edx, 8
		pusha
		movzx	edx, dl
		mov	eax, ebx
		mov	ecx, 1
		call	io_mem_write_glue
		popa
		inc	ebx
		dec	ebp
	jnz	.overlapped_mmio_1_loop
	jmp	.overlapped_mmio_1_back
.overlapped_mmio_2:
	.overlapped_mmio_2_loop:
		rol	edx, 8
		pusha
		movzx	edx, dl
		mov	ecx, 1
		call	io_mem_write_glue
		popa
		inc	eax
		dec	ebp
	jnz	.overlapped_mmio_2_loop
	ret
align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	uint32 FASTCALL ppc_effective_write_dword()
;;
;;	IN	eax: address to translate
;;		ecx:edx dword to be written
;;		esi: current client pc offset
;;
;;	WILL NOT RETURN ON EXCEPTION!
;;
ppc_write_effective_dword_asm:
	mov	[gCPU+pc_ofs], esi
	mov	ebx, eax
	and	ebx, 0xfff
	cmp	ebx, 4089
	jae	.overlap

	push	ecx
	push	edx
	push	12			; roll back 12 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	cmp	eax, [gMemorySize]
	pop	edx
	pop	ecx
	jae	.mmio
	bswap	edx
	bswap	ecx
	add	eax, [gMemory]
	mov	[eax], ecx
	mov	[eax+4], edx
	ret
.mmio:
	call	io_mem_write64_glue
	ret
.overlap:
	push	ecx
	push	edx
	push	eax
	push	16			; roll back 16 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	mov	ebx, eax
	pop	eax
	add	eax, 8
	push	ebx
	push	16			; roll back 16 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	pop	ebx
	pop	edx
	pop	ecx
	mov	ebp, ebx
	and	ebp, 0xfff
	neg	ebp
	add	ebp, 4096
	bswap	ecx
	bswap	edx
	cmp	ebx, [gMemorySize]
	jae	.overlapped_mmio_1
	add	ebx, [gMemory]
	.loop1:
		mov	[ebx], cl
		shrd	ecx, edx, 8
		inc	ebx
		shr	edx, 8
		dec	ebp
	jnz	.loop1
	.overlapped_mmio_1_back:
	mov	ebp, eax
	and	eax, 0xfffff000
	and	ebp, 0x00000fff
	cmp	eax, [gMemorySize]
	jae	.overlapped_mmio_2
	add	eax, [gMemory]
	.loop2:
		mov	[eax], cl
		shrd	ecx, edx, 8
		inc	eax
		shr	edx, 8
		dec	ebp
	jnz	.loop2
	ret
.overlapped_mmio_1:
	.overlapped_mmio_1_loop:
		pusha
		movzx	edx, cl
		mov	eax, ebx
		mov	ecx, 1
		call	io_mem_write_glue
		popa
		shrd	ecx, edx, 8
		inc	ebx
		shr	edx, 8
		dec	ebp
	jnz	.overlapped_mmio_1_loop
	jmp	.overlapped_mmio_1_back
.overlapped_mmio_2:
	.overlapped_mmio_2_loop:
		pusha
		movzx	edx, cl
		mov	ecx, 1
		call	io_mem_write_glue
		popa
		shrd	ecx, edx, 8
		inc	eax
		shr	edx, 8
		dec	ebp
	jnz	.overlapped_mmio_2_loop
	ret
align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	uint32 FASTCALL ppc_read_effective_byte()
;;
;;	IN	eax: address to translate
;;		esi: current client pc offset
;;
;;	OUT	edx: byte, zero extended
;;
;;	WILL NOT RETURN ON EXCEPTION!
;;
ppc_read_effective_byte_asm:
	mov	[gCPU+pc_ofs], esi

	push	4			; roll back 4 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	cmp	eax, [gMemorySize]
	jae	.mmio
	add	eax, [gMemory]
	movzx	edx, byte [eax]
	ret
.mmio:
	mov	edx, 1
	call	io_mem_read_glue
	movzx	edx, al
	ret

align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	uint32 FASTCALL ppc_read_effective_half()
;;
;;	IN	eax: address to translate
;;		esi: current client pc offset
;;
;;	OUT	edx: half, zero extended
;;
;;	WILL NOT RETURN ON EXCEPTION!
;;
ppc_read_effective_half_z_asm:
	mov	[gCPU+pc_ofs], esi
	mov	ebx, eax
	and	ebx, 0xfff
	cmp	ebx, 4095
	jae	.overlap

	push	4			; roll back 4 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	cmp	eax, [gMemorySize]
	jae	.mmio
	add	eax, [gMemory]	
	movzx	edx, word [eax]
	xchg	dl, dh
	ret
.mmio:
	mov	edx, 2
	call	io_mem_read_glue
	movzx	edx, ax
	ret
.overlap:
	push	eax
	push	8			; roll back 8 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	xor	edx, edx
	cmp	eax, [gMemorySize]
	jae	.mmio1
	add	eax, [gMemory]
	mov	dh, [eax]
.loop1:
	pop	eax
	push	edx
	inc	eax
	push	8			; roll back 8 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	pop	edx
	cmp	eax, [gMemorySize]
	jae	.mmio2
	add	eax, [gMemory]
	mov	dl, [eax]
	ret

.mmio1:
	pusha
	mov	edx, 1
	call	io_mem_read_glue
	mov	[gCPU+temp2], al
	popa
	mov	dh, [gCPU+temp2]
	jmp	.loop1
.mmio2:
	push	edx
	mov	edx, 1
	call	io_mem_read_glue
	pop	edx
	mov	dl, al
	ret
align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	uint32 FASTCALL ppc_read_effective_half()
;;
;;	IN	eax: address to translate
;;		esi: current client pc offset
;;
;;	OUT	edx: half, sign extended
;;
;;	WILL NOT RETURN ON EXCEPTION!
;;
ppc_read_effective_half_s_asm:
	mov	[gCPU+pc_ofs], esi
	mov	ebx, eax
	and	ebx, 0xfff
	cmp	ebx, 4095
	jae	.overlap

	push	4			; roll back 4 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	cmp	eax, [gMemorySize]
	jae	.mmio
	add	eax, [gMemory]
	mov	cx, word [eax]
	xchg	ch, cl
	movsx	edx, cx
	ret
.mmio:
	mov	edx, 2
	call	io_mem_read_glue
	movsx	edx, ax
	ret
.overlap:
	push	eax
	push	8			; roll back 8 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	cmp	eax, [gMemorySize]
	jae	.mmio1
	add	eax, [gMemory]
	mov	ch, [eax]
.loop1:
	pop	eax
	push	ecx
	inc	eax
	push	8			; roll back 8 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	pop	ecx
	cmp	eax, [gMemorySize]
	jae	.mmio2
	add	eax, [gMemory]
	mov	cl, [eax]
	movsx	edx, cx
	ret

.mmio1:
	pusha
	mov	edx, 1
	call	io_mem_read_glue
	mov	[gCPU+temp2], al
	popa
	mov	ch, [gCPU+temp2]
	jmp	.loop1
.mmio2:
	push	ecx
	mov	edx, 1
	call	io_mem_read_glue
	pop	ecx
	mov	cl, al
	movsx	edx, cx
	ret
	
align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	uint32 FASTCALL ppc_read_effective_word()
;;
;;	IN	eax: address to translate
;;		esi: current client pc offset
;;
;;	OUT	edx: word
;;
;;	WILL NOT RETURN ON EXCEPTION!
;;
ppc_read_effective_word_asm:
	mov	[gCPU+pc_ofs], esi
	mov	ebx, eax
	and	ebx, 0xfff
	cmp	ebx, 4093
	jae	.overlap

	push	4			; roll back 4 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	cmp	eax, [gMemorySize]
	jae	.mmio
	add	eax, [gMemory]
	mov	edx, [eax]
	bswap	edx
	ret
.mmio:
	mov	edx, 4
	call	io_mem_read_glue
	mov	edx, eax
	ret
.overlap:
	push	eax
	push	ebx
	push	12			; roll back 12 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	pop	ebx
	mov	ecx, 4096
	sub	ecx, ebx
	cmp	eax, [gMemorySize]
	jae	.overlapped_mmio_1
	add	eax, [gMemory]
	.loop1:
		shl	edx, 8
		mov	dl, [eax]
		inc	eax
		dec	ecx
	jnz	.loop1
	.overlapped_mmio_1_back:
	pop	eax
	push	edx
	add	eax, 4
	push	ebx
	and	eax, 0xfffff000
	push	12			; roll back 12 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	pop	ebx
	pop	edx
	sub	ebx, 4092
	cmp	eax, [gMemorySize]
	jae	.overlapped_mmio_2
	add	eax, [gMemory]
	.loop2:
		shl	edx, 8
		mov	dl, [eax]
		inc	eax
		dec	ebx
	jnz	.loop2
	ret

.overlapped_mmio_1:
	.overlapped_mmio_1_loop:
		shl	edx, 8
		pusha
		mov	edx, 1
		call	io_mem_read_glue
		mov	[gCPU+temp2], al
		popa
		mov	dl, [gCPU+temp2]
		inc	eax
		dec	ecx
	jnz	.overlapped_mmio_1_loop
	jmp	.overlapped_mmio_1_back
.overlapped_mmio_2:
	.overlapped_mmio_2_loop:
		shl	edx, 8
		pusha
		mov	edx, 1
		call	io_mem_read_glue
		mov	[gCPU+temp2], al
		popa
		mov	dl, [gCPU+temp2]
		inc	eax
		dec	ebx
	jnz	.overlapped_mmio_2_loop
	ret
align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	uint32 FASTCALL ppc_read_effective_dword()
;;
;;	IN	eax: address to translate
;;		esi: current client pc offset
;;
;;	OUT	ecx:edx dword
;;
;;	WILL NOT RETURN ON EXCEPTION!
;;
ppc_read_effective_dword_asm:
	mov	[gCPU+pc_ofs], esi
	mov	ebx, eax
	and	ebx, 0xfff
	cmp	ebx, 4089
	jae	.overlap

	push	4			; roll back 4 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	cmp	eax, [gMemorySize]
	jae	.mmio
	add	eax, [gMemory]
	mov	ecx, [eax]
	mov	edx, [eax+4]
	bswap	ecx
	bswap	edx
	ret
.mmio:
	call	io_mem_read64_glue
	mov	ecx, edx
	mov	edx, eax
	ret
.overlap:
	push	eax
	push	ebx
	push	12			; roll back 12 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	pop	ebx
	mov	ebp, 4096
	sub	ebp, ebx
	cmp	eax, [gMemorySize]
	jae	.overlapped_mmio_1
	add	eax, [gMemory]
	.loop1:
		shld	ecx, edx, 8
		shl	edx, 8
		mov	dl, [eax]
		inc	eax
		dec	ebp
	jnz	.loop1
	.overlapped_mmio_1_back:
	pop	eax
	push	ecx
	push	edx
	add	eax, 8
	push	ebx
	and	eax, 0xfffff000
	push	16			; roll back 16 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	pop	ebx
	pop	edx
	pop	ecx
	sub	ebx, 4088
	cmp	eax, [gMemorySize]
	jae	.overlapped_mmio_2
	add	eax, [gMemory]
	.loop2:
		shld	ecx, edx, 8
		shl	edx, 8
		mov	dl, [eax]
		inc	eax
		dec	ebx
	jnz	.loop2
	ret

.overlapped_mmio_1:
	.overlapped_mmio_1_loop:
		shld	ecx, edx, 8
		shl	edx, 8
		pusha
		mov	edx, 1
		call	io_mem_read_glue
		mov	[gCPU+temp2], al
		popa
		mov	dl, [gCPU+temp2]
		inc	eax
		dec	ebp
	jnz	.overlapped_mmio_1_loop
	jmp	.overlapped_mmio_1_back
.overlapped_mmio_2:
	.overlapped_mmio_2_loop:
		shld	ecx, edx, 8
		shl	edx, 8
		pusha
		mov	edx, 1
		call	io_mem_read_glue
		mov	[gCPU+temp2], al
		popa
		mov	dl, [gCPU+temp2]
		inc	eax
		dec	ebx
	jnz	.overlapped_mmio_2_loop
	ret

align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	uint32 FASTCALL ppc_opc_stswi_asm()
;;
;;	IN	ecx: NB
;;		ebx: source
;;		eax: dest
;;		esi: current client pc offset
;;
;;	WILL NOT RETURN ON EXCEPTION!
;;
ppc_opc_stswi_asm:
	mov	[gCPU+pc_ofs], esi
	mov	edi, 1
	
.loop:
	dec	edi
	jnz	.ok1
		mov	edx, [gCPU+gpr+4*ebx]
		inc	ebx
		mov	edi, 4
		and	ebx, 0x1f	
	.ok1:
	push	eax
	push	ecx
	push	ebx
	push	edi
	push	edx
	push	24			; roll back 24 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	cmp	eax, [gMemorySize]
	pop	edx
	mov	ecx, edx
	jae	.mmio
	shr	ecx, 24
	add	eax, [gMemory]
	mov	[eax], cl
.back:
	pop	edi
	pop	ebx
	pop	ecx
	pop	eax
	shl	edx, 8
	inc	eax
	dec	ecx
	jnz	.loop
	ret
.mmio:
	push	edx
	mov	ecx, 1
	shr	edx, 24
	call	io_mem_write_glue
	pop	edx
	jmp	.back
align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	uint32 FASTCALL ppc_opc_lswi_asm()
;;
;;	IN	ecx: NB
;;		ebx: dest
;;		eax: source
;;		esi: current client pc offset
;;
;;	WILL NOT RETURN ON EXCEPTION!
;;
ppc_opc_lswi_asm:
	mov	[gCPU+pc_ofs], esi
	mov	edi, 4
.loop:
	or	edi, edi
	jnz	.ok1
		mov	[gCPU+gpr+4*ebx], edx
		inc	ebx
		mov	edi, 4
		and	ebx, 0x1f
		xor	edx, edx
	.ok1:

	push	eax
	push	ecx
	push	ebx
	push	edi
	push	edx
	push	24			; roll back 24 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	pop	edx
	cmp	eax, [gMemorySize]
	jae	.mmio
	add	eax, [gMemory]
	shl	edx, 8
	mov	dl, byte [eax]
.back:
	pop	edi
	pop	ebx
	pop	ecx
	pop	eax
	
	dec	edi
	inc	eax
	dec	ecx
	jnz	.loop
	
	or	edi, edi
	jz	.ret
	.loop2:
		shl	edx, 8		
		dec	edi
	jnz	.loop2	
.ret:
	mov	[gCPU+gpr+4*ebx], edx
	ret
.mmio:
	push	edx
	mov	edx, 1
	call	io_mem_read_glue
	pop	edx
	shl	edx, 8
	mov	dl, al
	jmp	.back
align 16
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;	uint32 FASTCALL ppc_opc_icbi_asm()
;;
;;	IN	eax: effective address
ppc_opc_icbi_asm:
	push	4			; roll back 4 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	cmp	eax, [gMemorySize]
	mov	ebp, [gJITC+clientPages]
	jae	.ok
	shr	eax, 12
	cmp	dword [ebp+eax*4], 0
	jz	.ok
.destroy:
	mov	eax, [ebp+eax*4]
	call	jitcDestroyAndFreeClientPage
.ok:
	ret
end
