/*
 *	PearPC
 *	jitc_mmu.S
 *
 *	Copyright (C) 2003-2006 Sebastian Biallas (sb@biallas.net)
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

#define TLB_ENTRIES 32

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
	MEMBER(tlb_code_0_eff, TLB_ENTRIES*4)
	MEMBER(tlb_data_0_eff, TLB_ENTRIES*4)
	MEMBER(tlb_data_8_eff, TLB_ENTRIES*4)
	MEMBER(tlb_code_0_phys, TLB_ENTRIES*4)
	MEMBER(tlb_data_0_phys, TLB_ENTRIES*4)
	MEMBER(tlb_data_8_phys, TLB_ENTRIES*4)
	MEMBER(blblbl, 4)
	MEMBER(tlb_code_0_hits, 8)
	MEMBER(tlb_data_0_hits, 8)
	MEMBER(tlb_data_8_hits, 8)
	MEMBER(tlb_code_0_misses, 8)
	MEMBER(tlb_data_0_misses, 8)
	MEMBER(tlb_data_8_misses, 8)

	MEMBER(nativeReg, 8*4)
	
	MEMBER(nativeRegState, 8*4)
	
	MEMBER(nativeFlags, 4)

	MEMBER(nativeFlagsState, 4)
	MEMBER(nativeCarryState, 4)
	
	MEMBER(clientReg, 600*4)
	
	MEMBER(nativeRegsList, 8*4)
		 
	MEMBER(LRUreg, 4)
	MEMBER(MRUreg, 4)

	MEMBER(LRUpage, 4)
	MEMBER(MRUpage, 4)

	MEMBER(freeFragmentsList, 4)

	MEMBER(freeClientPages, 4)
	
	MEMBER(translationCache, 4)

STRUCT	#ClientPage
	MEMBER(entrypoints, 1024*4)
	MEMBER(baseaddress, 4)
	MEMBER(tcf_current, 4)
	MEMBER(bytesLeft, 4)
	MEMBER(tcp, 4)
	MEMBER(moreRU, 4)
	MEMBER(lessRU, 4)

#define gCPU(r) EXTERN(gCPU)+r
#define gJITC(r) EXTERN(gJITC)+r

.text

.balign 16
##############################################################################################
##
EXPORT(ppc_mmu_tlb_invalidate_all_asm):
	cld
	or	%eax, -1
	mov	%ecx, TLB_ENTRIES*4*3 / 4
	mov	%edi, offset gJITC(tlb_code_0_eff)
	rep	stosd
	ret

##############################################################################################
##
##	IN: 	%eax: effective address to invalidate
##
EXPORT(ppc_mmu_tlb_invalidate_entry_asm):
	mov	%ecx, %eax
	or	%eax, -1
	shr	%ecx, 12
	and	%ecx, TLB_ENTRIES-1
	mov	[gJITC(tlb_code_0_eff+%ecx*4)], %eax
	mov	[gJITC(tlb_data_0_eff+%ecx*4)], %eax
	mov	[gJITC(tlb_data_8_eff+%ecx*4)], %eax
	ret
	
##############################################################################################
##		read_physical_word_pg
.macro read_physical_word_pg r1, r2
	mov	\r2, [EXTERN(gMemory)]
	mov	\r2, [\r2 + \r1]
	bswap	\r2
.endm

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

###############################################################################
##		bat_lookup
#define bat_lookup(di, n, rw, datacode)                                        \
	mov	%edx, [gCPU(di##bat_bl + n*4)];                                \
	mov	%ebx, [gCPU(di##bat_bepi + n*4)];                              \
	                                                                       \
	and	%edx, %eax;                                                    \
	cmp	%ebx, %edx;                                                    \
	jne	3f;                                                            \
	                                                                       \
	bt	dword ptr [gCPU(msr)], 14;                                     \
	mov	%ebp, 2;                                                       \
	sbb	%ebp, 0;	        /* BATU_Vp  / BATU_Vs */               \
	                                                                       \
	test	[gCPU(di##batu + n*4)], %ebp;                                  \
	jz	3f;                                                            \
	                                                                       \
	/* FIXME: check access rights */                                       \
	mov	%esi, %eax;                                                    \
	mov	%edi, %eax;                                                    \
	mov	%edx, 0xfffff000;                                              \
	and     %eax, [gCPU(di##bat_nbl + n*4)];                               \
	or      %eax, [gCPU(di##bat_brpn + n*4)];                              \
                                                                               \
/** TLB-Code */                                                                \
	shr	%edi, 12;                                                      \
	and	%esi, %edx;                                                    \
	and	%edi, TLB_ENTRIES-1;                                           \
	and	%edx, %eax;                                                    \
	mov	[gJITC(tlb_##datacode##_##rw##_eff+%edi*4)], %esi;             \
	mov	[gJITC(tlb_##datacode##_##rw##_phys+%edi*4)], %edx;            \
/*	add	dword ptr [gJITC(tlb_##datatcode##_##rw##_misses)], 1;   */    \
/*	adc	dword ptr [gJITC(tlb_##datatcode##_##rw##_misses+4)], 0; */    \
	ret	4;                                                             \
3:

##############################################################################################
##	pg_table_lookup
##
##	param1: PTE1_H or 0
##	param2: 0 for read, 8 for write
##	param3: data / code
##	param4: pt offset
#define pg_table_lookup(n, rw, datacode, offset)                               \
	read_physical_word_pg %ebx+offset, %eax;                               \
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
	cmp	%ecx, %ebp;                                                    \
	jne	1f;                                                            \
	                                                                       \
	and	%eax, 0x3f;     /* API */                                      \
	cmp	%eax, %edi;                                                    \
	jne	1f;                                                            \
	                                                                       \
	/* page found */                                                       \
	                                                                       \
	read_physical_word_pg %ebx+4+offset, %esi;                             \
	/* # %esi = pte2; */                                                   \
	                                                                       \
	/* FIXME: use bt trick? */                                             \
	test	dword ptr [gCPU(msr)], (1<<14); /* MSR_PR */                   \
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
	cmp	byte ptr [ppc_pte_protection + (rw) + 4*%eax + %ecx], 1;       \
.if n==0;                                                                      \
/*	add	%esp, 4;	/* hash1, no longer needed */                  \
	pop	%edx;                                                          \
.endif;                                                                        \
	pop	%eax;		/* the effective address */                    \
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
	add	%ebx, [EXTERN(gMemory)];                                       \
	mov	[%ebx+4+offset], %edx;                                         \
	                                                                       \
	and	%esi, 0xfffff000;                                              \
/** TLB-Code */                                                                \
	mov	%edx, %eax;                                                    \
	mov	%ecx, %eax;                                                    \
	shr	%edx, 12;                                                      \
	and	%ecx, 0xfffff000;                                              \
	and	%edx, TLB_ENTRIES-1;                                           \
	mov	[gJITC(tlb_## datacode ##_## rw ##_eff + %edx*4)], %ecx;       \
	mov	[gJITC(tlb_## datacode ##_## rw ##_phys + %edx*4)], %esi;      \
/*	add	dword ptr [gJITC(tlb_## datacode ##_## rw ##_misses)], 1;*/    \
/*	adc	dword ptr [gJITC(tlb_## datacode ##_## rw ##_misses+4)], 0;*/  \
/***/                                                                          \
	and	%eax, 0x00000fff;                                              \
	or	%eax, %esi;                                                    \
	ret	4;                                                             \
1:

##############################################################################################
##	protection_fault_%2_%3
protection_fault_0_code:
	# ppc_exception(PPC_EXC_ISI, PPC_EXC_SRR1_PROT, addr);
	pop	%edx		# return address is no longer needed
	pop	%ebx		# bytes to roll back
	add	%esp, %ebx
	mov	%ecx, (1<<27)	# PPC_EXC_SRR1_PROT
	jmp	EXTERN(ppc_isi_exception_asm)
protection_fault_0_data:
	# ppc_exception(PPC_EXC_DSI, PPC_EXC_DSISR_PROT, addr);
	pop	%edx		# return address is no longer needed
	pop	%ebx		# bytes to roll back
	add	%esp, %ebx
	mov	%ecx, (1<<27)	# PPC_EXC_DSISR_PROT
	jmp	EXTERN(ppc_dsi_exception_asm)
protection_fault_8_data:
	# ppc_exception(PPC_EXC_DSI, PPC_EXC_DSISR_PROT | PPC_EXC_DSISR_STORE, addr);
	pop	%edx			# return address is no longer needed
	pop	%ebx			# bytes to roll back
	add	%esp, %ebx
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
	cmp	%ecx, [gJITC(tlb_##datacode##_##rw##_eff + %edx*4)];           \
	jne	1f;                                                            \
	/*                                                                     \
	 *	if a tlb entry is invalid, its                                 \
	 *	lower 12 bits are 1, so the cmp is guaranteed to fail.         \
	 */                                                                    \
/*	add	dword ptr [gJITC(tlb_##datacode##_##rw##_hits)], 1;   */       \
/*	adc	dword ptr [gJITC(tlb_##datacode##_##rw##_hits+4)], 0; */       \
	and	%eax, 0x00000fff;                                              \
	or	%eax, [gJITC(tlb_##datacode##_##rw##_phys + %edx*4)];          \
	ret	4;                                                             \
1:                                                                             \

.balign 16
ppc_effective_to_physical_code_ret:
	mov	%edx, %eax
	mov	%ecx, %eax
	shr	%edx, 12
	and	%ecx, 0xfffff000
	and	%edx, TLB_ENTRIES-1
	mov	[gJITC(tlb_code_0_eff+%edx*4)], %ecx
	mov	[gJITC(tlb_code_0_phys+%edx*4)], %ecx
	ret	4

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

	test	byte ptr [gCPU(msr)], (1<<5)	# MSR_IR
	jz	ppc_effective_to_physical_code_ret

	bat_lookup(i, 0, 0, code)
	bat_lookup(i, 1, 0, code)
	bat_lookup(i, 2, 0, code)
	bat_lookup(i, 3, 0, code)

	mov	%ebx, %eax
	shr	%ebx, 28			# SR
	mov	%edx, [gCPU(sr+4*%ebx)]
	
	# test	%edx, SR_T --> die
	
	test	%edx, (1<<28)			# SR_N
	jnz	9f
	
	mov	%ebx, %eax
	mov 	%ebp, %edx
	shr	%ebx, 12
	mov	%edi, %eax
	and	%ebx, 0xffff
	shr	%edi, 22
	and	%ebp, 0xffffff
	and	%edi, 0x3f
	
	# now:
	# %eax = addr
	# %ebx = page_index
	# %ebp = VSID
	# %edi = api
	
	xor	%ebx, %ebp
	
	# %ebx = hash1
	
	push	%eax
	push	%ebx			# das brauch ich
	
	and	%ebx, [gCPU(pagetable_hashmask)]
	shl	%ebx, 6
	or	%ebx, [gCPU(pagetable_base)]
	
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
	pop	%ebx
	not	%ebx
	and	%ebx, [gCPU(pagetable_hashmask)]
	shl	%ebx, 6
	or	%ebx, [gCPU(pagetable_base)]

	pg_table_lookup(1<<6, 0, code, 0)
	pg_table_lookup(1<<6, 0, code, 8)
	pg_table_lookup(1<<6, 0, code, 16)
	pg_table_lookup(1<<6, 0, code, 24)
	pg_table_lookup(1<<6, 0, code, 32)
	pg_table_lookup(1<<6, 0, code, 40)
	pg_table_lookup(1<<6, 0, code, 48)
	pg_table_lookup(1<<6, 0, code, 56)

	# page lookup failed --> throw exception
	
	pop	%eax
	pop	%edx			# return address is no longer needed
	pop	%ecx			# bytes to roll back
	add	%esp, %ecx

	mov	%ecx, (1<<30)		# PPC_EXC_SRR1_PAGE
	jmp	EXTERN(ppc_isi_exception_asm)
9:
	# segment isnt executable --> throw exception
	pop	%edx			# return address is no longer needed
	pop	%ecx			# bytes to roll back
	add	%esp, %ecx

	mov	%ecx, (1<<28)		# PPC_EXC_SRR1_GUARD
	jmp	EXTERN(ppc_isi_exception_asm)

.balign 16
ppc_effective_to_physical_data_read_ret:
	mov	%edx, %eax
	mov	%ecx, %eax
	shr	%edx, 12
	and	%ecx, 0xfffff000
	and	%edx, TLB_ENTRIES-1
	mov	[gJITC(tlb_data_0_eff+%edx*4)], %ecx
	mov	[gJITC(tlb_data_0_phys+%edx*4)], %ecx
	ret	4

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

	test	byte ptr [gCPU(msr)], (1<<4)	# MSR_DR
	jz	ppc_effective_to_physical_data_read_ret
	                        
	bat_lookup(d, 0, 0, data)
	bat_lookup(d, 1, 0, data)
	bat_lookup(d, 2, 0, data)
	bat_lookup(d, 3, 0, data)

	mov	%ebx, %eax
	shr	%ebx, 28			# SR
	mov	%edx, [gCPU(sr+4*%ebx)]
	
	# test %edx, SR_T --> die
	
	mov	%ebx, %eax
	mov	%ebp, %edx
	shr	%ebx, 12
	mov	%edi, %eax
	and	%ebx, 0xffff
	shr	%edi, 22
	and	%ebp, 0xffffff
	and	%edi, 0x3f
	
	# now:
	# %eax = addr
	# %ebx = page_index
	# %ebp = VSID
	# %edi = api
	
	xor	%ebx, %ebp
	
	# %ebx = hash1
	
	push	%eax
	push	%ebx			# das brauch ich
	
	and	%ebx, [gCPU(pagetable_hashmask)]
	shl	%ebx, 6
	or	%ebx, [gCPU(pagetable_base)]
	
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
	pop	%ebx
	not	%ebx
	and	%ebx, [gCPU(pagetable_hashmask)]
	shl	%ebx, 6
	or	%ebx, [gCPU(pagetable_base)]

	pg_table_lookup(1<<6, 0, data, 0)
	pg_table_lookup(1<<6, 0, data, 8)
	pg_table_lookup(1<<6, 0, data, 16)
	pg_table_lookup(1<<6, 0, data, 24)
	pg_table_lookup(1<<6, 0, data, 32)
	pg_table_lookup(1<<6, 0, data, 40)
	pg_table_lookup(1<<6, 0, data, 48)
	pg_table_lookup(1<<6, 0, data, 56)

	# page lookup failed --> throw exception
	
	pop	%eax
	pop	%edx			# return address is no longer needed
	pop	%ecx			# bytes to roll back
	add	%esp, %ecx
	
	mov	%ecx, (1<<30)		# PPC_EXC_DSISR_PAGE
	jmp	EXTERN(ppc_dsi_exception_asm)

.balign 16
ppc_effective_to_physical_data_write_ret:
	mov	%edx, %eax
	mov	%ecx, %eax
	shr	%edx, 12
	and	%ecx, 0xfffff000
	and	%edx, TLB_ENTRIES-1
	mov	[gJITC(tlb_data_8_eff+%edx*4)], %ecx
	mov	[gJITC(tlb_data_8_phys+%edx*4)], %ecx
	ret	4

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

	test	byte ptr [gCPU(msr)], (1<<4)	# MSR_DR
	jz	ppc_effective_to_physical_data_write_ret
	
	bat_lookup(d, 0, 8, data)
	bat_lookup(d, 1, 8, data)
	bat_lookup(d, 2, 8, data)
	bat_lookup(d, 3, 8, data)

	mov	%ebx, %eax
	shr	%eax, 28			# SR
	mov	%edx, [gCPU(sr+4*%eax)]
	
	# test %edx, SR_T --> die
	
	mov	%eax, %ebx
	mov	%edi, %ebx
	mov	%ebp, %edx
	shr	%ebx, 12
	and	%ebx, 0xffff
	shr	%edi, 22
	and	%ebp, 0xffffff
	and	%edi, 0x3f
	
	# now:
	# %eax = addr
	# %ebx = page_index
	# %ebp = VSID
	# %edi = api
	
	xor	%ebx, %ebp
	
	# %ebx = hash1
	
	push	%eax
	push	%ebx			# das brauch ich
	
	and	%ebx, [gCPU(pagetable_hashmask)]
	shl	%ebx, 6
	or	%ebx, [gCPU(pagetable_base)]
	
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
	mov	%ebx, [%esp]
	add	%esp, 4
	not	%ebx
	and	%ebx, [gCPU(pagetable_hashmask)]
	shl	%ebx, 6
	or	%ebx, [gCPU(pagetable_base)]

	pg_table_lookup(1<<6, 8, data, 0)
	pg_table_lookup(1<<6, 8, data, 8)
	pg_table_lookup(1<<6, 8, data, 16) 
	pg_table_lookup(1<<6, 8, data, 24)
	pg_table_lookup(1<<6, 8, data, 32)
	pg_table_lookup(1<<6, 8, data, 40)
	pg_table_lookup(1<<6, 8, data, 48)
	pg_table_lookup(1<<6, 8, data, 56)

	# page lookup failed --> throw exception
	
	pop	%eax
	pop	%edx			# return address is no longer needed
	pop	%ebx			# bytes to roll back
	add	%esp, %ebx

	mov	%ecx, (1<<30)|(1<<25)	# PPC_EXC_DSISR_PAGE | PPC_EXC_DSISR_STORE
	jmp	EXTERN(ppc_dsi_exception_asm)

.balign 16
##############################################################################################
##	uint32 FASTCALL ppc_effective_write_byte()
##
##	IN	%eax: address to translate
##		%esi: current client pc offset
##		%dl: byte to be written
##
##	WILL NOT RETURN ON EXCEPTION!
##
EXPORT(ppc_write_effective_byte_asm):
	mov	[gCPU(pc_ofs)], %esi

	push	%edx
	push	8			# roll back 8 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	cmp	%eax, [EXTERN(gMemorySize)]
	pop	%edx
	jae	1f
	add	%eax, [EXTERN(gMemory)]
	mov	[%eax], %dl
	ret
1:
	mov	%ecx, 1
	movzx	%edx, %dl
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
	mov	[gCPU(pc_ofs)], %esi
	mov	%ebx, %eax
	and	%ebx, 0xfff
	cmp	%ebx, 4095
	jae	1f

	push	%edx
	push	8			# roll back 8 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	cmp	%eax, [EXTERN(gMemorySize)]
	pop	%edx
	jae	2f
	add	%eax, [EXTERN(gMemory)]
	mov	[%eax], %dh
	mov	[%eax+1], %dl
	ret
2:
	xchg	%dh, %dl
	mov	%ecx, 2
	movzx	%edx, %dx
	jmp	EXTERN(io_mem_write_glue)

1:
	push	%edx
	push	%eax
	push	12			# roll back 12 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	mov	%ebx, %eax
	pop	%eax
	push	%ebx
	inc	%eax
	push	12			# roll back 12 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	pop	%ebx
	pop	%edx
	cmp	%ebx, [EXTERN(gMemorySize)]
	jae	1f
	add	%ebx, [EXTERN(gMemory)]
	mov	[%ebx], %dh
	3:
	cmp	%eax, [EXTERN(gMemorySize)]
	jae	2f
	add	%eax, [EXTERN(gMemory)]
	mov	[%eax], %dl
	ret

1:
	push	%eax
	push	%edx
	movzx	%edx, %dh
	mov	%eax, %ebx
	mov	%ecx, 1
	call	EXTERN(io_mem_write_glue)
	pop	%edx
	pop	%eax
	jmp	3b
2:
	movzx	%edx, %dl
	mov	%ecx, 1
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
	mov	[gCPU(pc_ofs)], %esi
	mov	%ebx, %eax
	and	%ebx, 0xfff
	cmp	%ebx, 4093
	jae	1f

	bswap	%edx

	push	%edx
	push	8			# roll back 8 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	cmp	%eax, [EXTERN(gMemorySize)]
	pop	%edx
	jae	2f
	add	%eax, [EXTERN(gMemory)]
	mov	[%eax], %edx
	ret

2:
	mov	%ecx, 4
	jmp	EXTERN(io_mem_write_glue)

1:
	push	%edx
	push	%eax
	push	12			# roll back 12 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	mov	%ebx, %eax
	pop	%eax
	add	%eax, 4
	push	%ebx
	push	12			# roll back 12 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	pop	%ebx
	pop	%edx
	mov	%ebp, %ebx
	and	%ebp, 0xfff
	neg	%ebp
	add	%ebp, 4096
	cmp	%ebx, [EXTERN(gMemorySize)]
	jae	2f
	add	%ebx, [EXTERN(gMemory)]
	1:
		rol	%edx, 8
		mov	[%ebx], %dl
		inc	%ebx
		dec	%ebp
	jnz	1b
	4:
	mov	%ebp, %eax
	and	%eax, 0xfffff000
	and	%ebp, 0x00000fff
	cmp	%eax, [EXTERN(gMemorySize)]
	jae	3f
	add	%eax, [EXTERN(gMemory)]
	1:
		rol	%edx, 8
		mov	[%eax], %dl
		inc	%eax
		dec	%ebp
	jnz	1b
	ret

2:
	push	%eax
	1:
		rol	%edx, 8
		push	%ebp
		#push	%ebx
		push	%edx
		movzx	%edx, %dl
		mov	%eax, %ebx
		mov	%ecx, 1
		call	EXTERN(io_mem_write_glue)
		pop	%edx
		pop	%ebp
		inc	%ebx
		dec	%ebp
	jnz	1b
	pop	%eax
	jmp	4b
3:
	1:
		rol	%edx, 8
		push	%ebp
		push	%eax
		push	%edx
		movzx	%edx, %dl
		mov	%ecx, 1
		call	EXTERN(io_mem_write_glue)
		pop	%edx
		pop	%eax
		pop	%ebp
		inc	%eax
		dec	%ebp
	jnz	1b
	ret
.balign 16
##############################################################################################
##	uint32 FASTCALL ppc_effective_write_dword()
##
##	IN	%eax: address to translate
##		%ecx:%edx dword to be written
##		%esi: current client pc offset
##
##	WILL NOT RETURN ON EXCEPTION!
##
EXPORT(ppc_write_effective_dword_asm):
	mov	[gCPU(pc_ofs)], %esi
	mov	%ebx, %eax
	and	%ebx, 0xfff
	cmp	%ebx, 4089
	jae	2f			# overlap

	bswap	%ecx
	bswap	%edx

	push	%ecx
	push	%edx
	push	12			# roll back 12 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	cmp	%eax, [EXTERN(gMemorySize)]
	pop	%edx
	pop	%ecx
	jae	1f
	add	%eax, [EXTERN(gMemory)]
	mov	[%eax], %ecx
	mov	[%eax+4], %edx
	ret
1:
	mov	%ebx, %ecx
	mov	%ecx, %edx
	mov	%edx, %ebx
	jmp	EXTERN(io_mem_write64_glue)

2:
	push	%ecx
	push	%edx
	push	%eax
	push	16			# roll back 16 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	mov	%ebx, %eax
	pop	%eax
	add	%eax, 8
	push	%ebx
	push	16			# roll back 16 bytes in case of exception
	call	ppc_effective_to_physical_data_write
	pop	%ebx
	pop	%edx
	pop	%ecx
	mov	%ebp, %ebx
	and	%ebp, 0xfff
	neg	%ebp
	add	%ebp, 4096
	bswap	%ecx
	bswap	%edx
	cmp	%ebx, [EXTERN(gMemorySize)]
	jae	3f
	add	%ebx, [EXTERN(gMemory)]
	1:
		mov	[%ebx], %cl
		shrd	%ecx, %edx, 8
		inc	%ebx
		shr	%edx, 8
		dec	%ebp
	jnz	1b
	4:
	mov	%ebp, %eax
	and	%eax, 0xfffff000
	and	%ebp, 0x00000fff
	cmp	%eax, [EXTERN(gMemorySize)]
	jae	5f
	add	%eax, [EXTERN(gMemory)]
	1:
		mov	[%eax], %cl
		shrd	%ecx, %edx, 8
		inc	%eax
		shr	%edx, 8
		dec	%ebp
	jnz	1b
	ret
3:
	1:
		pusha
		movzx	%edx, %cl
		mov	%eax, %ebx
		mov	%ecx, 1
		call	EXTERN(io_mem_write_glue)
		popa
		shrd	%ecx, %edx, 8
		inc	%ebx
		shr	%edx, 8
		dec	%ebp
	jnz	1b
	jmp	4b
5:
	1:
		pusha
		movzx	%edx, %cl
		mov	%ecx, 1
		call	EXTERN(io_mem_write_glue)
		popa
		shrd	%ecx, %edx, 8
		inc	%eax
		shr	%edx, 8
		dec	%ebp
	jnz	1b
	ret
.balign 16
##############################################################################################
##	uint32 FASTCALL ppc_effective_write_qword()
##
##	IN	%eax: address to translate
##		%edx: address from which to get the values to be written
##		%esi: current client pc offset
##
##	WILL NOT RETURN ON EXCEPTION!
##
EXPORT(ppc_write_effective_qword_asm):
	mov	[gCPU(pc_ofs)], %esi
	and	%eax, 0xfffffff0
	push	%edx

	push	8			# roll back 8 bytes in case of exception
	call	ppc_effective_to_physical_data_write

	cmp	%eax, [EXTERN(gMemorySize)]
	pop	%edx
	jae	1f

	add	%eax, [EXTERN(gMemory)]

	mov	%ebx, [%edx]
	mov	%ecx, [%edx+4]
	mov	%edi, [%edx+8]
	mov	%esi, [%edx+12]

	bswap	%ebx
	bswap	%ecx
	bswap	%edi
	bswap	%esi

	mov	[%eax+12], %ebx
	mov	[%eax+8], %ecx
	mov	[%eax+4], %edi
	mov	[%eax], %esi
	ret
1:
	call	EXTERN(io_mem_write128_glue)
	ret

ppc_write_effective_qword_sse_asm:
	mov	[gCPU(pc_ofs)], %esi
	and	%eax, 0xfffffff0
	push	%edx

	push	8			# roll back 8 bytes in case of exception
	call	ppc_effective_to_physical_data_write

	cmp	%eax, [EXTERN(gMemorySize)]
	pop	%edx
	jae	1f

	add	%eax, [EXTERN(gMemory)]

	movaps	[%eax], %xmm0
	ret
1:
	movaps	[%edx], %xmm0
	call	EXTERN(io_mem_write128_native_glue)
	ret
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
	mov	[gCPU(pc_ofs)], %esi

	push	4			# roll back 4 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	cmp	%eax, [EXTERN(gMemorySize)]
	jae	1f
	add	%eax, [EXTERN(gMemory)]
	movzx	%edx, byte ptr [%eax]
	ret
1:
	mov	%edx, 1
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
	mov	[gCPU(pc_ofs)], %esi
	mov	%ebx, %eax
	and	%ebx, 0xfff
	cmp	%ebx, 4095
	jae	1f

	push	4			# roll back 4 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	cmp	%eax, [EXTERN(gMemorySize)]
	jae	2f
	add	%eax, [EXTERN(gMemory)]	
	movzx	%edx, word ptr [%eax]
	xchg	%dl, %dh
	ret
2:
	mov	%edx, 2
	call	EXTERN(io_mem_read_glue)
	xchg	%al, %ah
	movzx	%edx, %ax
	ret

1:
	push	%eax
	push	8			# roll back 8 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	xor	%edx, %edx
	cmp	%eax, [EXTERN(gMemorySize)]
	jae	2f
	add	%eax, [EXTERN(gMemory)]
	mov	%dh, [%eax]
1:
	pop	%eax
	push	%edx
	inc	%eax
	push	8			# roll back 8 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	pop	%edx
	cmp	%eax, [EXTERN(gMemorySize)]
	jae	3f
	add	%eax, [EXTERN(gMemory)]
	mov	%dl, [%eax]
	ret

2:
	pusha
	mov	%edx, 1
	call	EXTERN(io_mem_read_glue)
	mov	[gCPU(temp2)], %al
	popa
	mov	%dh, [gCPU(temp2)]
	jmp	1b
3:
	push	%edx
	mov	%edx, 1
	call	EXTERN(io_mem_read_glue)
	pop	%edx
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
	mov	[gCPU(pc_ofs)], %esi
	mov	%ebx, %eax
	and	%ebx, 0xfff
	cmp	%ebx, 4095
	jae	1f

	push	4			# roll back 4 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	cmp	%eax, [EXTERN(gMemorySize)]
	jae	2f
	add	%eax, [EXTERN(gMemory)]
	mov	%cx, [%eax]
	xchg	%ch, %cl
	movsx	%edx, %cx
	ret
2:
	mov	%edx, 2
	call	EXTERN(io_mem_read_glue)
	xchg	%ah, %al
	movsx	%edx, %ax
	ret

1:
	push	%eax
	push	8			# roll back 8 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	cmp	%eax, [EXTERN(gMemorySize)]
	jae	2f
	add	%eax, [EXTERN(gMemory)]
	mov	%ch, [%eax]
1:
	pop	%eax
	push	%ecx
	inc	%eax
	push	8			# roll back 8 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	pop	%ecx
	cmp	%eax, [EXTERN(gMemorySize)]
	jae	3f
	add	%eax, [EXTERN(gMemory)]
	mov	%cl, [%eax]
	movsx	%edx, %cx
	ret

2:
	pusha
	mov	%edx, 1
	call	EXTERN(io_mem_read_glue)
	mov	[gCPU(temp2)], %al
	popa
	mov	%ch, [gCPU(temp2)]
	jmp	1b
3:
	push	%ecx
	mov	%edx, 1
	call	EXTERN(io_mem_read_glue)
	pop	%ecx
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
	mov	[gCPU(pc_ofs)], %esi
	mov	%ebx, %eax
	and	%ebx, 0xfff
	cmp	%ebx, 4093
	jae	1f

	push	4			# roll back 4 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	cmp	%eax, [EXTERN(gMemorySize)]
	jae	2f
	add	%eax, [EXTERN(gMemory)]
	mov	%edx, [%eax]
	bswap	%edx
	ret
2:
	mov	%edx, 4
	call	EXTERN(io_mem_read_glue)
	mov	%edx, %eax
	bswap	%edx
	ret
1:
	push	%eax
	push	%ebx
	push	12			# roll back 12 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	pop	%ebx
	mov	%ecx, 4096
	sub	%ecx, %ebx
	cmp	%eax, [EXTERN(gMemorySize)]
	jae	2f
	add	%eax, [EXTERN(gMemory)]
	1:
		shl	%edx, 8
		mov	%dl, [%eax]
		inc	%eax
		dec	%ecx
	jnz	1b
	4:
	pop	%eax
	push	%edx
	add	%eax, 4
	push	%ebx
	and	%eax, 0xfffff000
	push	12			# roll back 12 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	pop	%ebx
	pop	%edx
	sub	%ebx, 4092
	cmp	%eax, [EXTERN(gMemorySize)]
	jae	3f
	add	%eax, [EXTERN(gMemory)]
	1:
		shl	%edx, 8
		mov	%dl, [%eax]
		inc	%eax
		dec	%ebx
	jnz	1b
	ret

2:
	1:
		shl	%edx, 8
		pusha
		mov	%edx, 1
		call	EXTERN(io_mem_read_glue)
		mov	[gCPU(temp2)], %al
		popa
		mov	%dl, [gCPU(temp2)]
		inc	%eax
		dec	%ecx
	jnz	1b
	jmp	4b
3:
	1:
		shl	%edx, 8
		pusha
		mov	%edx, 1
		call	EXTERN(io_mem_read_glue)
		mov	[gCPU(temp2)], %al
		popa
		mov	%dl, [gCPU(temp2)]
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
##	OUT	%ecx:%edx dword
##
##	WILL NOT RETURN ON EXCEPTION!
##
EXPORT(ppc_read_effective_dword_asm):
	mov	[gCPU(pc_ofs)], %esi
	mov	%ebx, %eax
	and	%ebx, 0xfff
	cmp	%ebx, 4089
	jae	1f

	push	4			# roll back 4 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	cmp	%eax, [EXTERN(gMemorySize)]
	jae	2f
	add	%eax, [EXTERN(gMemory)]
	mov	%ecx, [%eax]
	mov	%edx, [%eax+4]
	bswap	%ecx
	bswap	%edx
	ret
2:
	call	EXTERN(io_mem_read64_glue)
	mov	%ecx, %eax
	mov	%edx, %edx
	bswap	%ecx
	bswap	%edx
	ret

1:
	push	%eax
	push	%ebx
	push	12			# roll back 12 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	pop	%ebx
	mov	%ebp, 4096
	sub	%ebp, %ebx
	cmp	%eax, [EXTERN(gMemorySize)]
	jae	2f
	add	%eax, [EXTERN(gMemory)]
	1:
		shld	%ecx, %edx, 8
		shl	%edx, 8
		mov	%dl, [%eax]
		inc	%eax
		dec	%ebp
	jnz	1b
	4:
	pop	%eax
	push	%ecx
	push	%edx
	add	%eax, 8
	push	%ebx
	and	%eax, 0xfffff000
	push	16			# roll back 16 bytes in case of exception
	call	ppc_effective_to_physical_data_read
	pop	%ebx
	pop	%edx
	pop	%ecx
	sub	%ebx, 4088
	cmp	%eax, [EXTERN(gMemorySize)]
	jae	3f
	add	%eax, [EXTERN(gMemory)]
	1:
		shld	%ecx, %edx, 8
		shl	%edx, 8
		mov	%dl, [%eax]
		inc	%eax
		dec	%ebx
	jnz	1b
	ret

2:
	1:
		shld	%ecx, %edx, 8
		shl	%edx, 8
		pusha
		mov	%edx, 1
		call	EXTERN(io_mem_read_glue)
		mov	[gCPU(temp2)], %al
		popa
		mov	%dl, [gCPU(temp2)]
		inc	%eax
		dec	%ebp
	jnz	1b
	jmp	4b
3:
	1:
		shld	%ecx, %edx, 8
		shl	%edx, 8
		pusha
		mov	%edx, 1
		call	EXTERN(io_mem_read_glue)
		mov	[gCPU(temp2)], %al
		popa
		mov	%dl, [gCPU(temp2)]
		inc	%eax
		dec	%ebx
	jnz	1b
	ret
.balign 16
##############################################################################################
##	uint32 FASTCALL ppc_read_effective_qword()
##
##	IN	%eax: address to translate
##		%edx: address to store the qword
##		%esi: current client pc offset
##
##	WILL NOT RETURN ON EXCEPTION!
##
EXPORT(ppc_read_effective_qword_asm):
	mov	[gCPU(pc_ofs)], %esi
	and	%eax, 0xfffffff0
	push	%edx

	push	8			# roll back 8 bytes in case of exception
	call	ppc_effective_to_physical_data_read

	cmp	%eax, [EXTERN(gMemorySize)]
	pop	%edx
	jae	1f

	add	%eax, [EXTERN(gMemory)]

	mov	%ebx, [%eax]
	mov	%ecx, [%eax+4]
	mov	%edi, [%eax+8]
	mov	%esi, [%eax+12]

	bswap	%ebx
	bswap	%ecx
	bswap	%edi
	bswap	%esi

	mov	[%edx+12], %ebx
	mov	[%edx+8], %ecx
	mov	[%edx+4], %edi
	mov	[%edx], %esi
	ret
1:
	call	EXTERN(io_mem_read128_glue)
	ret

EXPORT(ppc_read_effective_qword_sse_asm):
	mov	[gCPU(pc_ofs)], %esi
	and	%eax, 0xfffffff0
	push	%edx

	push	8			# roll back 8 bytes in case of exception
	call	ppc_effective_to_physical_data_read

	cmp	%eax, [EXTERN(gMemorySize)]
	pop	%edx
	jae	1f

	add	%eax, [EXTERN(gMemory)]

	movaps	%xmm0, [%eax]

	mov	%ebx, [%eax]
	mov	%ecx, [%eax+4]
	mov	%edi, [%eax+8]
	mov	%esi, [%eax+12]

	bswap	%ebx
	bswap	%ecx
	bswap	%edi
	bswap	%esi

	mov	[%edx+12], %ebx
	mov	[%edx+8], %ecx
	mov	[%edx+4], %edi
	mov	[%edx], %esi
	ret

1:
	push	%edx
	call	EXTERN(io_mem_read128_native_glue)
	pop	%edx

	movaps	%xmm0, [%edx]

	mov	%eax, [%edx]
	mov	%ebx, [%edx+4]
	mov	%edi, [%edx+8]
	mov	%esi, [%edx+12]

	bswap	%eax
	bswap	%ebx
	bswap	%edi
	bswap	%esi

	mov	[%edx+12], %esi
	mov	[%edx+8], %edi
	mov	[%edx+4], %ebx
	mov	[%edx], %eax
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
	mov	[gCPU(pc_ofs)], %esi
	mov	%edi, 1
	
	1:
		dec	%edi
		jnz	2f
			mov	%edx, [gCPU(gpr+4*%ebx)]
			inc	%ebx
			mov	%edi, 4
			and	%ebx, 0x1f	
		2:
		push	%eax
		push	%ecx
		push	%ebx
		push	%edi
		push	%edx
		push	24			# roll back 24 bytes in case of exception
		call	ppc_effective_to_physical_data_write
		cmp	%eax, [EXTERN(gMemorySize)]
		pop	%edx
		mov	%ecx, %edx
		jae	3f
		shr	%ecx, 24
		add	%eax, [EXTERN(gMemory)]
		mov	[%eax], %cl
	4:
		pop	%edi
		pop	%ebx
		pop	%ecx
		pop	%eax
		shl	%edx, 8
		inc	%eax
		dec	%ecx
	jnz	1b
	ret

3:
	push	%edx
	mov	%ecx, 1
	shr	%edx, 24
	call	EXTERN(io_mem_write_glue)
	pop	%edx
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
	mov	[gCPU(pc_ofs)], %esi
	mov	%edi, 4
	1:
		or	%edi, %edi
		jnz	2f
			mov	[gCPU(gpr+4*%ebx)], %edx
			inc	%ebx
			mov	%edi, 4
			and	%ebx, 0x1f
			xor	%edx, %edx
		2:

		push	%eax
		push	%ecx
		push	%ebx
		push	%edi
		push	%edx
		push	24			# roll back 24 bytes in case of exception
		call	ppc_effective_to_physical_data_read
		pop	%edx
		cmp	%eax, [EXTERN(gMemorySize)]
		jae	3f
		add	%eax, [EXTERN(gMemory)]
		shl	%edx, 8
		mov	%dl, byte ptr [%eax]
	4:
		pop	%edi
		pop	%ebx
		pop	%ecx
		pop	%eax
	
		dec	%edi
		inc	%eax
		dec	%ecx
	jnz	1b
	
	or	%edi, %edi
	jz	5f
		2:
			shl	%edx, 8		
			dec	%edi
		jnz	2b

	5:
	mov	[gCPU(gpr)+4*%ebx], %edx
	ret

3:
	push	%edx
	mov	%edx, 1
	call	EXTERN(io_mem_read_glue)
	pop	%edx
	shl	%edx, 8
	mov	%dl, %al
	jmp	4b

.balign 16
##############################################################################################
##	uint32 FASTCALL ppc_opc_icbi_asm()
##
##	IN	%eax: effective address
EXPORT(ppc_opc_icbi_asm):
	mov	[gCPU(pc_ofs)], %esi
	push	4			# roll back 4 bytes in case of exception
	add	%eax, 1			# FIXME: WTF???
	call	ppc_effective_to_physical_data_read
	cmp	%eax, [EXTERN(gMemorySize)]
	mov	%ebp, [gJITC(clientPages)]
	jae	1f
	shr	%eax, 12
	cmp	dword ptr [%ebp+%eax*4], 0
	jnz	2f
1:
	ret
	
2:
	mov	%eax, [%ebp+%eax*4]
	jmp	EXTERN(jitcDestroyAndFreeClientPage)
