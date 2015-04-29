// as does not have a ~ operator..
#define ASM_NEG32(a) (0xffffffff-(a))

#define TLB_ENTRIES 32


//STRUCT(PPC_CPU_State)
#define jitc 0
#define gpr (jitc+8)
#define fpr (gpr+32*4)
#define cr (fpr+32*8)
#define fpscr (cr+4)
#define xer (fpscr+4)
#define xer_ca (xer+4)
#define lt_reg (xer_ca+4)
#define ctr (lt_reg+4)
#define msr (ctr+4)
#define pvr (msr+4)
#define ibatu (pvr + 4)
#define ibatl (ibatu + 4*4)
#define ibat_bl (ibatl + 4*4)
#define ibat_nbl (ibat_bl + 4*4)
#define ibat_bepi (ibat_nbl + 4*4)
#define ibat_brpn (ibat_bepi + 4*4)

#define dbatu (ibat_brpn + 4*4)
#define dbatl (dbatu + 4*4)
#define dbat_bl (dbatl + 4*4)
#define dbat_nbl (dbat_bl + 4*4)
#define dbat_bepi (dbat_nbl + 4*4)
#define dbat_brpn (dbat_bepi + 4*4)

#define sdr1 (dbat_brpn + 4*4)

#define sr (sdr1 + 4)

#define dar (sr + 16*4)
#define dsisr (dar + 4)
#define sprg (dsisr + 4)
#define srr0 (sprg + 4*4)
#define srr1 (srr0 + 4)

#define decr (srr1+4)
#define ear (decr+4)
#define pir (ear+4)
#define tb (pir+4)

#define hid (tb+8)

#define pc (hid + 16*4)
#define npc (pc + 4)
#define current_opc (npc + 4)

#define exception_pending (current_opc + 4)
#define dec_exception (exception_pending+1)
#define ext_exception (exception_pending+2)
#define stop_exception (exception_pending+3)
#define singlestep_ignore (exception_pending+4)

#define pagetable_base (exception_pending+8)
#define pagetable_hashmask (pagetable_base+4)
#define reserve (pagetable_hashmask+4)
#define have_reservation (reserve+4)

#define tlb_last (have_reservation + 4)
#define tlb_pa (tlb_last + 4)
#define tlb_va (tlb_pa + 4*4)
#define pdec (tlb_va + 4*4)
#define ptb (pdec + 8)

#define temp (ptb + 8)
#define temp2 (temp + 4)
#define x87cw (temp2 + 4)
#define pc_ofs (x87cw + 4)
#define current_code_base (pc_ofs + 4)

#define tlb_code_0_eff (current_code_base + 4)
#define tlb_data_0_eff (tlb_code_0_eff + TLB_ENTRIES*4)
#define tlb_data_8_eff (tlb_data_0_eff + TLB_ENTRIES*4)
#define tlb_code_0_phys (tlb_data_8_eff + TLB_ENTRIES*4)
#define tlb_data_0_phys  (tlb_code_0_phys + TLB_ENTRIES*8)
#define tlb_data_8_phys (tlb_data_0_phys + TLB_ENTRIES*8)

//STRUCT(JITC)
#define clientPages 0

//STRUCT(ClientPage)
#define entrypoints 0
#define tcf_current (entrypoints + 1024*8)
#define baseaddress (tcf_current + 8)
#define bytesLeft (baseaddress + 4)
#define tcp (bytesLeft + 4)
#define moreRU (tcp + 8)
#define lessRU (moreRU + 8)

#define curCPU(r) rdi+r
#define curCPUoffset(frame) rsp+(8+8*frame)

.macro checkCurCPU
.endm
.macro checkCurCPUdebug
	cmp	rdi, [gCPU]
	je	9f
	call 	jitc_error
9:
	mov	rdi, [curCPU(jitc)]
	cmp	rdi, [gJITC]
	je	9f
	xor	eax, eax
	mov	[rax], rax
9:
	mov	rdi, [gCPU]
.endm

.macro getCurCPU frame
	lea	rdi, [curCPUoffset(\frame)]
	checkCurCPU
.endm

.text
