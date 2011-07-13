/*
 *	PearPC
 *	jitc_mmu.S
 *
 *	Copyright (C) 2003-2011 Sebastian Biallas (sb@biallas.net)
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

#define __FROM_ASM__

#include "jitc_common.h"
#include "../../io/graphic/gcard.h"

.text

.macro _pusha
	push %rax
	push %rcx
	push %rdx
	push %rbx
	push %rbp
	push %rsi
	push %rdi
	push %r8
	push %r9
	push %r10
	push %r11
	push %r12
	push %r13
	push %r14
	push %r15
.endm

.macro _popa
	pop  %r15
	pop  %r14
	pop  %r13
	pop  %r12
	pop  %r11
	pop  %r10
	pop  %r9
	pop  %r8
	pop  %rdi
	pop  %rsi
	pop  %rbp
	pop  %rbx
	pop  %rdx
	pop  %rcx
	pop  %rax
.endm

##############################################################################################
##
##		This loads the cpu-ptr into %rdi
##

.macro mmu_prologue
	#
	#	Frame 1
	#
	getCurCPU 1
	mov	[curCPU(pc_ofs)], %esi
.endm

.balign 16
##############################################################################################
##
##	IN: %rdi: cpu
##
EXPORT(ppc_mmu_tlb_invalidate_all_asm):
	checkCurCPU
	cld
	mov	%r8, %rdi
	or	%rax, -1
	mov	%ecx, TLB_ENTRIES*4*3 / 8
	add	%rdi, tlb_code_0_eff
	rep	stosq
	mov	%rdi, %r8
	ret

.balign 16
##############################################################################################
##
##	IN: 	%eax: effective address to invalidate
##		%rdi: cpu
##
EXPORT(ppc_mmu_tlb_invalidate_entry_asm):
	or	%ecx, -1
	shr	%eax, 12
	and	%eax, TLB_ENTRIES-1
	mov	[curCPU(tlb_code_0_eff) + %rax*4], %ecx
	mov	[curCPU(tlb_data_0_eff) + %rax*4], %ecx
	mov	[curCPU(tlb_data_8_eff) + %rax*4], %ecx
	ret

.balign 16
##############################################################################################
##		ppc_pte_protection
ppc_pte_protection:

	## read(0)/write(1)*8 | key*4 | pp
	
	## read
	.byte 1 # r/w
	.byte 1 # r/w
	.byte 1 # r/w
	.byte 1 # r
	.byte 0 # -
	.byte 1 # r
	.byte 1 # r/w
	.byte 1 # r
	
	## write
	.byte 1 # r/w
	.byte 1 # r/w
	.byte 1 # r/w
	.byte 0 # r
	.byte 0 # -
	.byte 0 # r
	.byte 1 # r/w
	.byte 0 # r

damageframebuffer:
	push	%rax
	push	%rbx
	push	%rcx
	push	%rdx
	push	%rsi
	push	%rdi
	push	%r8
	mov	%edi, %ebx
	sub	%edi, IO_GCARD_FRAMEBUFFER_PA_START
	call	EXTERN(damageFrameBufferExt)
	mov	%edi, %ebx
	sub	%edi, IO_GCARD_FRAMEBUFFER_PA_START-4095
	call	EXTERN(damageFrameBufferExt)
	pop	%r8
	pop	%rdi
	pop	%rsi
	pop	%rdx
	pop	%rcx
	pop	%rbx
	pop	%rax
	ret
	
###############################################################################
##		bat_lookup
#define bat_lookup(di, n, rw, datacode)                                        \
	mov	%edx, [curCPU(di##bat_bl + n*4)];                              \
	mov	%ebx, [curCPU(di##bat_bepi + n*4)];                            \
	                                                                       \
	and	%edx, %eax;                                                    \
	cmp	%ebx, %edx;                                                    \
	jne	3f;                                                            \
	                                                                       \
	bt	dword ptr [curCPU(msr)], 14;                                   \
	mov	%r12d, 2;                                                      \
	sbb	%r12d, 0;	        /* BATU_Vp  / BATU_Vs */               \
	                                                                       \
	test	[curCPU(di##batu + n*4)], %r12d;                               \
	jz	3f;                                                            \
	                                                                       \
	/* FIXME: check access rights */                                       \
	mov	%esi, %eax;                                                    \
	mov	%ecx, %eax;                                                    \
	mov	%edx, 0xfffff000;                                              \
	and     %eax, [curCPU(di##bat_nbl + n*4)];                             \
	or      %eax, [curCPU(di##bat_brpn + n*4)];                            \
                                                                               \
/** TLB-Code */                                                                \
	shr	%ecx, 12;                                                      \
	and	%esi, %edx;                                                    \
	and	%edx, %eax;                                                    \
	and	%ecx, TLB_ENTRIES-1;                                           \
.if datacode==data;                                                            \
	cmp	%edx, [EXTERN(gMemorySize)];                                   \
	ja	4f;                                                            \
	add	%rdx, [EXTERN(gMemory)];                                       \
	add	%rax, [EXTERN(gMemory)];                                       \
6:;                                                                            \
.endif;                                                                        \
/*	movsxd  %rsi, %esi;*/                                                  \
/*	sub	%rdx, %rsi;*/                                                  \
	mov	[curCPU(tlb_##datacode##_##rw##_eff) + %rcx*4], %esi;          \
	mov	[curCPU(tlb_##datacode##_##rw##_phys) + %rcx*8], %rdx;         \
/*	add	qword ptr [curCPU(tlb_##datatcode##_##rw##_misses)], 1;   */   \
	clc;                                                                   \
.if datacode==data;                                                            \
	ret	8;                                                             \
4:                                                                             \
	cmp	%edx, IO_GCARD_FRAMEBUFFER_PA_START;                           \
	jb	5f;                                                            \
	cmp	%edx, IO_GCARD_FRAMEBUFFER_PA_END;                             \
	ja	5f;                                                            \
	mov	%ebx, %edx;                                                    \
	call	damageframebuffer;                                             \
	sub	%edx, IO_GCARD_FRAMEBUFFER_PA_START;                           \
	sub	%eax, IO_GCARD_FRAMEBUFFER_PA_START;                           \
	add	%rdx, [EXTERN(gFrameBuffer)];                                  \
	add	%rax, [EXTERN(gFrameBuffer)];                                  \
	jmp	6b;                                                            \
5:                                                                             \
	stc;                                                                   \
.endif;                                                                        \
	ret	8;                                                             \
3:

##############################################################################################
##	pg_table_lookup
##
##	param1: PTE1_H or 0
##	param2: 0 for read, 8 for write
##	param3: data / code
##	param4: pt offset
##	r9d     effective address
#define pg_table_lookup(n, rw, datacode, offset)                               \
	mov	%rax, [EXTERN(gMemory)];                                       \
	mov	%eax, [%rax + %rbx + offset];                                  \
	bswap	%eax;                                                          \
	                                                                       \
	/* %ecx = pte1 */                                                      \
	                                                                       \
	mov	%ecx, %eax;                                                    \
	and	%eax, (1<<6) | (1<<31);  	/* (PTE1_V | PTE1_H) */        \
	cmp	%eax, (n) | (1<<31);                                           \
	jne	1f;                                                            \
	                                                                       \
	mov	%eax, %ecx;                                                    \
	shr	%ecx, 7;                                                       \
	and	%ecx, 0xffffff; /* VSID */                                     \
	cmp	%ecx, %r12d;                                                   \
	jne	1f;                                                            \
	                                                                       \
	and	%eax, 0x3f;     /* API */                                      \
	cmp	%eax, %r8d;                                                    \
	jne	1f;                                                            \
	                                                                       \
	/* page found */                                                       \
	                                                                       \
	mov	%rsi, [EXTERN(gMemory)];                                       \
	mov	%esi, [%rsi + %rbx + 4 + offset];                              \
	bswap	%esi;                                                          \
	                                                                       \
	/* # %esi = pte2; */                                                   \
	                                                                       \
	/* FIXME: use bt trick? */                                             \
	test	dword ptr [curCPU(msr)], (1<<14); /* MSR_PR */                   \
	mov	%eax, (1<<29);	/* SR_Kp */                                    \
	setz	%cl;                                                           \
	shl	%eax, %cl;	/* SR_Kp <--> SR_Ks */                         \
	test	%edx, %eax;	/* SR_Kp / SR_Ks */                            \
	setnz	%al;                                                           \
	movzx	%eax, %al;                                                     \
                                                                               \
	mov	%ecx, %esi;                                                    \
	and	%ecx, 3;                                                       \
	                                                                       \
	cmp	byte ptr [ppc_pte_protection + (rw) + 4*%rax + %rcx], 1;       \
.if n==0;                                                                      \
/*	add	%rsp, 8;	/* hash1, no longer needed */                  \
	pop	%rdx;                                                          \
.else;                                                                        \
.endif;                                                                        \
	mov	%eax, %r9d; /* the effective address */                    \
	jne	protection_fault_##rw##_##datacode;                            \
	                                                                       \
	/* 	update R and C bits */                                         \
	mov	%edx, %esi;                                                    \
.if rw==0;                                                                     \
	or	%edx, (1<<8);		/* PTE2_R */                           \
.else;                                                                         \
	or	%edx, (1<<8) | (1<<7);	/* PTE2_R | PTE2_C */                  \
.endif;                                                                        \
	bswap	%edx;                                                          \
	add	%rbx, [EXTERN(gMemory)];                                       \
	mov	[%rbx+4+offset], %edx;                                         \
	                                                                       \
	and	%esi, 0xfffff000;                                              \
/** TLB-Code */                                                                \
	mov	%edx, %eax;                                                    \
	mov	%ecx, %eax;                                                    \
	shr	%edx, 12;                                                      \
	and	%ecx, 0xfffff000;                                              \
	and	%edx, TLB_ENTRIES-1;                                           \
	mov	%ebx, %esi;                                                    \
.if datacode==data;                                                            \
	cmp	%ebx, [EXTERN(gMemorySize)];                                   \
	ja	4f;                                                            \
	add	%rsi, [EXTERN(gMemory)];                                       \
6:;                                                                            \
.endif;                                                                        \
	mov	[curCPU(tlb_## datacode ##_## rw ##_eff) + %rdx*4], %ecx;      \
	mov	[curCPU(tlb_## datacode ##_## rw ##_phys) + %rdx*8], %rsi;     \
/*	add	qword ptr [curCPU(tlb_## datacode ##_## rw ##_misses)], 1;*/   \
/***/                                                                          \
	and	%eax, 0xfff;                                                   \
	add	%rax, %rsi;                                                    \
	clc;                                                                   \
	ret	8;                                                             \
.if datacode==data;                                                            \
4:                                                                             \
	cmp	%ebx, IO_GCARD_FRAMEBUFFER_PA_START;                           \
	jb	5f;                                                            \
	cmp	%ebx, IO_GCARD_FRAMEBUFFER_PA_END;                             \
	ja	5f;                                                            \
	call	damageframebuffer;                                             \
	sub	%esi, IO_GCARD_FRAMEBUFFER_PA_START;                           \
	add	%rsi, [EXTERN(gFrameBuffer)];                                  \
	jmp	6b;                                                            \
5:	and	%eax, 0xfff;                                                    \
	add	%eax, %esi;                                                    \
	stc;                                                                   \
	ret	8;                                                             \
.endif;                                                                        \
1:

##############################################################################################
##	protection_fault_%2_%3
protection_fault_0_code:
	# ppc_exception(PPC_EXC_ISI, PPC_EXC_SRR1_PROT, addr);
	pop	%rdx		# return address is no longer needed
	pop	%rbx		# bytes to roll back
	add	%rsp, %rbx
	mov	%ecx, (1<<27)	# PPC_EXC_SRR1_PROT
	jmp	EXTERN(ppc_isi_exception_asm)
protection_fault_0_data:
	# ppc_exception(PPC_EXC_DSI, PPC_EXC_DSISR_PROT, addr);
	pop	%rdx		# return address is no longer needed
	pop	%rbx		# bytes to roll back
	add	%rsp, %rbx
	mov	%ecx, (1<<27)	# PPC_EXC_DSISR_PROT
	jmp	EXTERN(ppc_dsi_exception_asm)
protection_fault_8_data:
	# ppc_exception(PPC_EXC_DSI, PPC_EXC_DSISR_PROT | PPC_EXC_DSISR_STORE, addr);
	pop	%rdx			# return address is no longer needed
	pop	%rbx			# bytes to roll back
	add	%rsp, %rbx
	mov	%ecx, (1<<27) | (1<<25)	# PPC_EXC_DSISR_PROT | PPC_EXC_DSISR_STORE
	jmp	EXTERN(ppc_dsi_exception_asm)

##############################################################################################
##	param1: 0 for read, 8 for write
##	param2: data / code
#define tlb_lookup(rw, datacode)                                               \
	mov	%edx, %eax;                                                    \
	mov	%ecx, %eax;                                                    \
	shr	%edx, 12;                                                      \
	and	%ecx, 0xfffff000;                                              \
	and	%edx, TLB_ENTRIES-1;                                           \
	cmp	%ecx, [curCPU(tlb_##datacode##_##rw##_eff) + %rdx*4];          \
	jne	1f;                                                            \
	/*                                                                     \
	 *	if a tlb entry is invalid, its                                 \
	 *	lower 12 bits are 1, so the cmp is guaranteed to fail.         \
	 */                                                                    \
/*	add	qword ptr [curCPU(tlb_##datacode##_##rw##_hits)], 1;   */      \
	and	%eax, 0xfff;                                                   \
	add	%rax, [curCPU(tlb_##datacode##_##rw##_phys) + %rdx*8];         \
	clc;                                                                   \
	ret	8;                                                             \
1:                                                                             \

.balign 16
ppc_effective_to_physical_code_ret:
	mov	%edx, %eax
	mov	%ecx, %eax
	shr	%edx, 12
	and	%ecx, 0xfffff000
	and	%edx, TLB_ENTRIES-1
	mov	[curCPU(tlb_code_0_eff) + %rdx*4], %ecx
	mov	[curCPU(tlb_code_0_phys) + %rdx*8], %rcx
	ret	8

.balign 16
##############################################################################################
##	uint32 FASTCALL ppc_effective_to_physical_code(uint32 addr)
## 
##	IN	%eax: address to translate
## 
##	WILL NOT RETURN ON EXCEPTION!
##
EXPORT(ppc_effective_to_physical_code):
	tlb_lookup(0, code)

	test	byte ptr [curCPU(msr)], (1<<5)	# MSR_IR
	jz	ppc_effective_to_physical_code_ret

	bat_lookup(i, 0, 0, code)
	bat_lookup(i, 1, 0, code)
	bat_lookup(i, 2, 0, code)
	bat_lookup(i, 3, 0, code)

	mov	%ebx, %eax
	shr	%ebx, 28			# SR
	mov	%edx, [curCPU(sr+4*%rbx)]

	# test	%edx, SR_T --> die
	
	test	%edx, (1<<28)			# SR_N
	jnz	9f
	
	mov	%ebx, %eax
	mov 	%r12d, %edx
	shr	%ebx, 12
	mov	%r8d, %eax
	and	%ebx, 0xffff
	shr	%r8d, 22
	and	%r12d, 0xffffff
	and	%r8d, 0x3f
	
	# now:
	# %eax = addr
	# %ebx = page_index
	# %r12d = VSID
	# %r8d = api
	
	xor	%ebx, %r12d
	
	# %ebx = hash1
	
	mov	%r9d, %eax
	push	%rbx			# das brauch ich
	
	and	%ebx, [curCPU(pagetable_hashmask)]
	shl	%ebx, 6
	or	%ebx, [curCPU(pagetable_base)]
	
	# %ebx = pteg_addr
	
	pg_table_lookup(0, 0, code, 0)
	pg_table_lookup(0, 0, code, 8)
	pg_table_lookup(0, 0, code, 16)
	pg_table_lookup(0, 0, code, 24)
	pg_table_lookup(0, 0, code, 32)
	pg_table_lookup(0, 0, code, 40)
	pg_table_lookup(0, 0, code, 48)
	pg_table_lookup(0, 0, code, 56)
	
	# hash function number 2
	pop	%rbx
	not	%ebx
	and	%ebx, [curCPU(pagetable_hashmask)]
	shl	%ebx, 6
	or	%ebx, [curCPU(pagetable_base)]

	pg_table_lookup(1<<6, 0, code, 0)
	pg_table_lookup(1<<6, 0, code, 8)
	pg_table_lookup(1<<6, 0, code, 16)
	pg_table_lookup(1<<6, 0, code, 24)
	pg_table_lookup(1<<6, 0, code, 32)
	pg_table_lookup(1<<6, 0, code, 40)
	pg_table_lookup(1<<6, 0, code, 48)
	pg_table_lookup(1<<6, 0, code, 56)

	# page lookup failed --> throw exception
	
	mov	%eax, %r9d
	pop	%rdx			# return address is no longer needed
	pop	%rcx			# bytes to roll back
	add	%rsp, %rcx

	mov	%ecx, (1<<30)		# PPC_EXC_SRR1_PAGE
	jmp	EXTERN(ppc_isi_exception_asm)
9:
	# segment isnt executable --> throw exception
	pop	%rdx			# return address is no longer needed
	pop	%rcx			# bytes to roll back
	add	%rsp, %rcx

	mov	%ecx, (1<<28)		# PPC_EXC_SRR1_GUARD
	jmp	EXTERN(ppc_isi_exception_asm)

.balign 16
ppc_effective_to_physical_data_read_ret:
	mov	%edx, %eax
	mov	%ecx, %eax
	shr	%edx, 12
	and	%ecx, 0xfffff000
	and	%edx, TLB_ENTRIES-1
	cmp	%ecx, [gMemorySize]
	ja	4f
	add	%rax, [gMemory]
	mov	[curCPU(tlb_data_0_eff) + %rdx*4], %ecx
	add	%rcx, [gMemory]
	mov	[curCPU(tlb_data_0_phys) + %rdx*8], %rcx
	clc
	ret	8
4:	stc
	ret	8
	
.balign 16
##############################################################################################
##	uint32 FASTCALL ppc_effective_to_physical_data_read(uint32 addr)
## 
##	IN	%eax: address to translate
## 
##	WILL NOT RETURN ON EXCEPTION!
##
ppc_effective_to_physical_data_read:
	tlb_lookup(0, data)

	test	byte ptr [curCPU(msr)], (1<<4)	# MSR_DR
	jz	ppc_effective_to_physical_data_read_ret
	                        
	bat_lookup(d, 0, 0, data)
	bat_lookup(d, 1, 0, data)
	bat_lookup(d, 2, 0, data)
	bat_lookup(d, 3, 0, data)

	mov	%ebx, %eax
	shr	%ebx, 28			# SR
	mov	%edx, [curCPU(sr+4*%rbx)]
	
	# test %edx, SR_T --> die
	
	mov	%ebx, %eax
	mov	%r12d, %edx
	shr	%ebx, 12
	mov	%r8d, %eax
	and	%ebx, 0xffff
	shr	%r8d, 22
	and	%r12d, 0xffffff
	and	%r8d, 0x3f
	
	# now:
	# %eax = addr
	# %ebx = page_index
	# %r12d = VSID
	# %r8d = api
	
	xor	%ebx, %r12d
	
	# %ebx = hash1
	
	mov	%r9d, %eax
	push	%rbx			# das brauch ich
	
	and	%ebx, [curCPU(pagetable_hashmask)]
	shl	%ebx, 6
	or	%ebx, [curCPU(pagetable_base)]
	
	# %ebx = pteg_addr
	
	pg_table_lookup(0, 0, data, 0)
	pg_table_lookup(0, 0, data, 8)
	pg_table_lookup(0, 0, data, 16)
	pg_table_lookup(0, 0, data, 24)
	pg_table_lookup(0, 0, data, 32)
	pg_table_lookup(0, 0, data, 40)
	pg_table_lookup(0, 0, data, 48)
	pg_table_lookup(0, 0, data, 56)
	
	# hash function number 2
	pop	%rbx
	not	%ebx
	and	%ebx, [curCPU(pagetable_hashmask)]
	shl	%ebx, 6
	or	%ebx, [curCPU(pagetable_base)]

	pg_table_lookup(1<<6, 0, data, 0)
	pg_table_lookup(1<<6, 0, data, 8)
	pg_table_lookup(1<<6, 0, data, 16)
	pg_table_lookup(1<<6, 0, data, 24)
	pg_table_lookup(1<<6, 0, data, 32)
	pg_table_lookup(1<<6, 0, data, 40)
	pg_table_lookup(1<<6, 0, data, 48)
	pg_table_lookup(1<<6, 0, data, 56)

	# page lookup failed --> throw exception
	
	mov	%eax, %r9d
	pop	%rdx			# return address is no longer needed
	pop	%rcx			# bytes to roll back
	add	%rsp, %rcx
	
	mov	%ecx, (1<<30)		# PPC_EXC_DSISR_PAGE
	jmp	EXTERN(ppc_dsi_exception_asm)

.balign 16
ppc_effective_to_physical_data_write_ret:
	mov	%edx, %eax
	mov	%ecx, %eax
	shr	%edx, 12
	and	%ecx, 0xfffff000
	and	%edx, TLB_ENTRIES-1
	cmp	%ecx, [gMemorySize]
	ja	4f
	add	%rax, [gMemory]
	mov	[curCPU(tlb_data_8_eff) + %rdx*4], %ecx
	add	%rcx, [gMemory]
	mov	[curCPU(tlb_data_8_phys) + %rdx*8], %rcx
	clc
	ret	8
4:	stc
	ret	8
	

.balign 16
##############################################################################################
##	uint32 FASTCALL ppc_effective_to_physical_data_write(uint32 addr)
##
##	IN	%eax: address to translate
##
##	WILL NOT RETURN ON EXCEPTION!
##
ppc_effective_to_physical_data_write:
	tlb_lookup(8, data)

	test	byte ptr [curCPU(msr)], (1<<4)	# MSR_DR
	jz	ppc_effective_to_physical_data_write_ret
	
	bat_lookup(d, 0, 8, data)
	bat_lookup(d, 1, 8, data)
	bat_lookup(d, 2, 8, data)
	bat_lookup(d, 3, 8, data)

	mov	%ebx, %eax
	shr	%eax, 28			# SR
	mov	%edx, [curCPU(sr+4*%rax)]
	
	# test %edx, SR_T --> die
	
	mov	%eax, %ebx
	mov	%r8d, %ebx
	mov	%r12d, %edx
	shr	%ebx, 12
	and	%ebx, 0xffff
	shr	%r8d, 22
	and	%r12d, 0xffffff
	and	%r8d, 0x3f
	
	# now:
	# %eax = addr
	# %ebx = page_index
	# %r12d = VSID
	# %r8d = api
	
	xor	%ebx, %r12d
	
	# %ebx = hash1
	
	mov	%r9d, %eax
	push	%rbx			# das brauch ich
	
	and	%ebx, [curCPU(pagetable_hashmask)]
	shl	%ebx, 6
	or	%ebx, [curCPU(pagetable_base)]
	
	# %ebx = pteg_addr
	
	pg_table_lookup(0, 8, data, 0)
	pg_table_lookup(0, 8, data, 8)
	pg_table_lookup(0, 8, data, 16)
	pg_table_lookup(0, 8, data, 24)
	pg_table_lookup(0, 8, data, 32)
	pg_table_lookup(0, 8, data, 40)
	pg_table_lookup(0, 8, data, 48)
	pg_table_lookup(0, 8, data, 56)
	
	# hash function number 2
	mov	%ebx, [%rsp]
	add	%rsp, 8
	not	%ebx
	and	%ebx, [curCPU(pagetable_hashmask)]
	shl	%ebx, 6
	or	%ebx, [curCPU(pagetable_base)]

	pg_table_lookup(1<<6, 8, data, 0)
	pg_table_lookup(1<<6, 8, data, 8)
	pg_table_lookup(1<<6, 8, data, 16) 
	pg_table_lookup(1<<6, 8, data, 24)
	pg_table_lookup(1<<6, 8, data, 32)
	pg_table_lookup(1<<6, 8, data, 40)
	pg_table_lookup(1<<6, 8, data, 48)
	pg_table_lookup(1<<6, 8, data, 56)

	# page lookup failed --> throw exception
	
	mov	%eax, %r9d
	pop	%rdx			# return address is no longer needed
	pop	%rbx			# bytes to roll back
	add	%rsp, %rbx

	mov	%ecx, (1<<30)|(1<<25)	# PPC_EXC_DSISR_PAGE | PPC_EXC_DSISR_STORE
	jmp	EXTERN(ppc_dsi_exception_asm)

.balign 16
##############################################################################################
##	uint32 FASTCALL ppc_write_effective_byte()
##
##	IN	%eax: address to translate
##		%esi: current client pc offset
##		%dl: byte to be written
##
##	WILL NOT RETURN ON EXCEPTION!
##
EXPORT(ppc_write_effective_byte_asm):
	mmu_prologue

	push	%rdx
	push	16			# roll back 16 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	mov	%edx, [%rsp]
	lea	%rsp, [%rsp+8]
	jc	1f
	mov	[%rax], %dl
	ret
1:
	mov	%edi, %eax
	movzx	%esi, %dl
	mov	%edx, 1
	jmp	EXTERN(io_mem_write_glue)

.balign 16
##############################################################################################
##	uint32 FASTCALL ppc_effective_write_half()
##
##	IN	%eax: address to translate
##		 %dx: half to be written
##		%esi: current client pc offset
##
##	WILL NOT RETURN ON EXCEPTION!
##
EXPORT(ppc_write_effective_half_asm):
	mmu_prologue
	mov	%ebx, %eax
	and	%ebx, 0xfff
	cmp	%ebx, 4095
	jae	1f

	push	%rdx
	push	16			# roll back 16 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	mov	%edx, [%rsp]
	lea	%rsp, [%rsp+8]
	jc	2f
	mov	[%rax], %dh
	mov	[%rax+1], %dl
	ret
2:
	rol	%dx, 8
	mov	%edi, %eax
	movzx	%esi, %dx
	mov	%edx, 2
	jmp	EXTERN(io_mem_write_glue)

1:
	push	%rdx
	push	%rax
	push	24			# roll back 24 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	mov	%edx, [%rsp+8]
	jc	1f
	mov	[%rax], %dh
3:
	pop	%rax
	inc	%eax
	push	16			# roll back 16 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	pop	%rdx
	jc	2f
	mov	[%rax], %dl
	ret

1:
	movzx	%esi, %dh
	mov	%edi, %eax
	mov	%edx, 1
	call	EXTERN(io_mem_write_glue)
	getCurCPU 1
	jmp	3b
2:
	movzx	%esi, %dl
	mov	%edi, %eax
	mov	%edx, 1
	jmp	EXTERN(io_mem_write_glue)

.balign 16
##############################################################################################
##	uint32 FASTCALL ppc_effective_write_word()
##
##	IN	%eax: address to translate
##		%edx: word to be written
##		%esi: current client pc offset
##
##	WILL NOT RETURN ON EXCEPTION!
##
EXPORT(ppc_write_effective_word_asm):
	mmu_prologue
	mov	%ebx, %eax
	and	%ebx, 0xfff
	cmp	%ebx, 4093
	jae	1f

	bswap	%edx

	push	%rdx
	push	16			# roll back 16 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	mov	%edx, [%rsp]
	lea	%rsp, [%rsp+8]
	jc	2f
	mov	[%rax], %edx
	ret

2:
	mov	%edi, %eax
	mov	%esi, %edx
	mov	%edx, 4
	jmp	EXTERN(io_mem_write_glue)

1:
	push	%rdx
	push	%rax
	push	24			# roll back 24 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	mov	%edx, [%rsp+8]
	mov	%ebx, [%rsp]
	mov	%ecx, 4096
	jc	2f
	and	%ebx, 0xfff
	sub	%ecx, %ebx
	1:
		rol	%edx, 8
		mov	[%rax], %dl
		inc	%rax
		dec	%ecx
	jnz	1b
	4:
	mov	[%rsp+8], %edx
	mov	%eax, [%rsp]
	add	%eax, 4
	push	24			# roll back 24 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	pop	%rbx
	pop	%rdx
	jc	3f
	add	%ebx, 4
	and	%ebx, 0xfff
	sub	%rax, %rbx
	1:
		rol	%edx, 8
		mov	[%rax], %dl
		inc	%rax
		dec	%ebx
	jnz	1b
	ret

2:
	and	%ebx, 0xfff
	sub	%ecx, %ebx
	1:
		rol	%edx, 8
		push	%rax
		push	%rcx
		push	%rdx
		movzx	%esi, %dl
		mov	%edi, %eax
		mov	%edx, 1
		call	EXTERN(io_mem_write_glue)
		pop	%rdx
		pop	%rcx
		pop	%rax
		inc	%eax
		dec	%ecx
	jnz	1b
	jmp	4b
3:
	add	%ebx, 4
	and	%ebx, 0xfff
	sub	%eax, %ebx
	1:
		rol	%edx, 8
		push	%rax
		push	%rdx
		mov	%edi, %eax
		movzx	%esi, %dl
		mov	%edx, 1
		call	EXTERN(io_mem_write_glue)
		pop	%rdx
		pop	%rax
		inc	%rax
		dec	%rbx
	jnz	1b
	ret
.balign 16
##############################################################################################
##	uint32 FASTCALL ppc_effective_write_dword()
##
##	IN	%eax: address to translate
##		%rdx dword to be written
##		%esi: current client pc offset
##
##	WILL NOT RETURN ON EXCEPTION!
##
EXPORT(ppc_write_effective_dword_asm):
	mmu_prologue
	mov	%ebx, %eax
	and	%ebx, 0xfff
	cmp	%ebx, 4089
	jae	1f			# overlap

	bswap	%rdx

	push	%rdx
	push	16			# roll back 16 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	mov	%rdx, [%rsp]
	lea	%rsp, [%rsp+8]
	jc	2f
	mov	[%rax], %rdx
	ret
2:
	mov	%edi, %eax
	mov	%rsi, %rdx
	jmp	EXTERN(io_mem_write64_glue)

1:
	push	%rdx
	push	%rax
	push	24			# roll back 24 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	mov	%rdx, [%rsp+8]
	mov	%ebx, [%rsp]
	mov	%ecx, 4096
	jc	2f
	and	%ebx, 0xfff
	sub	%ecx, %ebx
	1:
		rol	%rdx, 8
		mov	[%rax], %dl
		inc	%rax
		dec	%ecx
	jnz	1b
	4:
	mov	[%rsp+8], %rdx
	mov	%eax, [%rsp]
	add	%eax, 8
	push	24			# roll back 16 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	pop	%rbx
	pop	%rdx
	jc	3f
	add	%ebx, 8
	and	%ebx, 0xfff
	sub	%rax, %rbx
	1:
		rol	%rdx, 8
		mov	[%rax], %dl
		inc	%rax
		dec	%ebx
	jnz	1b
	ret

2:
	and	%ebx, 0xfff
	sub	%ecx, %ebx
	1:
		rol	%rdx, 8
		push	%rax
		push	%rcx
		push	%rdx
		movzx	%esi, %dl
		mov	%edi, %eax
		mov	%edx, 1
		call	EXTERN(io_mem_write_glue)
		pop	%rdx
		pop	%rcx
		pop	%rax
		inc	%eax
		dec	%ecx
	jnz	1b
	jmp	4b
3:
	add	%ebx, 4
	and	%ebx, 0xfff
	sub	%eax, %ebx
	1:
		rol	%rdx, 8
		push	%rax
		push	%rdx
		mov	%edi, %eax
		movzx	%esi, %dl
		mov	%edx, 1
		call	EXTERN(io_mem_write_glue)
		pop	%rdx
		pop	%rax
		inc	%rax
		dec	%rbx
	jnz	1b
	ret

.balign 16
##############################################################################################
##	uint32 FASTCALL ppc_effective_write_qword()
##
##	IN	%eax: address to translate
##		%rdx: address from which to get the values to be written
##		%esi: current client pc offset
##
##	WILL NOT RETURN ON EXCEPTION!
##
EXPORT(ppc_write_effective_qword_asm):
	mmu_prologue
	and	%eax, 0xfffffff0
	push	%rdx

	push	16			# roll back 16 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	pop	%rdx
	jc	1f

	mov	%rbx, [%rdx]
	mov	%rsi, [%rdx+8]

	bswap	%rbx
	bswap	%rsi

	mov	[%rax+8], %rbx
	mov	[%rax], %rsi
	ret
1:
	mov	%edi, %eax
	mov	%rsi, %rdx
	jmp	EXTERN(io_mem_write128_glue)

ppc_write_effective_qword_sse_asm:
	mmu_prologue
	and	%eax, 0xfffffff0
	push	%rdx

	push	16			# roll back 16 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	pop	%rdx
	jc	1f

	movaps	[%rax], %xmm0
	ret
1:
	movaps	[%rdx], %xmm0
	mov	%edi, %eax
	mov	%rsi, %rdx
	jmp	EXTERN(io_mem_write128_native_glue)

.balign 16
##############################################################################################
##	uint32 FASTCALL ppc_read_effective_byte()
##
##	IN	%eax: address to translate
##		%esi: current client pc offset
##
##	OUT	%edx: byte, zero extended
##
##	WILL NOT RETURN ON EXCEPTION!
##
EXPORT(ppc_read_effective_byte_asm):
	mmu_prologue

	push	8			# roll back 8 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	jc	1f
	movzx	%edx, byte ptr [%rax]
	ret
1:
	mov	%edi, %eax
	mov	%esi, 1
	call	EXTERN(io_mem_read_glue)
	movzx	%edx, %al
	ret

.balign 16
##############################################################################################
##	uint32 FASTCALL ppc_read_effective_half()
##
##	IN	%eax: address to translate
##		%esi: current client pc offset
##
##	OUT	%edx: half, zero extended
##
##	WILL NOT RETURN ON EXCEPTION!
##
EXPORT(ppc_read_effective_half_z_asm):
	mmu_prologue
	mov	%ebx, %eax
	and	%ebx, 0xfff
	cmp	%ebx, 4095
	jae	1f

	push	8			# roll back 8 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	jc	2f
	movzx	%edx, word ptr [%rax]
	rol	%dx, 8
	ret
2:
	mov	%edi, %eax
	mov	%esi, 2
	call	EXTERN(io_mem_read_glue)
	rol	%ax, 8
	movzx	%edx, %ax
	ret

1:
	push	%rax
	push	16			# roll back 16 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	jc	2f
	xor	%edx, %edx
	mov	%dh, [%rax]
1:
	mov	%eax, [%rsp]
	mov	[%rsp], %edx
	inc	%eax
	push	16			# roll back 16 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	pop	%rdx
	jc	3f
	mov	%dl, [%rax]
	ret

2:
	push	%rdi
	mov	%edi, %eax
	mov	%esi, 1
	call	EXTERN(io_mem_read_glue)
	xor	%edx, %edx
	mov	%dh, %al
	pop	%rdi
	jmp	1b
3:
	push	%rdx
	mov	%edi, %eax
	mov	%esi, 1
	call	EXTERN(io_mem_read_glue)
	pop	%rdx
	mov	%dl, %al
	ret
	
.balign 16
##############################################################################################
##	uint32 FASTCALL ppc_read_effective_half()
##
##	IN	%eax: address to translate
##		%esi: current client pc offset
##
##	OUT	%edx: half, sign extended
##
##	WILL NOT RETURN ON EXCEPTION!
##
EXPORT(ppc_read_effective_half_s_asm):
	mmu_prologue
	mov	%ebx, %eax
	and	%ebx, 0xfff
	cmp	%ebx, 4095
	jae	1f

	push	8			# roll back 8 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	jc	2f
	mov	%cx, [%rax]
	rol	%cx, 8
	movsx	%edx, %cx
	ret
2:
	mov	%edi, %eax
	mov	%esi, 2
	call	EXTERN(io_mem_read_glue)
	rol	%ax, 8
	movsx	%edx, %ax
	ret

1:
	push	%rax
	push	16			# roll back 16 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	jc	2f
	xor	%ecx, %ecx
	mov	%ch, [%rax]
1:
	mov	%eax, [%rsp]
	mov	[%rsp], %ecx
	inc	%eax
	push	16			# roll back 16 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	pop	%rcx
	jc	3f
	mov	%cl, [%rax]
	movsx	%edx, %cx
	ret

2:
	push	%rdi
	mov	%edi, %eax
	mov	%esi, 1
	call	EXTERN(io_mem_read_glue)
	xor	%ecx, %ecx
	mov	%ch, %al
	pop	%rdi
	jmp	1b
3:
	push	%rcx
	mov	%edi, %eax
	mov	%esi, 1
	call	EXTERN(io_mem_read_glue)
	pop	%rcx
	mov	%cl, %al
	movsx	%edx, %cx
	ret
	
.balign 16
##############################################################################################
##	uint32 FASTCALL ppc_read_effective_word()
##
##	IN	%eax: address to translate
##		%esi: current client pc offset
##
##	OUT	%edx: word
##
##	WILL NOT RETURN ON EXCEPTION!
##
EXPORT(ppc_read_effective_word_asm):
	mmu_prologue
	mov	%ebx, %eax
	and	%ebx, 0xfff
	cmp	%ebx, 4093
	jae	1f

	push	8			# roll back 8 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	jc	2f
	mov	%edx, [%rax]
	bswap	%edx
	ret
2:
	mov	%edi, %eax
	mov	%esi, 4
	call	EXTERN(io_mem_read_glue)
	mov	%edx, %eax
	bswap	%edx
	ret
1:
	push	%rax
	push	%rbx
	push	24			# roll back 24 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	pop	%rbx
	mov	%ecx, 4096
	jc	2f
	sub	%ecx, %ebx
	1:
		shl	%edx, 8
		mov	%dl, [%rax]
		inc	%rax
		dec	%ecx
	jnz	1b
	4:
	mov	%eax, [%rsp]
	mov	[%rsp], %edx
	add	%eax, 4
	push	%rbx
	and	%eax, 0xfffff000
	push	24			# roll back 24 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	pop	%rbx
	pop	%rdx
	jc	3f
	sub	%ebx, 4092
	1:
		shl	%edx, 8
		mov	%dl, [%rax]
		inc	%rax
		dec	%ebx
	jnz	1b
	ret

2:
	sub	%ecx, %ebx
	push	%rdi
	1:
		shl	%edx, 8
		push	%rcx
		push	%rax
		push	%rdx
		mov	%edi, %eax
		mov	%esi, 1
		call	EXTERN(io_mem_read_glue)
		pop	%rdx
		mov	%dl, %al
		pop	%rax
		pop	%rcx
		inc	%eax
		dec	%ecx
	jnz	1b
	pop	%rdi
	jmp	4b
3:
	sub	%ebx, 4092
	1:
		shl	%edx, 8
		push	%rax
		push	%rdx
		mov	%edi, %eax
		mov	%esi, 1
		call	EXTERN(io_mem_read_glue)
		pop	%rdx
		mov	%dl, %al
		pop	%rax
		inc	%eax
		dec	%ebx
	jnz	1b
	ret
.balign 16
##############################################################################################
##	uint32 FASTCALL ppc_read_effective_dword()
##
##	IN	%eax: address to translate
##		%esi: current client pc offset
##
##	OUT	%rdx dword
##
##	WILL NOT RETURN ON EXCEPTION!
##
EXPORT(ppc_read_effective_dword_asm):
	mmu_prologue
	mov	%ebx, %eax
	and	%ebx, 0xfff
	cmp	%ebx, 4089
	jae	1f

	push	8			# roll back 8 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	jc	2f
	mov	%rdx, [%rax]
	bswap	%rdx
	ret
2:
	mov	%edi, %eax
	call	EXTERN(io_mem_read64_glue)
	mov	%rdx, %rax
	bswap	%rdx
	ret

1:
	push	%rax
	push	%rbx
	push	24			# roll back 24 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	pop	%rbx
	mov	%r12d, 4096
	jc	2f
	sub	%r12d, %ebx
	1:
		shl	%rdx, 8
		mov	%dl, [%rax]
		inc	%rax
		dec	%r12d
	jnz	1b
	4:
	pop	%rax
	push	%rdx
	add	%eax, 8
	push	%rbx
	and	%eax, 0xfffff000
	push	24			# roll back 24 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	pop	%rbx
	pop	%rdx
	jc	3f
	sub	%ebx, 4088
	1:
		shl	%rdx, 8
		mov	%dl, [%rax]
		inc	%rax
		dec	%ebx
	jnz	1b
	ret

2:
	sub	%r12d, %ebx
	push	%rdi
	1:
		shl	%rdx, 8
		push	%rax
		push	%rdx
		mov	%edi, %eax
		mov	%esi, 1
		call	EXTERN(io_mem_read_glue)
		pop	%rdx
		mov	%dl, %al
		pop	%rax
		inc	%eax
		dec	%r12d
	jnz	1b
	pop	%rdi
	jmp	4b
3:
	sub	%ebx, 4088
	1:
		shl	%rdx, 8
		push	%rax
		push	%rdx
		mov	%edi, %eax
		mov	%esi, 1
		call	EXTERN(io_mem_read_glue)
		pop	%rdx
		mov	%dl, %al
		pop	%rax
		inc	%eax
		dec	%ebx
	jnz	1b
	ret
	
.balign 16
##############################################################################################
##	uint32 FASTCALL ppc_read_effective_qword()
##
##	IN	%eax: address to translate
##		%rdx: address to store the qword
##		%esi: current client pc offset
##
##	WILL NOT RETURN ON EXCEPTION!
##
EXPORT(ppc_read_effective_qword_asm):
	mmu_prologue
	and	%eax, 0xfffffff0
	push	%rdx

	push	16			# roll back 16 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	pop	%rdx
	jc	1f

	mov	%rbx, [%rax]
	mov	%rsi, [%rax+8]

	bswap	%rbx
	bswap	%rsi

	mov	[%rdx+8], %rbx
	mov	[%rdx], %rsi
	ret
1:
	mov	%edi, %eax
	mov	%rsi, %rdx
	jmp	EXTERN(io_mem_read128_glue)

EXPORT(ppc_read_effective_qword_sse_asm):
	mmu_prologue
	and	%eax, 0xfffffff0
	push	%rdx

	push	16			# roll back 16 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	pop	%rdx
	jc	1f

	movaps	%xmm0, [%rax]

	mov	%rbx, [%rax]
	mov	%rsi, [%rax+8]

	bswap	%rbx
	bswap	%rsi

	mov	[%rdx+8], %rbx
	mov	[%rdx], %rsi
	ret

1:
	push	%rdx
	mov	%edi, %eax
	mov	%rsi, %rdx
	call	EXTERN(io_mem_read128_native_glue)
	pop	%rdx

	movaps	%xmm0, [%rdx]

	mov	%rax, [%rdx]
	mov	%rsi, [%rdx+8]

	bswap	%rax
	bswap	%rsi

	mov	[%rdx+8], %rsi
	mov	[%rdx], %rax
	ret

.balign 16
##############################################################################################
##	uint32 FASTCALL ppc_opc_stswi_asm()
##
##	IN	%ecx: NB
##		%ebx: source
##		%eax: dest
##		%esi: current client pc offset
##
##	WILL NOT RETURN ON EXCEPTION!
##
EXPORT(ppc_opc_stswi_asm):
	mmu_prologue
	mov	%r8d, 1
	
	1:
		dec	%r8d
		jnz	2f
			mov	%edx, [curCPU(gpr+4*%rbx)]
			inc	%ebx
			mov	%r8d, 4
			and	%ebx, 0x1f
		2:
		push	%rax
		push	%rcx
		push	%rbx
		push	%r8
		push	%rdi
		push	%rdx
		push	56			# roll back 56 bytes in case of exception
		call	ppc_effective_to_physical_data_write
		pop	%rdx
		mov	%ecx, %edx
		jc	3f
		shr	%ecx, 24
		mov	[%rax], %cl
	4:
		pop	%rdi
		pop	%r8
		pop	%rbx
		pop	%rcx
		pop	%rax
		shl	%edx, 8
		inc	%eax
		dec	%ecx
	jnz	1b
	ret

3:
	push	%rdx
	mov	%edi, %eax
	shr	%edx, 24
	mov	%esi, %edx
	mov	%edx, 1
	call	EXTERN(io_mem_write_glue)
	pop	%rdx
	jmp	4b
.balign 16
##############################################################################################
##	uint32 FASTCALL ppc_opc_lswi_asm()
##
##	IN	%ecx: NB
##		%ebx: dest
##		%eax: source
##		%esi: current client pc offset
##
##	WILL NOT RETURN ON EXCEPTION!
##
EXPORT(ppc_opc_lswi_asm):
	mmu_prologue
	mov	%r8d, 4
	1:
		or	%r8d, %r8d
		jnz	2f
			mov	[curCPU(gpr+4*%rbx)], %edx
			inc	%ebx
			mov	%r8d, 4
			and	%ebx, 0x1f
			xor	%edx, %edx
		2:

		push	%rax
		push	%rcx
		push	%rbx
		push	%rdi
		push	%r8
		push	%rdx
		push	56			# roll back 56 bytes in case of exception
		call	ppc_effective_to_physical_data_read
		pop	%rdx
		jc	3f
		shl	%edx, 8
		mov	%dl, byte ptr [%rax]
	4:
		pop	%r8
		pop	%rdi
		pop	%rbx
		pop	%rcx
		pop	%rax

		dec	%r8d
		inc	%eax
		dec	%ecx
	jnz	1b
	
	or	%r8d, %r8d
	jz	5f
		2:
			shl	%edx, 8
			dec	%r8d
		jnz	2b

	5:
	mov	[curCPU(gpr)+4*%rbx], %edx
	ret

3:
	push	%rdx
	mov	%edi, %eax
	mov	%esi, 1
	call	EXTERN(io_mem_read_glue)
	pop	%rdx
	shl	%edx, 8
	mov	%dl, %al
	jmp	4b

.balign 16
##############################################################################################
##	uint32 FASTCALL ppc_opc_icbi_asm()
##
##	IN	%eax: effective address
EXPORT(ppc_opc_icbi_asm):
	mmu_prologue
	push	8			# roll back 8 bytes in case of exception
	add	%eax, 1			# FIXME: WTF???
	call	ppc_effective_to_physical_data_read
	jc	1f
	sub	%rax, [gMemory]
	#	%rdi is set to cpu
	mov	%rdi, [curCPU(jitc)]
	mov	%rcx, [%rdi+clientPages]
	shr	%eax, 12
	cmp	qword ptr [%rcx+%rax*8], 0
	jnz	2f
1:
	rep;	ret
	
2:
	mov	%rsi, [%rcx+%rax*8]
	jmp	EXTERN(jitcDestroyAndFreeClientPage)

##############################################################################################
