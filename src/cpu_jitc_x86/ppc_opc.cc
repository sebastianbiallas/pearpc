/*
 *	PearPC
 *	ppc_opc.cc
 *
 *	Copyright (C) 2003, 2004 Sebastian Biallas (sb@biallas.net)
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

#include "debug/tracers.h"
#include "io/pic/pic.h"
#include "info.h"
#include "ppc_cpu.h"
#include "ppc_exc.h"
#include "ppc_mmu.h"
#include "ppc_opc.h"
#include "ppc_dec.h"

#include "jitc.h"
#include "jitc_asm.h"
#include "x86asm.h"

static uint64 gDECwriteITB;
static uint64 gDECwriteValue;

static void readDEC()
{
	uint64 itb = ppc_get_cpu_ideal_timebase() - gDECwriteITB;
	gCPU.dec = gDECwriteValue - itb;
//	PPC_OPC_WARN("read  dec=%08x\n", gCPU.dec);
}

static void writeDEC()
{
//	PPC_OPC_WARN("write dec=%08x\n", gCPU.dec);
	uint64 q = 1000000000ULL*gCPU.dec / gClientTimeBaseFrequency;

	sys_set_timer(gDECtimer, q / 1000000000ULL,
				  q % 1000000000ULL, false);

	gDECwriteValue = gCPU.dec;
	gDECwriteITB = ppc_get_cpu_ideal_timebase();
}

void ppc_set_msr(uint32 newmsr)
{
/*	if ((newmsr & MSR_EE) && !(gCPU.msr & MSR_EE)) {
		if (pic_check_interrupt()) {
			gCPU.exception_pending = true;
			gCPU.ext_exception = true;
		}
	}*/
	ppc_mmu_tlb_invalidate();
#ifndef PPC_CPU_ENABLE_SINGLESTEP
	if (newmsr & MSR_SE) {
		SINGLESTEP("");
		PPC_CPU_WARN("MSR[SE] (singlestep enable) set, but compiled w/o SE support.\n");
	}
#else 
	gCPU.singlestep_ignore = true;
#endif
	if (newmsr & PPC_CPU_UNSUPPORTED_MSR_BITS) {
		PPC_CPU_ERR("unsupported bits in MSR set: %08x @%08x\n", newmsr & PPC_CPU_UNSUPPORTED_MSR_BITS, gCPU.pc);
	}
	if (newmsr & MSR_POW) {
		// doze();
		newmsr &= ~MSR_POW;
	}
	gCPU.msr = newmsr;
	
}

void ppc_opc_gen_check_privilege()
{
	if (!gJITC.checkedPriviledge) {
		jitcFloatRegisterClobberAll();
		jitcClobberCarryAndFlags();
		NativeReg r1 = jitcGetClientRegister(PPC_MSR);
		asmALURegImm(X86_TEST, r1, MSR_PR);
		NativeAddress fixup = asmJxxFixup(X86_Z);
		jitcFlushRegisterDirty();
		asmALURegImm(X86_MOV, ECX, PPC_EXC_PROGRAM_PRIV);
		asmALURegImm(X86_MOV, EDX, gJITC.current_opc);
		asmALURegImm(X86_MOV, ESI, gJITC.pc);
		asmJMP((NativeAddress)ppc_program_exception_asm);
		asmResolveFixup(fixup, asmHERE());
		gJITC.checkedPriviledge = true;
	}
}

static inline void ppc_opc_gen_set_pc_rel(uint32 li)
{
	li += gJITC.pc;
	if (li < 4096) {
		/*
		 *	ESI is already set
		 */
		asmALURegImm(X86_MOV, EAX, li);
		asmCALL((NativeAddress)ppc_heartbeat_ext_rel_asm);
		jitcEmitAssure(10);
		/*
		 *	We assure here 10 bytes, to have enough space for 
		 *	next two instructions (since we want to modify them)
		 */
		asmMOVRegImm_NoFlags(EAX, li);
		asmCALL((NativeAddress)ppc_new_pc_this_page_asm);
	} else {
		asmALURegImm(X86_MOV, EAX, li);
		asmJMP((NativeAddress)ppc_new_pc_rel_asm);
	}
}

/*
 *	bx		Branch
 *	.435
 */
void ppc_opc_bx()
{
	uint32 li;
	PPC_OPC_TEMPL_I(gCPU.current_opc, li);
	if (!(gCPU.current_opc & PPC_OPC_AA)) {
		li += gCPU.pc;
	}
	if (gCPU.current_opc & PPC_OPC_LK) {
		gCPU.lr = gCPU.pc + 4;
	}
	gCPU.npc = li;
}

JITCFlow ppc_opc_gen_bx()
{
	uint32 li;
	PPC_OPC_TEMPL_I(gJITC.current_opc, li);
	jitcClobberAll();
	if (gJITC.current_opc & PPC_OPC_LK) {
		asmMOVRegDMem(EAX, (uint32)&gCPU.current_code_base);
		asmALURegImm(X86_ADD, EAX, gJITC.pc+4);
		asmMOVDMemReg((uint32)&gCPU.lr, EAX);
	}
	asmALURegImm(X86_MOV, ESI, gJITC.pc);
	if (gJITC.current_opc & PPC_OPC_AA) {
		asmALURegImm(X86_MOV, EAX, li);
		asmJMP((NativeAddress)ppc_new_pc_asm);
	} else {
		ppc_opc_gen_set_pc_rel(li);
	}
	return flowEndBlockUnreachable;
}

/*
 *	bcx		Branch Conditional
 *	.436
 */
void ppc_opc_bcx()
{
	uint32 BO, BI, BD;
	PPC_OPC_TEMPL_B(gCPU.current_opc, BO, BI, BD);
	if (!(BO & 4)) {
		gCPU.ctr--;
	}
	bool bo2 = (BO & 2);
	bool bo8 = (BO & 8); // branch condition true
	bool cr = (gCPU.cr & (1<<(31-BI)));
	if (((BO & 4) || ((gCPU.ctr!=0) ^ bo2))
	&& ((BO & 16) || (!(cr ^ bo8)))) {
		if (!(gCPU.current_opc & PPC_OPC_AA)) {
			BD += gCPU.pc;
		}
		if (gCPU.current_opc & PPC_OPC_LK) {
			gCPU.lr = gCPU.pc + 4;
		}
		gCPU.npc = BD;
	}
}
JITCFlow ppc_opc_gen_bcx()
{
	uint32 BO, BI, BD;
	PPC_OPC_TEMPL_B(gJITC.current_opc, BO, BI, BD);
	NativeAddress fixup = NULL;
	jitcFloatRegisterClobberAll();
	if (!(BO & 16)) {
		// only branch if condition
		if (BO & 4) {
			// don't check ctr
			PPC_CRx cr = (PPC_CRx)(BI / 4);
			if (jitcFlagsMapped() && jitcGetFlagsMapping() == cr && (BI%4) != 3) {
				// x86 flags map to correct crX register
				// and not SO flag (which isnt mapped)
				NativeAddress fixup2=NULL;
				switch (BI%4) {
				case 0:
					// less than
					fixup = asmJxxFixup((BO & 8) ? X86_NS : X86_S);
					break;
				case 1:
					// greater than
					// there seems to be no equivalent instruction on the x86
					if (BO & 8) {
						fixup = asmJxxFixup(X86_S);
						fixup2 = asmJxxFixup(X86_Z);
					} else {
						NativeAddress fixup3 = asmJxxFixup(X86_S);
						NativeAddress fixup4 = asmJxxFixup(X86_Z);
						fixup = asmJMPFixup();
						asmResolveFixup(fixup3, asmHERE());
						asmResolveFixup(fixup4, asmHERE());
					}
					break;
				case 2:
					// equal
					fixup = asmJxxFixup((BO & 8) ? X86_NZ : X86_Z);
					break;
				}
				// FIXME: optimize me
				if (jitcCarryMapped()) {
					asmCALL((NativeAddress)ppc_flush_carry_and_flags_asm);
				} else {
					asmCALL((NativeAddress)ppc_flush_flags_asm);
				}
				jitcFlushRegisterDirty();
				if (gJITC.current_opc & PPC_OPC_LK) {
					asmMOVRegDMem(EAX, (uint32)&gCPU.current_code_base);
					asmALURegImm(X86_ADD, EAX, gJITC.pc+4);
					asmMOVDMemReg((uint32)&gCPU.lr, EAX);
				}
				asmALURegImm(X86_MOV, ESI, gJITC.pc);
				if (gJITC.current_opc & PPC_OPC_AA) {
					asmALURegImm(X86_MOV, EAX, BD);
					asmJMP((NativeAddress)ppc_new_pc_asm);
				} else {
					ppc_opc_gen_set_pc_rel(BD);
				}
				asmResolveFixup(fixup, asmHERE());
				if (fixup2) {
					asmResolveFixup(fixup2, asmHERE());
				}
				return flowContinue;
			} else {
				jitcClobberCarryAndFlags();
				// test specific crX bit
				asmTESTDMemImm((uint32)&gCPU.cr, 1<<(31-BI));
				fixup = asmJxxFixup((BO & 8) ? X86_Z : X86_NZ);
			}
		} else {
			// decrement and check condition
			jitcClobberCarryAndFlags();
			NativeReg ctr = jitcGetClientRegisterDirty(PPC_CTR);
			asmDECReg(ctr);
			NativeAddress fixup = asmJxxFixup((BO & 2) ? X86_NZ : X86_Z);
			asmTESTDMemImm((uint32)(&gCPU.cr), 1<<(31-BI));
			NativeAddress fixup2 = asmJxxFixup((BO & 8) ? X86_Z : X86_NZ);
			jitcFlushRegisterDirty();
			if (gJITC.current_opc & PPC_OPC_LK) {
				asmMOVRegDMem(EAX, (uint32)&gCPU.current_code_base);
				asmALURegImm(X86_ADD, EAX, gJITC.pc+4);
				asmMOVDMemReg((uint32)&gCPU.lr, EAX);
			}
			asmALURegImm(X86_MOV, ESI, gJITC.pc);
			if (gJITC.current_opc & PPC_OPC_AA) {
				asmALURegImm(X86_MOV, EAX, BD);
				asmJMP((NativeAddress)ppc_new_pc_asm);
			} else {
				ppc_opc_gen_set_pc_rel(BD);
			}
			asmResolveFixup(fixup, asmHERE());
			asmResolveFixup(fixup2, asmHERE());
			return flowContinue;
		}
	} else {
		// don't check condition
		if (BO & 4) {
			// always branch
			jitcClobberCarryAndFlags();
			jitcFlushRegister();
			if (gJITC.current_opc & PPC_OPC_LK) {
				asmMOVRegDMem(EAX, (uint32)&gCPU.current_code_base);
				asmALURegImm(X86_ADD, EAX, gJITC.pc+4);
				asmMOVDMemReg((uint32)&gCPU.lr, EAX);
			}
			asmALURegImm(X86_MOV, ESI, gJITC.pc);
			if (gJITC.current_opc & PPC_OPC_AA) {
				asmALURegImm(X86_MOV, EAX, BD);
				asmJMP((NativeAddress)ppc_new_pc_asm);
		    	} else {
				ppc_opc_gen_set_pc_rel(BD);
			}
			return flowEndBlockUnreachable;
		} else {
			// decrement ctr and branch on ctr
			jitcClobberCarryAndFlags();
			NativeReg ctr = jitcGetClientRegisterDirty(PPC_CTR);
			asmDECReg(ctr);
			fixup = asmJxxFixup((BO & 2) ? X86_NZ : X86_Z);
		}
	}
	jitcFlushRegisterDirty();
	if (gJITC.current_opc & PPC_OPC_LK) {
		asmMOVRegDMem(EAX, (uint32)&gCPU.current_code_base);
		asmALURegImm(X86_ADD, EAX, gJITC.pc+4);
		asmMOVDMemReg((uint32)&gCPU.lr, EAX);
	}
	asmALURegImm(X86_MOV, ESI, gJITC.pc);
	if (gJITC.current_opc & PPC_OPC_AA) {
		asmALURegImm(X86_MOV, EAX, BD);
		asmJMP((NativeAddress)ppc_new_pc_asm);
	} else {
		ppc_opc_gen_set_pc_rel(BD);
	}
	asmResolveFixup(fixup, asmHERE());
	return flowContinue;
}

/*
 *	bcctrx		Branch Conditional to Count Register
 *	.438
 */
void ppc_opc_bcctrx()
{
	uint32 BO, BI, BD;
	PPC_OPC_TEMPL_XL(gCPU.current_opc, BO, BI, BD);
	PPC_OPC_ASSERT(BD==0);
	PPC_OPC_ASSERT(!(BO & 2));     
	bool bo8 = (BO & 8);
	bool cr = (gCPU.cr & (1<<(31-BI)));
	if ((BO & 16) || (!(cr ^ bo8))) {
		if (gCPU.current_opc & PPC_OPC_LK) {
			gCPU.lr = gCPU.pc + 4;
		}
		gCPU.npc = gCPU.ctr & 0xfffffffc;
	}
}
JITCFlow ppc_opc_gen_bcctrx()
{
	uint32 BO, BI, BD;
	PPC_OPC_TEMPL_XL(gJITC.current_opc, BO, BI, BD);
	jitcFloatRegisterClobberAll();
	if (BO & 16) {
		// branch always
		jitcClobberCarryAndFlags();
		jitcFlushRegister();
		jitcGetClientRegister(PPC_CTR, NATIVE_REG | EAX);
		if (gJITC.current_opc & PPC_OPC_LK) {
			asmMOVRegDMem(ECX, (uint32)&gCPU.current_code_base);
			asmALURegImm(X86_ADD, ECX, gJITC.pc+4);
			asmMOVDMemReg((uint32)&gCPU.lr, ECX);
		}
		asmALURegImm(X86_MOV, ESI, gJITC.pc);
		asmALURegImm(X86_AND, EAX, 0xfffffffc);
		asmJMP((NativeAddress)ppc_new_pc_asm);
		return flowEndBlockUnreachable;
	} else {
		// test specific crX bit
		jitcClobberCarryAndFlags();
		asmTESTDMemImm((uint32)(&gCPU.cr), 1<<(31-BI));
		jitcGetClientRegister(PPC_CTR, NATIVE_REG | EAX);
		NativeAddress fixup = asmJxxFixup((BO & 8) ? X86_Z : X86_NZ);
		jitcFlushRegisterDirty();
		if (gJITC.current_opc & PPC_OPC_LK) {
			asmMOVRegDMem(ECX, (uint32)&gCPU.current_code_base);
			asmALURegImm(X86_ADD, ECX, gJITC.pc+4);
			asmMOVDMemReg((uint32)&gCPU.lr, ECX);
		}
		asmALURegImm(X86_MOV, ESI, gJITC.pc);
		asmALURegImm(X86_AND, EAX, 0xfffffffc);
		asmJMP((NativeAddress)ppc_new_pc_asm);
		asmResolveFixup(fixup, asmHERE());	
		return flowContinue;
	}
}
/*
 *	bclrx		Branch Conditional to Link Register
 *	.440
 */
void ppc_opc_bclrx()
{
	uint32 BO, BI, BD;
	PPC_OPC_TEMPL_XL(gCPU.current_opc, BO, BI, BD);
	PPC_OPC_ASSERT(BD==0);
	if (!(BO & 4)) {
		gCPU.ctr--;
	}
	bool bo2 = (BO & 2);
	bool bo8 = (BO & 8);
	bool cr = (gCPU.cr & (1<<(31-BI)));
	if (((BO & 4) || ((gCPU.ctr!=0) ^ bo2))
	&& ((BO & 16) || (!(cr ^ bo8)))) {
		BD = gCPU.lr & 0xfffffffc;
		if (gCPU.current_opc & PPC_OPC_LK) {
			gCPU.lr = gCPU.pc + 4;
		}
		gCPU.npc = BD;
	}
}
JITCFlow ppc_opc_gen_bclrx()
{
	uint32 BO, BI, BD;
	PPC_OPC_TEMPL_XL(gJITC.current_opc, BO, BI, BD);
	if (!(BO & 4)) {
		PPC_OPC_ERR("not impl.: bclrx + BO&4\n");
	}
	jitcFloatRegisterClobberAll();
	if (BO & 16) {
		// branch always
		jitcClobberCarryAndFlags();
		jitcFlushRegister();
		jitcGetClientRegister(PPC_LR, NATIVE_REG | EAX);
		if (gJITC.current_opc & PPC_OPC_LK) {
			asmMOVRegDMem(ECX, (uint32)&gCPU.current_code_base);
			asmALURegImm(X86_ADD, ECX, gJITC.pc+4);
			asmMOVDMemReg((uint32)&gCPU.lr, ECX);
		}
		asmALURegImm(X86_MOV, ESI, gJITC.pc);
		asmALURegImm(X86_AND, EAX, 0xfffffffc);
		asmJMP((NativeAddress)ppc_new_pc_asm);
		return flowEndBlockUnreachable;
	} else {
		jitcClobberCarryAndFlags();
		// test specific crX bit
		asmTESTDMemImm((uint32)&gCPU.cr, 1<<(31-BI));
		jitcGetClientRegister(PPC_LR, NATIVE_REG | EAX);
		NativeAddress fixup = asmJxxFixup((BO & 8) ? X86_Z : X86_NZ);
		jitcFlushRegisterDirty();
		if (gJITC.current_opc & PPC_OPC_LK) {
			asmMOVRegDMem(ECX, (uint32)&gCPU.current_code_base);
			asmALURegImm(X86_ADD, ECX, gJITC.pc+4);
			asmMOVDMemReg((uint32)&gCPU.lr, ECX);
		}
		asmALURegImm(X86_MOV, ESI, gJITC.pc);
		asmALURegImm(X86_AND, EAX, 0xfffffffc);
		asmJMP((NativeAddress)ppc_new_pc_asm);
		asmResolveFixup(fixup, asmHERE());
		return flowContinue;
	}
}

/*
 *	dcbf		Data Cache Block Flush
 *	.458
 */
void ppc_opc_dcbf()
{
	// NO-OP
}
JITCFlow ppc_opc_gen_dcbf()
{
	// NO-OP
	return flowContinue;
}

/*
 *	dcbi		Data Cache Block Invalidate
 *	.460
 */
void ppc_opc_dcbi()
{
	if (gCPU.msr & MSR_PR) {
		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
		return;
	}
	// FIXME: check addr
}
JITCFlow ppc_opc_gen_dcbi()
{
	ppc_opc_gen_check_privilege();
	return flowContinue;
}
/*
 *	dcbst		Data Cache Block Store
 *	.461
 */
void ppc_opc_dcbst()
{
	// NO-OP
}
JITCFlow ppc_opc_gen_dcbst()
{
	// NO-OP
	return flowContinue;
}
/*
 *	dcbt		Data Cache Block Touch
 *	.462
 */
void ppc_opc_dcbt()
{
	// NO-OP
}
JITCFlow ppc_opc_gen_dcbt()
{
	// NO-OP
	return flowContinue;
}
/*
 *	dcbtst		Data Cache Block Touch for Store
 *	.463
 */
void ppc_opc_dcbtst()
{
	// NO-OP
}
JITCFlow ppc_opc_gen_dcbtst()
{
	// NO-OP
	return flowContinue;
}
/*
 *	eciwx		External Control In Word Indexed
 *	.474
 */
void ppc_opc_eciwx()
{
	PPC_OPC_ERR("eciwx unimplemented.\n");
}
JITCFlow ppc_opc_gen_eciwx()
{
	PPC_OPC_ERR("eciwx unimplemented.\n");
	return flowContinue;
}
/*
 *	ecowx		External Control Out Word Indexed
 *	.476
 */
void ppc_opc_ecowx()
{
	PPC_OPC_ERR("ecowx unimplemented.\n");
}
JITCFlow ppc_opc_gen_ecowx()
{
	PPC_OPC_ERR("ecowx unimplemented.\n");
	return flowContinue;
}
/*
 *	eieio		Enforce In-Order Execution of I/O
 *	.478
 */
void ppc_opc_eieio()
{
	// NO-OP
}
JITCFlow ppc_opc_gen_eieio()
{
	// NO-OP
	return flowContinue;
}

/*
 *	icbi		Instruction Cache Block Invalidate
 *	.519
 */
void ppc_opc_icbi()
{
	// FIXME: not a NOP with jitc
}
JITCFlow ppc_opc_gen_icbi()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rD, rA, rB);
	jitcClobberAll();
	if (rA) {
		byte modrm[6];
		asmMOVRegDMem(EAX, (uint32)&gCPU.gpr[rA]);
		asmALURegMem(X86_ADD, EAX, modrm, x86_mem(modrm, REG_NO, (uint32)&gCPU.gpr[rB]));
	} else {
		asmMOVRegDMem(EAX, (uint32)&gCPU.gpr[rB]);
	}
	asmCALL((NativeAddress)ppc_opc_icbi_asm);
	asmALURegImm(X86_MOV, EAX, gJITC.pc+4);
	asmALURegImm(X86_MOV, ESI, gJITC.pc);
	asmJMP((NativeAddress)ppc_new_pc_rel_asm);
	return flowEndBlockUnreachable;
}

/*
 *	isync		Instruction Synchronize
 *	.520
 */
void ppc_opc_isync()
{
	// NO-OP
}
JITCFlow ppc_opc_gen_isync()
{
	// NO-OP
	return flowContinue;
}

static uint32 ppc_cmp_and_mask[8] = {
	0xfffffff0,
	0xffffff0f,
	0xfffff0ff,
	0xffff0fff,
	0xfff0ffff,
	0xff0fffff,
	0xf0ffffff,
	0x0fffffff,
};
/*
 *	mcrf		Move Condition Register Field
 *	.561
 */
void ppc_opc_mcrf()
{
	uint32 crD, crS, bla;
	PPC_OPC_TEMPL_X(gCPU.current_opc, crD, crS, bla);
	// FIXME: bla == 0
	crD>>=2;
	crS>>=2;
	crD = 7-crD;
	crS = 7-crS;
	uint32 c = (gCPU.cr>>(crS*4)) & 0xf;
	gCPU.cr &= ppc_cmp_and_mask[crD];
	gCPU.cr |= c<<(crD*4);
}
JITCFlow ppc_opc_gen_mcrf()
{
	ppc_opc_gen_interpret(ppc_opc_mcrf);
	return flowEndBlock;
}
/*
 *	mcrfs		Move to Condition Register from FPSCR
 *	.562
 */
void ppc_opc_mcrfs()
{
	PPC_OPC_ERR("mcrfs unimplemented.\n");
}
JITCFlow ppc_opc_gen_mcrfs()
{
	PPC_OPC_ERR("mcrfs unimplemented.\n");
}
/*
 *	mcrxr		Move to Condition Register from XER
 *	.563
 */
void ppc_opc_mcrxr()
{
	int crD, a, b;
	PPC_OPC_TEMPL_X(gCPU.current_opc, crD, a, b);
	crD >>= 2;
	crD = 7-crD;
	gCPU.cr &= ppc_cmp_and_mask[crD];
	gCPU.cr |= (((gCPU.xer & 0xf0000000) | (gCPU.xer_ca ? XER_CA : 0))>>28)<<(crD*4);
	gCPU.xer = ~0xf0000000;
	gCPU.xer_ca = 0;
}
JITCFlow ppc_opc_gen_mcrxr()
{
	ppc_opc_gen_interpret(ppc_opc_mcrxr);
	return flowEndBlock;
}

static void inline move_reg(PPC_Register creg1, PPC_Register creg2)
{
	NativeReg reg2 = jitcGetClientRegister(creg2);
	NativeReg reg1 = jitcMapClientRegisterDirty(creg1);
	asmALURegReg(X86_MOV, reg1, reg2);
}

static void inline move_reg0(PPC_Register creg1)
{
	NativeReg reg1 = jitcMapClientRegisterDirty(creg1);
	asmMOVRegImm_NoFlags(reg1, 0);
}

/*
 *	mfcr		Move from Condition Register
 *	.564
 */
void ppc_opc_mfcr()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rD, rA, rB);
	PPC_OPC_ASSERT(rA==0 && rB==0);
	gCPU.gpr[rD] = gCPU.cr;
}
JITCFlow ppc_opc_gen_mfcr()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rD, rA, rB);
	jitcClobberFlags();
	NativeReg d = jitcMapClientRegisterDirty(PPC_GPR(rD));
	asmMOVRegDMem(d, (uint32)&gCPU.cr);
	return flowContinue;
}
/*
 *	mffs		Move from FPSCR
 *	.565
 */
void ppc_opc_mffsx()
{
	int frD, rA, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, frD, rA, rB);
	PPC_OPC_ASSERT(rA==0 && rB==0);
	gCPU.fpr[frD] = gCPU.fpscr;
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_OPC_ERR("mffs. unimplemented.\n");
	}
}
JITCFlow ppc_opc_gen_mffsx()
{
	ppc_opc_gen_interpret(ppc_opc_mffsx);
	return flowEndBlock;
}

/*
 *	mfmsr		Move from Machine State Register
 *	.566
 */
void ppc_opc_mfmsr()
{
	if (gCPU.msr & MSR_PR) {
		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
		return;
	}
	int rD, rA, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rD, rA, rB);
	PPC_OPC_ASSERT((rA == 0) && (rB == 0));
	gCPU.gpr[rD] = gCPU.msr;
}
JITCFlow ppc_opc_gen_mfmsr()
{
	ppc_opc_gen_check_privilege();
	int rD, rA, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rD, rA, rB);
	move_reg(PPC_GPR(rD), PPC_MSR);
	return flowContinue;
}

void FASTCALL blbl(uint32 a)
{
	PPC_OPC_ERR("invalid spr @%08x\n", a);
}

/*
 *	mfspr		Move from Special-Purpose Register
 *	.567
 */
void ppc_opc_mfspr()
{
	int rD, spr1, spr2;
	PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, spr1, spr2);
	switch (spr2) {
	case 0:
		switch (spr1) {
		case 1: gCPU.gpr[rD] = gCPU.xer | (gCPU.xer_ca ? XER_CA : 0); return;
		case 8: gCPU.gpr[rD] = gCPU.lr; return;
		case 9: gCPU.gpr[rD] = gCPU.ctr; return;
		}
	}
	if (gCPU.msr & MSR_PR) {
		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
		return;
	}
	switch (spr2) {
	case 0:
		switch (spr1) {
		case 18: gCPU.gpr[rD] = gCPU.dsisr; return;
		case 19: gCPU.gpr[rD] = gCPU.dar; return;
		case 22: gCPU.gpr[rD] = gCPU.dec; return;
		case 25: gCPU.gpr[rD] = gCPU.sdr1; return;
		case 26: gCPU.gpr[rD] = gCPU.srr[0]; return;
		case 27: gCPU.gpr[rD] = gCPU.srr[1]; return;
		}
		break;
	case 8:
		switch (spr1) {
		case 16: gCPU.gpr[rD] = gCPU.sprg[0]; return;
		case 17: gCPU.gpr[rD] = gCPU.sprg[1]; return;
		case 18: gCPU.gpr[rD] = gCPU.sprg[2]; return;
		case 19: gCPU.gpr[rD] = gCPU.sprg[3]; return;
		case 26: gCPU.gpr[rD] = gCPU.ear; return;
		case 31: gCPU.gpr[rD] = gCPU.pvr; return;
		}
		break;
	case 16:
		switch (spr1) {
		case 16: gCPU.gpr[rD] = gCPU.ibatu[0]; return;
		case 17: gCPU.gpr[rD] = gCPU.ibatl[0]; return;
		case 18: gCPU.gpr[rD] = gCPU.ibatu[1]; return;
		case 19: gCPU.gpr[rD] = gCPU.ibatl[1]; return;
		case 20: gCPU.gpr[rD] = gCPU.ibatu[2]; return;
		case 21: gCPU.gpr[rD] = gCPU.ibatl[2]; return;
		case 22: gCPU.gpr[rD] = gCPU.ibatu[3]; return;
		case 23: gCPU.gpr[rD] = gCPU.ibatl[3]; return;
		case 24: gCPU.gpr[rD] = gCPU.dbatu[0]; return;
		case 25: gCPU.gpr[rD] = gCPU.dbatl[0]; return;
		case 26: gCPU.gpr[rD] = gCPU.dbatu[1]; return;
		case 27: gCPU.gpr[rD] = gCPU.dbatl[1]; return;
		case 28: gCPU.gpr[rD] = gCPU.dbatu[2]; return;
		case 29: gCPU.gpr[rD] = gCPU.dbatl[2]; return;
		case 30: gCPU.gpr[rD] = gCPU.dbatu[3]; return;
		case 31: gCPU.gpr[rD] = gCPU.dbatl[3]; return;
		}
		break;
	case 31:
		switch (spr1) {
		case 16:
//			PPC_OPC_WARN("read from spr %d:%d (HID0) not supported!\n", spr1, spr2);
			gCPU.gpr[rD] = gCPU.hid[0];
			return;
		case 17:
			PPC_OPC_WARN("read from spr %d:%d (HID1) not supported!\n", spr1, spr2);
			gCPU.gpr[rD] = gCPU.hid[1];
			return;
		case 25:
			PPC_OPC_WARN("read from spr %d:%d (L2CR) not supported! (from %08x)\n", spr1, spr2, gCPU.pc);
			gCPU.gpr[rD] = 0;
			return;
		case 27:
			PPC_OPC_WARN("read from spr %d:%d (ICTC) not supported!\n", spr1, spr2);
			gCPU.gpr[rD] = 0;
			return;
		case 28:
//			PPC_OPC_WARN("read from spr %d:%d (THRM1) not supported!\n", spr1, spr2);
			gCPU.gpr[rD] = 0;
			return;
		case 29:
//			PPC_OPC_WARN("read from spr %d:%d (THRM2) not supported!\n", spr1, spr2);
			gCPU.gpr[rD] = 0;
			return;
		case 30:
//			PPC_OPC_WARN("read from spr %d:%d (THRM3) not supported!\n", spr1, spr2);
			gCPU.gpr[rD] = 0;
			return;
		}
	}
	SINGLESTEP("invalid mfspr\n");
}

JITCFlow ppc_opc_gen_mfspr()
{
	int rD, spr1, spr2;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rD, spr1, spr2);
	switch (spr2) {
	case 0:
		switch (spr1) {
		case 1: {
			jitcClobberFlags();
			jitcGetClientCarry();
			NativeReg reg2 = jitcGetClientRegister(PPC_XER);
			NativeReg reg1 = jitcMapClientRegisterDirty(PPC_GPR(rD));
			asmALURegReg(X86_SBB, reg1, reg1);   // reg1 = CA ? -1 : 0
			asmALURegImm(X86_AND, reg1, XER_CA); // reg1 = CA ? XER_CA : 0
			asmALURegReg(X86_OR, reg1, reg2);
			jitcClobberCarry();
			return flowContinue;
		}
		case 8: move_reg(PPC_GPR(rD), PPC_LR); return flowContinue;
		case 9: move_reg(PPC_GPR(rD), PPC_CTR); return flowContinue;
		}
	}
	ppc_opc_gen_check_privilege();
	switch (spr2) {
	case 0:
		switch (spr1) {
		case 18: move_reg(PPC_GPR(rD), PPC_DSISR); return flowContinue;
		case 19: move_reg(PPC_GPR(rD), PPC_DAR); return flowContinue;
		case 22: {
			jitcClobberAll();
			asmCALL((NativeAddress)readDEC);
			move_reg(PPC_GPR(rD), PPC_DEC);
			return flowContinue;
		}
		case 25: move_reg(PPC_GPR(rD), PPC_SDR1); return flowContinue;
		case 26: move_reg(PPC_GPR(rD), PPC_SRR0); return flowContinue;
		case 27: move_reg(PPC_GPR(rD), PPC_SRR1); return flowContinue;
		}
		break;
	case 8:
		switch (spr1) {
		case 16: move_reg(PPC_GPR(rD), PPC_SPRG(0)); return flowContinue;
		case 17: move_reg(PPC_GPR(rD), PPC_SPRG(1)); return flowContinue;
		case 18: move_reg(PPC_GPR(rD), PPC_SPRG(2)); return flowContinue;
		case 19: move_reg(PPC_GPR(rD), PPC_SPRG(3)); return flowContinue;
		case 26: move_reg(PPC_GPR(rD), PPC_EAR); return flowContinue;
		case 31: move_reg(PPC_GPR(rD), PPC_PVR); return flowContinue;
		}
		break;
	case 16:
		switch (spr1) {
		case 16: move_reg(PPC_GPR(rD), PPC_IBATU(0)); return flowContinue;
		case 17: move_reg(PPC_GPR(rD), PPC_IBATL(0)); return flowContinue;
		case 18: move_reg(PPC_GPR(rD), PPC_IBATU(1)); return flowContinue;
		case 19: move_reg(PPC_GPR(rD), PPC_IBATL(1)); return flowContinue;
		case 20: move_reg(PPC_GPR(rD), PPC_IBATU(2)); return flowContinue;
		case 21: move_reg(PPC_GPR(rD), PPC_IBATL(2)); return flowContinue;
		case 22: move_reg(PPC_GPR(rD), PPC_IBATU(3)); return flowContinue;
		case 23: move_reg(PPC_GPR(rD), PPC_IBATL(3)); return flowContinue;
		case 24: move_reg(PPC_GPR(rD), PPC_DBATU(0)); return flowContinue;
		case 25: move_reg(PPC_GPR(rD), PPC_DBATL(0)); return flowContinue;
		case 26: move_reg(PPC_GPR(rD), PPC_DBATU(1)); return flowContinue;
		case 27: move_reg(PPC_GPR(rD), PPC_DBATL(1)); return flowContinue;
		case 28: move_reg(PPC_GPR(rD), PPC_DBATU(2)); return flowContinue;
		case 29: move_reg(PPC_GPR(rD), PPC_DBATL(2)); return flowContinue;
		case 30: move_reg(PPC_GPR(rD), PPC_DBATU(3)); return flowContinue;
		case 31: move_reg(PPC_GPR(rD), PPC_DBATL(3)); return flowContinue;
		}
		break;
	case 31:
		switch (spr1) {
		case 16: move_reg(PPC_GPR(rD), PPC_HID0); return flowContinue;
		case 17: move_reg(PPC_GPR(rD), PPC_HID1); return flowContinue;
		case 25: move_reg0(PPC_GPR(rD)); return flowContinue;
		case 27: move_reg0(PPC_GPR(rD)); return flowContinue;
		case 28: move_reg0(PPC_GPR(rD)); return flowContinue;
		case 29: move_reg0(PPC_GPR(rD)); return flowContinue;
		case 30: move_reg0(PPC_GPR(rD)); return flowContinue;
		}
	}
	asmMOVRegDMem(EAX, (uint32)&gCPU.current_code_base);
	asmJMP((NativeAddress)blbl);
	return flowContinue;
	PPC_OPC_ERR("unknown spr %d:%d\n", spr1, spr2);
	return flowEndBlockUnreachable;
}
/*
 *	mfsr		Move from Segment Register
 *	.570
 */
void ppc_opc_mfsr()
{
	if (gCPU.msr & MSR_PR) {
		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
		return;
	}
	int rD, SR, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rD, SR, rB);
	// FIXME: check insn
	gCPU.gpr[rD] = gCPU.sr[SR & 0xf];
}
JITCFlow ppc_opc_gen_mfsr()
{
	ppc_opc_gen_check_privilege();
	int rD, SR, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rD, SR, rB);
	move_reg(PPC_GPR(rD), PPC_SR(SR & 0xf));	
	return flowContinue;
}
/*
 *	mfsrin		Move from Segment Register Indirect
 *	.572
 */
void ppc_opc_mfsrin()
{
	if (gCPU.msr & MSR_PR) {
		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
		return;
	}
	int rD, rA, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rD, rA, rB);
	// FIXME: check insn
	gCPU.gpr[rD] = gCPU.sr[gCPU.gpr[rB] >> 28];
}
JITCFlow ppc_opc_gen_mfsrin()
{
	ppc_opc_gen_check_privilege();
	int rD, SR, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rD, SR, rB);
	jitcClobberCarryAndFlags();
	NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
	NativeReg d = jitcMapClientRegisterDirty(PPC_GPR(rD));
	if (b != d) jitcClobberRegister(NATIVE_REG | b);
	// no problem here if b==d 
	asmShiftRegImm(X86_SHR, b, 28);
	// mov d, [4*b+sr]
	byte modrm[6];
	asmALURegMem(X86_MOV, d, modrm, x86_mem_sib(modrm, REG_NO, 4, b, (uint32)(&gCPU.sr[0])));
	return flowContinue;
}
/*
 *	mftb		Move from Time Base
 *	.574
 */
void ppc_opc_mftb()
{
	int rD, spr1, spr2;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rD, spr1, spr2);
	switch (spr2) {
	case 8:
		switch (spr1) {
		case 12: gCPU.gpr[rD] = ppc_get_cpu_timebase(); return;
		case 13: gCPU.gpr[rD] = ppc_get_cpu_timebase() >> 32; return;
/*		case 12: gCPU.gpr[rD] = gCPU.tb; return;
		case 13: gCPU.gpr[rD] = gCPU.tb >> 32; return;*/
		}
		break;
	}
	SINGLESTEP("unknown mftb\n");
}
JITCFlow ppc_opc_gen_mftb()
{
	ppc_opc_gen_interpret(ppc_opc_mftb);
	return flowEndBlock;
}
/*
 *	mtcrf		Move to Condition Register Fields
 *	.576
 */
void ppc_opc_mtcrf()
{
	int rS;
	uint32 crm;
	uint32 CRM;
	PPC_OPC_TEMPL_XFX(gCPU.current_opc, rS, crm);
	CRM = ((crm&0x80)?0xf0000000:0)|((crm&0x40)?0x0f000000:0)|((crm&0x20)?0x00f00000:0)|((crm&0x10)?0x000f0000:0)|
	      ((crm&0x08)?0x0000f000:0)|((crm&0x04)?0x00000f00:0)|((crm&0x02)?0x000000f0:0)|((crm&0x01)?0x0000000f:0);
	gCPU.cr = (gCPU.gpr[rS] & CRM) | (gCPU.cr & ~CRM);
}
JITCFlow ppc_opc_gen_mtcrf()
{
	int rS;
	uint32 crm;
	uint32 CRM;
	PPC_OPC_TEMPL_XFX(gJITC.current_opc, rS, crm);
	CRM = ((crm&0x80)?0xf0000000:0)|((crm&0x40)?0x0f000000:0)|((crm&0x20)?0x00f00000:0)|((crm&0x10)?0x000f0000:0)|
	      ((crm&0x08)?0x0000f000:0)|((crm&0x04)?0x00000f00:0)|((crm&0x02)?0x000000f0:0)|((crm&0x01)?0x0000000f:0);
	jitcClobberCarryAndFlags();
	NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
	jitcClobberRegister(NATIVE_REG | s);
	byte modrm[6];
	asmALURegImm(X86_AND, s, CRM);
	asmALUMemImm(X86_AND, modrm, x86_mem(modrm, REG_NO, (uint32)&gCPU.cr), ~CRM);
	asmALUMemReg(X86_OR, modrm, x86_mem(modrm, REG_NO, (uint32)&gCPU.cr), s);
	return flowContinue;
}
/*
 *	mtfsb0x		Move to FPSCR Bit 0
 *	.577
 */
void ppc_opc_mtfsb0x()
{
	int crbD, n1, n2;
	PPC_OPC_TEMPL_X(gCPU.current_opc, crbD, n1, n2);
	if (crbD != 1 && crbD != 2) {
		gCPU.fpscr &= ~(1<<(31-crbD));
	}
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_OPC_ERR("mtfsb0. unimplemented.\n");
	}
}

static uint32 ppc_to_x86_roundmode[] = {
	0x0000, // round to nearest
	0x0c00, // round to zero
	0x0800, // round to pinf
	0x0400, // round to minf
};

static void ppc_opc_set_fpscr_roundmode(NativeReg r)
{
	byte modrm[6];
	asmALURegImm(X86_AND, r, 3); // RC
	asmALUMemImm(X86_AND, modrm, x86_mem(modrm, REG_NO, (uint32)&gCPU.x87cw), ~0x0c00);
	asmALURegMem(X86_MOV, r, modrm, x86_mem_sib(modrm, REG_NO, 4, r, (uint32)&ppc_to_x86_roundmode));
	asmALUMemReg(X86_OR, modrm, x86_mem(modrm, REG_NO, (uint32)&gCPU.x87cw), r);
	asmFLDCWMem(modrm, x86_mem(modrm, REG_NO, (uint32)&gCPU.x87cw));
}

JITCFlow ppc_opc_gen_mtfsb0x()
{
	int crbD, n1, n2;
	PPC_OPC_TEMPL_X(gJITC.current_opc, crbD, n1, n2);
	if (crbD != 1 && crbD != 2) {
		jitcGetClientRegister(PPC_FPSCR, NATIVE_REG | EAX);
		jitcClobberAll();
		asmALURegImm(X86_AND, EAX, ~(1<<(31-crbD)));
		asmMOVDMemReg((uint32)&gCPU.fpscr, EAX);
		if (crbD == 30 || crbD == 31) {
			ppc_opc_set_fpscr_roundmode(EAX);
		}
	}
	return flowContinue;
}
/*
 *	mtfsb1x		Move to FPSCR Bit 1
 *	.578
 */
void ppc_opc_mtfsb1x()
{
	int crbD, n1, n2;
	PPC_OPC_TEMPL_X(gCPU.current_opc, crbD, n1, n2);
	if (crbD != 1 && crbD != 2) {
		gCPU.fpscr |= 1<<(31-crbD);
	}
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_OPC_ERR("mtfsb1. unimplemented.\n");
	}
}
/*JITCFlow ppc_opc_gen_mtfsb1x()
{
	ppc_opc_gen_interpret(ppc_opc_mtfsb1x);
	return flowEndBlock;
}*/
JITCFlow ppc_opc_gen_mtfsb1x()
{
	int crbD, n1, n2;
	PPC_OPC_TEMPL_X(gJITC.current_opc, crbD, n1, n2);
	if (crbD != 1 && crbD != 2) {
		jitcGetClientRegister(PPC_FPSCR, NATIVE_REG | EAX);
		jitcClobberAll();
		asmALURegImm(X86_OR, EAX, 1<<(31-crbD));
		asmMOVDMemReg((uint32)&gCPU.fpscr, EAX);
		if (crbD == 30 || crbD == 31) {
			ppc_opc_set_fpscr_roundmode(EAX);
		}
	}
	return flowContinue;
}
/*
 *	mtfsfx		Move to FPSCR Fields
 *	.579
 */
void ppc_opc_mtfsfx()
{
	int frB;
	uint32 fm, FM;
	PPC_OPC_TEMPL_XFL(gCPU.current_opc, frB, fm);
	FM = ((fm&0x80)?0xf0000000:0)|((fm&0x40)?0x0f000000:0)|((fm&0x20)?0x00f00000:0)|((fm&0x10)?0x000f0000:0)|
	     ((fm&0x08)?0x0000f000:0)|((fm&0x04)?0x00000f00:0)|((fm&0x02)?0x000000f0:0)|((fm&0x01)?0x0000000f:0);
	gCPU.fpscr = (gCPU.fpr[frB] & FM) | (gCPU.fpscr & ~FM);
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_OPC_ERR("mtfsf. unimplemented.\n");
	}
}
/*JITCFlow ppc_opc_gen_mtfsfx()
{
	ppc_opc_gen_interpret(ppc_opc_mtfsfx);
	return flowEndBlock;
}*/
JITCFlow ppc_opc_gen_mtfsfx()
{
	int frB;
	uint32 fm, FM;
	PPC_OPC_TEMPL_XFL(gJITC.current_opc, frB, fm);
	FM = ((fm&0x80)?0xf0000000:0)|((fm&0x40)?0x0f000000:0)|((fm&0x20)?0x00f00000:0)|((fm&0x10)?0x000f0000:0)|
	     ((fm&0x08)?0x0000f000:0)|((fm&0x04)?0x00000f00:0)|((fm&0x02)?0x000000f0:0)|((fm&0x01)?0x0000000f:0);
	     
	NativeReg fpscr = jitcGetClientRegister(PPC_FPSCR);
	NativeReg b = jitcGetClientRegister(PPC_FPR_L(frB));
	jitcClobberAll();
	asmALURegImm(X86_AND, b, FM);
	asmALURegImm(X86_AND, fpscr, ~FM);
	asmALURegReg(X86_OR, fpscr, b);
	if (fm & 1) {
		asmMOVDMemReg((uint32)&gCPU.fpscr, fpscr);
		ppc_opc_set_fpscr_roundmode(fpscr);
	} else {
		jitcMapClientRegisterDirty(PPC_FPSCR, NATIVE_REG | fpscr);
	}
	if (gJITC.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_OPC_ERR("mtfsf. unimplemented.\n");
	}
	return flowContinue;
}
/*
 *	mtfsfix		Move to FPSCR Field Immediate
 *	.580
 */
void ppc_opc_mtfsfix()
{
	int crfD, n1;
	uint32 imm;
	PPC_OPC_TEMPL_X(gCPU.current_opc, crfD, n1, imm);
	crfD >>= 2;
	imm >>= 1;
	crfD = 7-crfD;
	gCPU.fpscr &= ppc_cmp_and_mask[crfD];
	gCPU.fpscr |= imm<<(crfD*4);
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_OPC_ERR("mtfsfi. unimplemented.\n");
	}
}
/*JITCFlow ppc_opc_gen_mtfsfix()
{
	ppc_opc_gen_interpret(ppc_opc_mtfsfix);
	return flowEndBlock;
}*/
JITCFlow ppc_opc_gen_mtfsfix()
{
	int crfD, n1;
	uint32 imm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, crfD, n1, imm);
	crfD >>= 2;
	imm >>= 1;
	crfD = 7-crfD;
	NativeReg fpscr = jitcGetClientRegister(PPC_FPSCR);
	jitcClobberAll();
	asmALURegImm(X86_AND, fpscr, ppc_cmp_and_mask[crfD]);
	asmALURegImm(X86_OR, fpscr, imm<<(crfD*4));
	if (crfD == 0) {
		asmMOVDMemReg((uint32)&gCPU.fpscr, fpscr);
		ppc_opc_set_fpscr_roundmode(fpscr);
	} else {
		jitcMapClientRegisterDirty(PPC_FPSCR, NATIVE_REG | fpscr);
	}
	if (gJITC.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_OPC_ERR("mtfsfi. unimplemented.\n");
	}
	return flowContinue;
}
/*
 *	mtmsr		Move to Machine State Register
 *	.581
 */
void ppc_opc_mtmsr()
{
	if (gCPU.msr & MSR_PR) {
		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
		return;
	}
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, rB);
	PPC_OPC_ASSERT((rA == 0) && (rB == 0));
	ppc_set_msr(gCPU.gpr[rS]);
}
JITCFlow ppc_opc_gen_mtmsr()
{
	ppc_opc_gen_check_privilege();
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, rA, rB);
	jitcClobberCarryAndFlags();
	jitcFlushRegister();
	jitcGetClientRegister(PPC_GPR(rS), NATIVE_REG | EAX);
	asmCALL((NativeAddress)ppc_set_msr_asm);
	asmALURegImm(X86_MOV, EAX, gJITC.pc+4);
	asmALURegImm(X86_MOV, ESI, gJITC.pc);
	asmJMP((NativeAddress)ppc_new_pc_rel_asm);
	return flowEndBlockUnreachable;
}
/*
 *	mtspr		Move to Special-Purpose Register
 *	.584
 */
void ppc_opc_mtspr()
{
	int rS, spr1, spr2;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, spr1, spr2);
	switch (spr2) {
	case 0:
		switch (spr1) {
		case 1:
			gCPU.xer = gCPU.gpr[rS] & ~XER_CA;
			gCPU.xer_ca = !!(gCPU.gpr[rS] & XER_CA);
			return;
		case 8:	gCPU.lr = gCPU.gpr[rS]; return;
		case 9:	gCPU.ctr = gCPU.gpr[rS]; return;
		}
	}
	if (gCPU.msr & MSR_PR) {
		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
		return;
	}
	switch (spr2) {
	case 0:
		switch (spr1) {
/*		case 18: gCPU.gpr[rD] = gCPU.dsisr; return;
		case 19: gCPU.gpr[rD] = gCPU.dar; return;*/
		case 22: {
			gCPU.dec = gCPU.gpr[rS];
			return;
		}
		case 25: 
			if (!ppc_mmu_set_sdr1(gCPU.gpr[rS], true)) {
				PPC_OPC_ERR("cannot set sdr1\n");
			}
			return;
		case 26: gCPU.srr[0] = gCPU.gpr[rS]; return;
		case 27: gCPU.srr[1] = gCPU.gpr[rS]; return;
		}
		break;
	case 8:
		switch (spr1) {
		case 16: gCPU.sprg[0] = gCPU.gpr[rS]; return;
		case 17: gCPU.sprg[1] = gCPU.gpr[rS]; return;
		case 18: gCPU.sprg[2] = gCPU.gpr[rS]; return;
		case 19: gCPU.sprg[3] = gCPU.gpr[rS]; return;
/*		case 26: gCPU.gpr[rD] = gCPU.ear; return;
		case 28: TB (lower)
		case 29: TB (upper)
		case 31: gCPU.gpr[rD] = gCPU.pvr; return;*/
		}
		break;
	case 16:
		switch (spr1) {
		case 16:
			gCPU.ibatu[0] = gCPU.gpr[rS];
			gCPU.ibat_bl17[0] = ~(BATU_BL(gCPU.ibatu[0])<<17);
			return;
		case 17:
			gCPU.ibatl[0] = gCPU.gpr[rS];
			return;
		case 18:
			gCPU.ibatu[1] = gCPU.gpr[rS];
			gCPU.ibat_bl17[1] = ~(BATU_BL(gCPU.ibatu[1])<<17);
			return;
		case 19:
			gCPU.ibatl[1] = gCPU.gpr[rS];
			return;
		case 20:
			gCPU.ibatu[2] = gCPU.gpr[rS];
			gCPU.ibat_bl17[2] = ~(BATU_BL(gCPU.ibatu[2])<<17);
			return;
		case 21:
			gCPU.ibatl[2] = gCPU.gpr[rS];
			return;
		case 22:
			gCPU.ibatu[3] = gCPU.gpr[rS];
			gCPU.ibat_bl17[3] = ~(BATU_BL(gCPU.ibatu[3])<<17);
			return;
		case 23:
			gCPU.ibatl[3] = gCPU.gpr[rS];
			return;
		case 24:
			gCPU.dbatu[0] = gCPU.gpr[rS];
			gCPU.dbat_bl17[0] = ~(BATU_BL(gCPU.dbatu[0])<<17);
			return;
		case 25:
			gCPU.dbatl[0] = gCPU.gpr[rS];
			return;
		case 26:
			gCPU.dbatu[1] = gCPU.gpr[rS];
			gCPU.dbat_bl17[1] = ~(BATU_BL(gCPU.dbatu[1])<<17);
			return;
		case 27:
			gCPU.dbatl[1] = gCPU.gpr[rS];
			return;
		case 28:
			gCPU.dbatu[2] = gCPU.gpr[rS];
			gCPU.dbat_bl17[2] = ~(BATU_BL(gCPU.dbatu[2])<<17);
			return;
		case 29:
			gCPU.dbatl[2] = gCPU.gpr[rS];
			return;
		case 30:
			gCPU.dbatu[3] = gCPU.gpr[rS];
			gCPU.dbat_bl17[3] = ~(BATU_BL(gCPU.dbatu[3])<<17);
			return;
		case 31:
			gCPU.dbatl[3] = gCPU.gpr[rS];
			return;
		}
		break;
	case 31:
		switch (spr1) {
		case 16:
//			PPC_OPC_WARN("write(%08x) to spr %d:%d (HID0) not supported! @%08x\n", gCPU.gpr[rS], spr1, spr2, gCPU.pc);
			gCPU.hid[0] = gCPU.gpr[rS];
			return;
		case 18:
			PPC_OPC_ERR("write(%08x) to spr %d:%d (IABR) not supported!\n", gCPU.gpr[rS], spr1, spr2);
			return;
		case 21:
			PPC_OPC_ERR("write(%08x) to spr %d:%d (DABR) not supported!\n", gCPU.gpr[rS], spr1, spr2);
			return;
		case 27:
			PPC_OPC_WARN("write(%08x) to spr %d:%d (ICTC) not supported!\n", gCPU.gpr[rS], spr1, spr2);
			return;
		case 28:
//			PPC_OPC_WARN("write(%08x) to spr %d:%d (THRM1) not supported!\n", gCPU.gpr[rS], spr1, spr2);
			return;
		case 29:
//			PPC_OPC_WARN("write(%08x) to spr %d:%d (THRM2) not supported!\n", gCPU.gpr[rS], spr1, spr2);
			return;
		case 30:
//			PPC_OPC_WARN("write(%08x) to spr %d:%d (THRM3) not supported!\n", gCPU.gpr[rS], spr1, spr2);
			return;
		}
	}
	SINGLESTEP("unknown mtspr\n");
}

static void FASTCALL ppc_mmu_set_sdr1_check_error(uint32 newsdr1)
{
	if (!ppc_mmu_set_sdr1(newsdr1, true)) {
		PPC_OPC_ERR("cannot set sdr1\n");
	}
}

void ppc_opc_gen_bl17(int ibatdbat, int idx)
{
	NativeReg reg = jitcGetClientRegister(ibatdbat ? PPC_DBATU(idx) : PPC_IBATU(idx));
	jitcClobberCarryAndFlags();
	jitcClobberRegister(NATIVE_REG | reg);
	asmALURegImm(X86_AND, reg, 0x1ffc);
	asmShiftRegImm(X86_SHL, reg, 15);
	asmALUReg(X86_NOT, reg);
	asmMOVDMemReg(ibatdbat ? (uint32)&gCPU.dbat_bl17[idx] : (uint32)&gCPU.ibat_bl17[idx], reg);
	// gCPU.dbat_bl17[3] = ~(BATU_BL(gCPU.dbatu[3])<<17);
}

JITCFlow ppc_opc_gen_mtspr()
{
	int rS, spr1, spr2;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, spr1, spr2);
	switch (spr2) {
	case 0:
		switch (spr1) {
		case 1: {
			jitcClobberFlags();
			NativeReg reg2 = jitcGetClientRegister(PPC_GPR(rS));
			NativeReg reg1 = jitcMapClientRegisterDirty(PPC_XER);
			asmALURegReg(X86_MOV, reg1, reg2);
			asmALURegImm(X86_AND, reg1, ~XER_CA);
			asmBTxRegImm(X86_BT, reg2, 29);
			jitcMapCarryDirty();
			return flowContinue;
		}
		case 8:	move_reg(PPC_LR, PPC_GPR(rS)); return flowContinue;
		case 9:	move_reg(PPC_CTR, PPC_GPR(rS)); return flowContinue;
		}
	}
	ppc_opc_gen_check_privilege();
	switch (spr2) {
	case 0:
		switch (spr1) {
		case 22: {
			byte modrm[6];
			asmALUMemImm(X86_MOV, modrm, x86_mem(modrm, REG_NO, (uint32)&gCPU.start_pc_ofs), gJITC.pc);
			move_reg(PPC_DEC, PPC_GPR(rS));
			jitcClobberAll();
			asmCALL((NativeAddress)writeDEC);
			return flowContinue;
		}
		case 25: {
			jitcGetClientRegister(PPC_GPR(rS), NATIVE_REG | EAX);
			jitcClobberAll();
			asmCALL((NativeAddress)ppc_mmu_set_sdr1_check_error);
			asmALURegImm(X86_MOV, EAX, gJITC.pc+4);
			asmALURegImm(X86_MOV, ESI, gJITC.pc);
			asmJMP((NativeAddress)ppc_new_pc_rel_asm);
			return flowEndBlockUnreachable;
		}
		case 26: move_reg(PPC_SRR0, PPC_GPR(rS)); return flowContinue;
		case 27: move_reg(PPC_SRR1, PPC_GPR(rS)); return flowContinue;
		}
		break;
	case 8:
		switch (spr1) {
		case 16: move_reg(PPC_SPRG(0), PPC_GPR(rS)); return flowContinue;
		case 17: move_reg(PPC_SPRG(1), PPC_GPR(rS)); return flowContinue;
		case 18: move_reg(PPC_SPRG(2), PPC_GPR(rS)); return flowContinue;
		case 19: move_reg(PPC_SPRG(3), PPC_GPR(rS)); return flowContinue;
		}
		break;
	case 16: {
		switch (spr1) {
		case 16:
			move_reg(PPC_IBATU(0), PPC_GPR(rS));
			ppc_opc_gen_bl17(0, 0);
			break;
		case 17:
			move_reg(PPC_IBATL(0), PPC_GPR(rS));
			break;
		case 18:
			move_reg(PPC_IBATU(1), PPC_GPR(rS));
			ppc_opc_gen_bl17(0, 1);
			break;
		case 19:
			move_reg(PPC_IBATL(1), PPC_GPR(rS));
			break;
		case 20:
			move_reg(PPC_IBATU(2), PPC_GPR(rS));
			ppc_opc_gen_bl17(0, 2);
			break;
		case 21:
			move_reg(PPC_IBATL(2), PPC_GPR(rS));
			break;
		case 22:
			move_reg(PPC_IBATU(3), PPC_GPR(rS));
			ppc_opc_gen_bl17(0, 3);
			break;
		case 23:
			move_reg(PPC_IBATL(3), PPC_GPR(rS));
			break;
		case 24:
			move_reg(PPC_DBATU(0), PPC_GPR(rS));
			ppc_opc_gen_bl17(1, 0);
			break;
		case 25:
			move_reg(PPC_DBATL(0), PPC_GPR(rS));
			break;
		case 26:
			move_reg(PPC_DBATU(1), PPC_GPR(rS));
			ppc_opc_gen_bl17(1, 1);
			break;
		case 27:
			move_reg(PPC_DBATL(1), PPC_GPR(rS));
			break;
		case 28:
			move_reg(PPC_DBATU(2), PPC_GPR(rS));
			ppc_opc_gen_bl17(1, 2);
			break;
		case 29:
			move_reg(PPC_DBATL(2), PPC_GPR(rS));
			break;
		case 30:
			move_reg(PPC_DBATU(3), PPC_GPR(rS));
			ppc_opc_gen_bl17(1, 3);
			break;
		case 31:
			move_reg(PPC_DBATL(3), PPC_GPR(rS));
			break;
		default: goto invalid;
		}
		jitcClobberAll();
		asmCALL((NativeAddress)ppc_mmu_tlb_invalidate_all_asm);
		asmALURegImm(X86_MOV, EAX, gJITC.pc+4);
		asmALURegImm(X86_MOV, ESI, gJITC.pc);
		asmJMP((NativeAddress)ppc_new_pc_rel_asm);
		return flowEndBlockUnreachable;
	}
	case 31:
		switch (spr1) {
		case 16: move_reg(PPC_HID0, PPC_GPR(rS)); return flowContinue;
		case 18: return flowContinue;
		case 21: return flowContinue;
		case 27: return flowContinue;
		case 28: return flowContinue;
		case 29: return flowContinue;
		case 30: return flowContinue;
		}
	}
	invalid:
	asmMOVRegDMem(EAX, (uint32)&gCPU.current_code_base);
	asmJMP((NativeAddress)blbl);
	return flowContinue;
	PPC_OPC_ERR("unknown spr %d:%d\n", spr1, spr2);
	return flowEndBlockUnreachable;
}
/*
 *	mtsr		Move to Segment Register
 *	.587
 */
void ppc_opc_mtsr()
{
	if (gCPU.msr & MSR_PR) {
		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
		return;
	}
	int rS, SR, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, SR, rB);
	// FIXME: check insn
	gCPU.sr[SR & 0xf] = gCPU.gpr[rS];
}
JITCFlow ppc_opc_gen_mtsr()
{
	ppc_opc_gen_check_privilege();
	int rS, SR, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, SR, rB);
	// FIXME: check insn
	move_reg(PPC_SR(SR & 0xf), PPC_GPR(rS));
	jitcClobberAll();
	asmCALL((NativeAddress)ppc_mmu_tlb_invalidate_all_asm);
	// sync
	asmALURegImm(X86_MOV, EAX, gJITC.pc+4);
	asmALURegImm(X86_MOV, ESI, gJITC.pc);
	asmJMP((NativeAddress)ppc_new_pc_rel_asm);
	return flowEndBlockUnreachable;	
}
/*
 *	mtsrin		Move to Segment Register Indirect
 *	.591
 */
void ppc_opc_mtsrin()
{
	if (gCPU.msr & MSR_PR) {
		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
		return;
	}
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, rB);
	// FIXME: check insn
	gCPU.sr[gCPU.gpr[rB] >> 28] = gCPU.gpr[rS];
}
JITCFlow ppc_opc_gen_mtsrin()
{
	ppc_opc_gen_check_privilege();
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, rA, rB);
	// FIXME: check insn
	NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
	NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
	if (b == s) {
		s = jitcAllocRegister();
		asmALURegReg(X86_MOV, s, b);
	}
	jitcClobberAll();
	asmShiftRegImm(X86_SHR, b, 28);
	// mov [4*b+sr], s
	byte modrm[6];
	asmALUMemReg(X86_MOV, modrm, x86_mem_sib(modrm, REG_NO, 4, b, (uint32)(&gCPU.sr[0])), s);
	asmCALL((NativeAddress)ppc_mmu_tlb_invalidate_all_asm);
	// sync
	asmALURegImm(X86_MOV, EAX, gJITC.pc+4);
	asmALURegImm(X86_MOV, ESI, gJITC.pc);
	asmJMP((NativeAddress)ppc_new_pc_rel_asm);
	return flowEndBlockUnreachable;	
}

/*
 *	rfi		Return from Interrupt
 *	.607
 */
void ppc_opc_rfi()
{
	if (gCPU.msr & MSR_PR) {
		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
		return;
	}
	ppc_set_msr(gCPU.srr[1]);
	gCPU.npc = gCPU.srr[0] & 0xfffffffc;
}
JITCFlow ppc_opc_gen_rfi()
{
	ppc_opc_gen_check_privilege();
	jitcClobberCarryAndFlags();
	jitcFlushRegister();
	jitcGetClientRegister(PPC_SRR1, NATIVE_REG | EAX);
	asmCALL((NativeAddress)ppc_set_msr_asm);
	byte modrm[6];
	asmALURegMem(X86_MOV, EAX, modrm, x86_mem(modrm, REG_NO, (uint32)(&gCPU.srr[0])));
	asmALURegImm(X86_MOV, ESI, gJITC.pc);
	asmALURegImm(X86_AND, EAX, 0xfffffffc);
	asmJMP((NativeAddress)ppc_new_pc_asm);
	return flowEndBlockUnreachable;
}

/*
 *	sc		System Call
 *	.621
 */
#include "io/graphic/gcard.h"
void ppc_opc_sc()
{
	if (gCPU.gpr[3] == 0x113724fa && gCPU.gpr[4] == 0x77810f9b) {
		gcard_osi();
		return;
	}
	ppc_exception(PPC_EXC_SC);
}
JITCFlow ppc_opc_gen_sc()
{
	jitcClobberCarryAndFlags();
	jitcFlushRegister();	
	
	NativeReg r1 = jitcGetClientRegister(PPC_GPR(3));
	asmALURegImm(X86_CMP, r1, 0x113724fa);
	asmALURegImm(X86_MOV, ESI, gJITC.pc+4);
	asmJxx(X86_NE, (NativeAddress)ppc_sc_exception_asm);

	jitcClobberRegister(NATIVE_REG | ESI);
	
	NativeReg r2 = jitcGetClientRegister(PPC_GPR(4));
	asmALURegImm(X86_CMP, r2, 0x77810f9b);
	if (r2 == ESI) {
		asmALURegImm(X86_MOV, ESI, gJITC.pc+4);
	}
	asmJxx(X86_NE, (NativeAddress)ppc_sc_exception_asm);

	asmCALL((NativeAddress)gcard_osi);

	jitcClobberRegister();
	return flowEndBlock;
}

/*
 *	sync		Synchronize
 *	.672
 */
void ppc_opc_sync()
{
	// NO-OP
}
JITCFlow ppc_opc_gen_sync()
{
	// NO-OP
	return flowContinue;
}

/*
 *	tlbia		Translation Lookaside Buffer Invalidate All
 *	.676
 */
void ppc_opc_tlbia()
{
	if (gCPU.msr & MSR_PR) {
		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
		return;
	}
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, rB);
	// FIXME: check rS.. for 0
	ppc_mmu_tlb_invalidate();
}
JITCFlow ppc_opc_gen_tlbia()
{
	ppc_opc_gen_check_privilege();
	jitcClobberAll();
	asmCALL((NativeAddress)ppc_mmu_tlb_invalidate_all_asm);
	asmALURegImm(X86_MOV, EAX, gJITC.pc+4);
	asmALURegImm(X86_MOV, ESI, gJITC.pc);
	asmJMP((NativeAddress)ppc_new_pc_rel_asm);
	return flowEndBlockUnreachable;
}

/*
 *	tlbie		Translation Lookaside Buffer Invalidate Entry
 *	.676
 */
void ppc_opc_tlbie()
{
	if (gCPU.msr & MSR_PR) {
		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
		return;
	}
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, rB);
	// FIXME: check rS.. for 0
	ppc_mmu_tlb_invalidate();
}
JITCFlow ppc_opc_gen_tlbie()
{
	ppc_opc_gen_check_privilege();
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, rA, rB);
	jitcGetClientRegister(PPC_GPR(rB), NATIVE_REG | EAX);
	jitcClobberAll();
	asmCALL((NativeAddress)ppc_mmu_tlb_invalidate_entry_asm);
	asmALURegImm(X86_MOV, EAX, gJITC.pc+4);
	asmALURegImm(X86_MOV, ESI, gJITC.pc);
	asmJMP((NativeAddress)ppc_new_pc_rel_asm);
	return flowEndBlockUnreachable;
}

/*
 *	tlbsync		Translation Lookaside Buffer Syncronize
 *	.677
 */
void ppc_opc_tlbsync()
{
	if (gCPU.msr & MSR_PR) {
		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
		return;
	}
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, rB);
	// FIXME: check rS.. for 0     
}
JITCFlow ppc_opc_gen_tlbsync()
{
	ppc_opc_gen_check_privilege();
	return flowContinue;
}

/*
 *	tw		Trap Word
 *	.678
 */
void ppc_opc_tw()
{
	int TO, rA, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, TO, rA, rB);
	uint32 a = gCPU.gpr[rA];
	uint32 b = gCPU.gpr[rB];
	if (((TO & 16) && ((sint32)a < (sint32)b)) 
	|| ((TO & 8) && ((sint32)a > (sint32)b)) 
	|| ((TO & 4) && (a == b)) 
	|| ((TO & 2) && (a < b)) 
	|| ((TO & 1) && (a > b))) {
		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_TRAP);
	}
}
JITCFlow ppc_opc_gen_tw()
{
	int TO, rA, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, TO, rA, rB);
	if (TO) {
		NativeReg a = jitcGetClientRegister(PPC_GPR(rA));
		NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
		jitcClobberAll();
		asmALURegImm(X86_CMP, a, b);
		NativeAddress fixup1=NULL, fixup2=NULL, fixup3=NULL, fixup4=NULL, fixup5=NULL;
		if (TO & 16) fixup1 = asmJxxFixup(X86_GE);
		if (TO & 8) fixup2 = asmJxxFixup(X86_LE);
		if (TO & 4) fixup3 = asmJxxFixup(X86_NE);
		if (TO & 2) fixup4 = asmJxxFixup(X86_AE);
		if (TO & 1) fixup5 = asmJxxFixup(X86_BE);
		asmALURegImm(X86_MOV, ESI, gJITC.pc);
		asmALURegImm(X86_MOV, ECX, PPC_EXC_PROGRAM_TRAP);
		asmJMP((NativeAddress)ppc_program_exception_asm);
		if (fixup1) asmResolveFixup(fixup1, asmHERE());
		if (fixup2) asmResolveFixup(fixup2, asmHERE());
		if (fixup3) asmResolveFixup(fixup3, asmHERE());
		if (fixup4) asmResolveFixup(fixup4, asmHERE());
		if (fixup5) asmResolveFixup(fixup5, asmHERE());
		return flowEndBlock;
	} else {
		return flowContinue;
	}
}

/*
 *	twi		Trap Word Immediate
 *	.679
 */
void ppc_opc_twi()
{
	int TO, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, TO, rA, imm);
	uint32 a = gCPU.gpr[rA];
	if (((TO & 16) && ((sint32)a < (sint32)imm)) 
	|| ((TO & 8) && ((sint32)a > (sint32)imm)) 
	|| ((TO & 4) && (a == imm)) 
	|| ((TO & 2) && (a < imm)) 
	|| ((TO & 1) && (a > imm))) {
		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_TRAP);
	}
}
JITCFlow ppc_opc_gen_twi()
{
	int TO, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, TO, rA, imm);
	if (TO) {
		NativeReg a = jitcGetClientRegister(PPC_GPR(rA));
		jitcClobberAll();
		asmALURegImm(X86_CMP, a, imm);
		NativeAddress fixup1=NULL, fixup2=NULL, fixup3=NULL, fixup4=NULL, fixup5=NULL;
		if (TO & 16) fixup1 = asmJxxFixup(X86_GE);
		if (TO & 8) fixup2 = asmJxxFixup(X86_LE);
		if (TO & 4) fixup3 = asmJxxFixup(X86_NE);
		if (TO & 2) fixup4 = asmJxxFixup(X86_AE);
		if (TO & 1) fixup5 = asmJxxFixup(X86_BE);
		asmALURegImm(X86_MOV, ESI, gJITC.pc);
		asmALURegImm(X86_MOV, ECX, PPC_EXC_PROGRAM_TRAP);
		asmJMP((NativeAddress)ppc_program_exception_asm);
		if (fixup1) asmResolveFixup(fixup1, asmHERE());
		if (fixup2) asmResolveFixup(fixup2, asmHERE());
		if (fixup3) asmResolveFixup(fixup3, asmHERE());
		if (fixup4) asmResolveFixup(fixup4, asmHERE());
		if (fixup5) asmResolveFixup(fixup5, asmHERE());
		return flowEndBlock;
	} else {
		return flowContinue;
	}
}
