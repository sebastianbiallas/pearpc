#define TLB_ENTRIES 32

STRUCT    #PPC_CPU_State
	MEMBER(jitc, 8)
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
	MEMBER(pdec, 8)
	MEMBER(ptb, 8)

	MEMBER(temp, 4)
	MEMBER(temp2, 4)
	MEMBER(x87cw, 4)
	MEMBER(pc_ofs, 4)
	MEMBER(current_code_base, 4)

STRUCT	#JITC
	MEMBER(clientPages, 8)
	MEMBER(tlb_code_0_eff, TLB_ENTRIES*4)
	MEMBER(tlb_data_0_eff, TLB_ENTRIES*4)
	MEMBER(tlb_data_8_eff, TLB_ENTRIES*4)
	MEMBER(tlb_code_0_phys, TLB_ENTRIES*4)
	MEMBER(tlb_data_0_phys, TLB_ENTRIES*4)
	MEMBER(tlb_data_8_phys, TLB_ENTRIES*4)

	MEMBER(tlb_code_0_hits, 8)
	MEMBER(tlb_data_0_hits, 8)
	MEMBER(tlb_data_8_hits, 8)
	MEMBER(tlb_code_0_misses, 8)
	MEMBER(tlb_data_0_misses, 8)
	MEMBER(tlb_data_8_misses, 8)

STRUCT	#ClientPage
	MEMBER(entrypoints, 1024*8)
	MEMBER(tcf_current, 8)
	MEMBER(baseaddress, 4)
	MEMBER(bytesLeft, 4)
	MEMBER(tcp, 8)
	MEMBER(moreRU, 8)
	MEMBER(lessRU, 8)

#define curCPU(r) %rdi+r
#define curCPUoffset(frame) %rsp+(8+8*frame)

.macro checkCurCPU
.endm
.macro checkCurCPUdebug
	cmp	%rdi, [gCPU]
	je	9f
	call 	jitc_error
9:
	mov	%rdi, [curCPU(jitc)]
	cmp	%rdi, [gJITC]
	je	9f
	xor	%eax, %eax
	mov	[%rax], %rax
9:
	mov	%rdi, [gCPU]
.endm

.macro getCurCPU frame
	lea	%rdi, [curCPUoffset(\frame)]
	checkCurCPU
.endm
