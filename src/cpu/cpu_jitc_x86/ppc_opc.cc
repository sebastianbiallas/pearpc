/*
 *	PearPC
 *	ppc_opc.cc
 *
 *	Copyright (C) 2003, 2004 Sebastian Biallas (sb@biallas.net)
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

static void FASTCALL writeDEC(uint32 newdec)
{
//	PPC_OPC_WARN("write dec=%08x\n", newdec);
	if (!(gCPU.dec & 0x80000000) && (newdec & 0x80000000)) {
		gCPU.dec = newdec;
		sys_set_timer(gDECtimer, 0, 0, false);
 	} else {
		gCPU.dec = newdec;
		/*
		 *	1000000000ULL and gCPU.dec are both smaller than 2^32
		 *	so this expression can't overflow
		 */
		uint64 q = 1000000000ULL*gCPU.dec / gClientTimeBaseFrequency;

		// FIXME: Occasionally, ppc seems to generate very large dec values
		// as a result of a memory overwrite or something else. Let's handle
		// that until we figure out why.
		if (q > 20 * 1000 * 1000) {
			PPC_OPC_WARN("write dec > 20 millisec := %08x (%qu)\n", gCPU.dec, q);
			q = 10 * 1000 * 1000;
			sys_set_timer(gDECtimer, 0, q, false);
		} else {
			sys_set_timer(gDECtimer, 0, q, false);
		}
	}
	gDECwriteValue = gCPU.dec;
	gDECwriteITB = ppc_get_cpu_ideal_timebase();
}

static void FASTCALL writeTBL(uint32 newtbl)
{
	uint64 tbBase = ppc_get_cpu_timebase();
	gCPU.tb = (tbBase & 0xffffffff00000000ULL) | (uint64)newtbl;
}
									
static void FASTCALL writeTBU(uint32 newtbu)
{
	uint64 tbBase = ppc_get_cpu_timebase();
	gCPU.tb = ((uint64)newtbu << 32) | (tbBase & 0xffffffff);
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
		jitcClobberCarryAndFlags();
		jitcFloatRegisterClobberAll();
		jitcFlushVectorRegister();
		NativeReg msr = jitcGetClientRegisterMapping(PPC_MSR);
		if (msr == REG_NO) {
			asmTEST32(&gCPU.msr, MSR_PR);
		} else {
			asmALU32(X86_TEST, msr, MSR_PR);
		}
		NativeAddress fixup = asmJxxFixup(X86_Z);
		jitcFlushRegisterDirty();
		asmALU32(X86_MOV, ECX, PPC_EXC_PROGRAM_PRIV);
		asmALU32(X86_MOV, EDX, gJITC.current_opc);
		asmALU32(X86_MOV, ESI, gJITC.pc);
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
		 *	We assure here 7+6+5+5 bytes, to have enough space for 
		 *	four instructions (since we want to modify them)
		 */
		jitcEmitAssure(7+6+5+5);
		
		asmMOV32_NoFlags(EAX, li);
		asmCALL((NativeAddress)ppc_heartbeat_ext_rel_asm);
		asmMOV32_NoFlags(EAX, li);
		asmCALL((NativeAddress)ppc_new_pc_this_page_asm);
		asmNOP(3);
	} else {
		asmALU32(X86_MOV, EAX, li);
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
		asmALU32(X86_MOV, EAX, &gCPU.current_code_base);
		asmALU32(X86_ADD, EAX, gJITC.pc+4);
		asmALU32(X86_MOV, &gCPU.lr, EAX);
	}
	if (gJITC.current_opc & PPC_OPC_AA) {
		asmALU32(X86_MOV, EAX, li);
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
	jitcFlushVectorRegister();
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
					asmSET8(X86_C, &gCPU.xer_ca);
				}
				asmCALL((NativeAddress)ppc_flush_flags_asm);
				jitcFlushRegisterDirty();
				if (gJITC.current_opc & PPC_OPC_LK) {
					asmALU32(X86_MOV, EAX, &gCPU.current_code_base);
					asmALU32(X86_ADD, EAX, gJITC.pc+4);
					asmALU32(X86_MOV, &gCPU.lr, EAX);
				}
				if (gJITC.current_opc & PPC_OPC_AA) {
					asmALU32(X86_MOV, EAX, BD);
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
				asmTEST32(&gCPU.cr, 1<<(31-BI));
				fixup = asmJxxFixup((BO & 8) ? X86_Z : X86_NZ);
			}
		} else {
			// decrement and check condition
			jitcClobberCarryAndFlags();
			NativeReg ctr = jitcGetClientRegisterDirty(PPC_CTR);
			asmDEC32(ctr);
			NativeAddress fixup = asmJxxFixup((BO & 2) ? X86_NZ : X86_Z);
			asmTEST32(&gCPU.cr, 1<<(31-BI));
			NativeAddress fixup2 = asmJxxFixup((BO & 8) ? X86_Z : X86_NZ);
			jitcFlushRegisterDirty();
			if (gJITC.current_opc & PPC_OPC_LK) {
				asmALU32(X86_MOV, EAX, &gCPU.current_code_base);
				asmALU32(X86_ADD, EAX, gJITC.pc+4);
				asmALU32(X86_MOV, &gCPU.lr, EAX);
			}
			if (gJITC.current_opc & PPC_OPC_AA) {
				asmALU32(X86_MOV, EAX, BD);
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
				asmALU32(X86_MOV, EAX, &gCPU.current_code_base);
				asmALU32(X86_ADD, EAX, gJITC.pc+4);
				asmALU32(X86_MOV, &gCPU.lr, EAX);
			}
			if (gJITC.current_opc & PPC_OPC_AA) {
				asmALU32(X86_MOV, EAX, BD);
				asmJMP((NativeAddress)ppc_new_pc_asm);
		    	} else {
				ppc_opc_gen_set_pc_rel(BD);
			}
			return flowEndBlockUnreachable;
		} else {
			// decrement ctr and branch on ctr
			jitcClobberCarryAndFlags();
			NativeReg ctr = jitcGetClientRegisterDirty(PPC_CTR);
			asmDEC32(ctr);
			fixup = asmJxxFixup((BO & 2) ? X86_NZ : X86_Z);
		}
	}
	jitcFlushRegisterDirty();
	if (gJITC.current_opc & PPC_OPC_LK) {
		asmALU32(X86_MOV, EAX, &gCPU.current_code_base);
		asmALU32(X86_ADD, EAX, gJITC.pc+4);
		asmALU32(X86_MOV, &gCPU.lr, EAX);
	}
	if (gJITC.current_opc & PPC_OPC_AA) {
		asmALU32(X86_MOV, EAX, BD);
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
	jitcFlushVectorRegister();
	if (BO & 16) {
		// branch always
		jitcClobberCarryAndFlags();
		jitcFlushRegister();
		jitcGetClientRegister(PPC_CTR, NATIVE_REG | EAX);
		if (gJITC.current_opc & PPC_OPC_LK) {
			asmALU32(X86_MOV, ECX, &gCPU.current_code_base);
			asmALU32(X86_ADD, ECX, gJITC.pc+4);
			asmALU32(X86_MOV, &gCPU.lr, ECX);
		}
		asmALU32(X86_AND, EAX, 0xfffffffc);
		asmJMP((NativeAddress)ppc_new_pc_asm);
		return flowEndBlockUnreachable;
	} else {
		// test specific crX bit
		jitcClobberCarryAndFlags();
		asmTEST32(&gCPU.cr, 1<<(31-BI));
		jitcGetClientRegister(PPC_CTR, NATIVE_REG | EAX);
		NativeAddress fixup = asmJxxFixup((BO & 8) ? X86_Z : X86_NZ);
		jitcFlushRegisterDirty();
		if (gJITC.current_opc & PPC_OPC_LK) {
			asmALU32(X86_MOV, ECX, &gCPU.current_code_base);
			asmALU32(X86_ADD, ECX, gJITC.pc+4);
			asmALU32(X86_MOV, &gCPU.lr, ECX);
		}
		asmALU32(X86_AND, EAX, 0xfffffffc);
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
	jitcFlushVectorRegister();
	if (BO & 16) {
		// branch always
		jitcClobberCarryAndFlags();
		jitcFlushRegister();
		jitcGetClientRegister(PPC_LR, NATIVE_REG | EAX);
		if (gJITC.current_opc & PPC_OPC_LK) {
			asmALU32(X86_MOV, ECX, &gCPU.current_code_base);
			asmALU32(X86_ADD, ECX, gJITC.pc+4);
			asmALU32(X86_MOV, &gCPU.lr, ECX);
		}
		asmALU32(X86_AND, EAX, 0xfffffffc);
		asmJMP((NativeAddress)ppc_new_pc_asm);
		return flowEndBlockUnreachable;
	} else {
		jitcClobberCarryAndFlags();
		// test specific crX bit
		asmTEST32(&gCPU.cr, 1<<(31-BI));
		jitcGetClientRegister(PPC_LR, NATIVE_REG | EAX);
		NativeAddress fixup = asmJxxFixup((BO & 8) ? X86_Z : X86_NZ);
		jitcFlushRegisterDirty();
		if (gJITC.current_opc & PPC_OPC_LK) {
			asmALU32(X86_MOV, ECX, &gCPU.current_code_base);
			asmALU32(X86_ADD, ECX, gJITC.pc+4);
			asmALU32(X86_MOV, &gCPU.lr, ECX);
		}
		asmALU32(X86_AND, EAX, 0xfffffffc);
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
		asmALU32(X86_MOV, EAX, &gCPU.gpr[rA]);
		asmALU32(X86_ADD, EAX, &gCPU.gpr[rB]);
	} else {
		asmALU32(X86_MOV, EAX, &gCPU.gpr[rB]);
	}
	asmALU32(X86_MOV, ESI, gJITC.pc);
	asmCALL((NativeAddress)ppc_opc_icbi_asm);
	asmALU32(X86_MOV, EAX, gJITC.pc+4);
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
	asmALU32(X86_MOV, reg1, reg2);
}

static void inline move_reg0(PPC_Register creg1)
{
	NativeReg reg1 = jitcMapClientRegisterDirty(creg1);
	asmMOV32_NoFlags(reg1, 0);
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
	asmALU32(X86_MOV, d, &gCPU.cr);
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

void FASTCALL unknown_tbr_warning(uint32 a, uint32 spr1, uint32 spr2)
{
	PPC_OPC_WARN("invalid tbr %d:%d  @%08x\n", spr1, spr2, a);
}

void FASTCALL unknown_spr_warning(uint32 a, uint32 spr1, uint32 spr2)
{
	PPC_OPC_WARN("invalid spr %d:%d  @%08x\n", spr1, spr2, a);
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
	case 8:	// altivec makes this user visible
		if (spr1 == 0) {
			gCPU.gpr[rD] = gCPU.vrsave; 
			return;
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
		case 12: gCPU.gpr[rD] = ppc_get_cpu_timebase(); return;
		case 13: gCPU.gpr[rD] = ppc_get_cpu_timebase() >> 32; return;
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
	case 29:
		switch (spr1) {
		case 16:
			gCPU.gpr[rD] = 0;
			return;
		case 17:
			gCPU.gpr[rD] = 0;
			return;
		case 18:
			gCPU.gpr[rD] = 0;
			return;
		case 24:
			gCPU.gpr[rD] = 0;
			return;
		case 25:
			gCPU.gpr[rD] = 0;
			return;
		case 26:
			gCPU.gpr[rD] = 0;
			return;
		case 28:
			gCPU.gpr[rD] = 0;
			return;
		case 29:
			gCPU.gpr[rD] = 0;
			return;
		case 30:
			gCPU.gpr[rD] = 0;
			return;
		}
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
		case 22:
			gCPU.gpr[rD] = 0;
			return;
		case 23:
			gCPU.gpr[rD] = 0;
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
		case 31:
//			PPC_OPC_WARN("read from spr %d:%d (???) not supported!\n", spr1, spr2);
			gCPU.gpr[rD] = 0;
			return;
		}
	}
	fprintf(stderr, "unknown mfspr: %i:%i\n", spr1, spr2);
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
			asmALU32(X86_SBB, reg1, reg1);   // reg1 = CA ? -1 : 0
			asmALU32(X86_AND, reg1, XER_CA); // reg1 = CA ? XER_CA : 0
			asmALU32(X86_OR, reg1, reg2);
			jitcClobberCarry();
			return flowContinue;
		}
		case 8: move_reg(PPC_GPR(rD), PPC_LR); return flowContinue;
		case 9: move_reg(PPC_GPR(rD), PPC_CTR); return flowContinue;
		}
	case 8:
		if (spr1 == 0) {
			move_reg(PPC_GPR(rD), PPC_VRSAVE); 
			return flowContinue;
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
		case 12: {
			jitcClobberAll();
			asmCALL((NativeAddress)ppc_get_cpu_timebase);
			jitcMapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | EAX);
			return flowContinue;
		}
		case 13: {
			jitcClobberAll();
			asmCALL((NativeAddress)ppc_get_cpu_timebase);
			jitcMapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | EDX);
			return flowContinue;
		}
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
	case 29:
		switch (spr1) {
		case 16: move_reg0(PPC_GPR(rD)); return flowContinue; //g4
		case 17: move_reg0(PPC_GPR(rD)); return flowContinue; //g4
		case 18: move_reg0(PPC_GPR(rD)); return flowContinue; //g4
		case 24: move_reg0(PPC_GPR(rD)); return flowContinue; //g4
		case 25: move_reg0(PPC_GPR(rD)); return flowContinue; //g4
		case 26: move_reg0(PPC_GPR(rD)); return flowContinue; //g4
		case 28: move_reg0(PPC_GPR(rD)); return flowContinue; //g4
		case 29: move_reg0(PPC_GPR(rD)); return flowContinue; //g4
		case 30: move_reg0(PPC_GPR(rD)); return flowContinue; //g4
		}
		break;
	case 31:
		switch (spr1) {
		case 16: move_reg(PPC_GPR(rD), PPC_HID0); return flowContinue;
		case 17: move_reg(PPC_GPR(rD), PPC_HID1); return flowContinue;
		case 22: move_reg0(PPC_GPR(rD)); return flowContinue; //g4
		case 23: move_reg0(PPC_GPR(rD)); return flowContinue; //g4
		case 25: move_reg0(PPC_GPR(rD)); return flowContinue;
		case 27: move_reg0(PPC_GPR(rD)); return flowContinue;
		case 28: move_reg0(PPC_GPR(rD)); return flowContinue;
		case 29: move_reg0(PPC_GPR(rD)); return flowContinue;
		case 30: move_reg0(PPC_GPR(rD)); return flowContinue;
		case 31: move_reg0(PPC_GPR(rD)); return flowContinue;
		}
	}
	move_reg0(PPC_GPR(rD));
	jitcClobberAll();
	asmALU32(X86_MOV, EAX, &gCPU.current_code_base);
	asmALU32(X86_ADD, EAX, gJITC.pc);
	asmALU32(X86_MOV, EDX, spr1);
	asmALU32(X86_MOV, ECX, spr2);
	asmCALL((NativeAddress)unknown_spr_warning);
	return flowEndBlock;
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
	asmShift32(X86_SHR, b, 28);
	// mov d, [4*b+sr]
	asmALU32(X86_MOV, d, REG_NO, 4, b, (uint32)&gCPU.sr[0]);
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
	int rD, spr1, spr2;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rD, spr1, spr2);
	switch (spr2) {
	case 8:
		switch (spr1) {
		case 12:
			jitcClobberAll();
			asmCALL((NativeAddress)ppc_get_cpu_timebase);
			jitcMapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | EAX);
			return flowContinue;
							
		case 13:
			jitcClobberAll();
			asmCALL((NativeAddress)ppc_get_cpu_timebase);
			jitcMapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | EDX);
			return flowContinue;
		}
		break;
	}
	move_reg0(PPC_GPR(rD));
	jitcClobberAll();
	asmALU32(X86_MOV, EAX, &gCPU.current_code_base);
	asmALU32(X86_MOV, EDX, spr1);
	asmALU32(X86_MOV, ECX, spr2);
	asmCALL((NativeAddress)unknown_tbr_warning);
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
	asmALU32(X86_AND, s, CRM);
	asmALU32(X86_AND, &gCPU.cr, ~CRM);
	asmALU32(X86_OR, &gCPU.cr, s);
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
	asmALU32(X86_AND, r, 3); // RC
	asmAND32(&gCPU.x87cw, ~0x0c00);
	asmALU32(X86_MOV, r, REG_NO, 4, r, (uint32)&ppc_to_x86_roundmode);
	asmALU32(X86_OR, &gCPU.x87cw, r);
	asmFLDCW(&gCPU.x87cw);
}

JITCFlow ppc_opc_gen_mtfsb0x()
{
	int crbD, n1, n2;
	PPC_OPC_TEMPL_X(gJITC.current_opc, crbD, n1, n2);
	if (crbD != 1 && crbD != 2) {
		jitcGetClientRegister(PPC_FPSCR, NATIVE_REG | EAX);
		jitcClobberAll();
		asmALU32(X86_AND, EAX, ~(1<<(31-crbD)));
		asmALU32(X86_MOV, &gCPU.fpscr, EAX);
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
		asmALU32(X86_OR, EAX, 1<<(31-crbD));
		asmALU32(X86_MOV, &gCPU.fpscr, EAX);
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
	asmALU32(X86_AND, b, FM);
	asmALU32(X86_AND, fpscr, ~FM);
	asmALU32(X86_OR, fpscr, b);
	if (fm & 1) {
		asmALU32(X86_MOV, &gCPU.fpscr, fpscr);
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
	asmALU32(X86_AND, fpscr, ppc_cmp_and_mask[crfD]);
	asmALU32(X86_OR, fpscr, imm<<(crfD*4));
	if (crfD == 0) {
		asmALU32(X86_MOV, &gCPU.fpscr, fpscr);
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
	jitcClobberCarryAndFlags();
	jitcFlushRegister();
	ppc_opc_gen_check_privilege();
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, rA, rB);
	jitcGetClientRegister(PPC_GPR(rS), NATIVE_REG | EAX);
	asmCALL((NativeAddress)ppc_set_msr_asm);
	asmALU32(X86_MOV, EAX, gJITC.pc+4);
	asmJMP((NativeAddress)ppc_new_pc_rel_asm);
//	return flowContinue;
	return flowEndBlockUnreachable;
}


static inline void ppc_opc_batu_helper(bool dbat, int idx)
{
	if (dbat) {
		gCPU.dbat_bl[idx] = ((~gCPU.dbatu[idx] << 15) & 0xfffe0000);
		gCPU.dbat_nbl[idx] = ~gCPU.dbat_bl[idx];
		gCPU.dbat_bepi[idx] = (gCPU.dbatu[idx] & gCPU.dbat_bl[idx]);
	} else {
		gCPU.ibat_bl[idx] = ((~gCPU.ibatu[idx] << 15) & 0xfffe0000);
		gCPU.ibat_bepi[idx] = (gCPU.ibatu[idx] & gCPU.ibat_bl[idx]);
	}
}

static inline void ppc_opc_batl_helper(bool dbat, int idx)
{
	if (dbat) {
		gCPU.dbat_brpn[idx] = (gCPU.dbatl[idx] & gCPU.dbat_bl[idx]);
	} else {
		gCPU.ibat_brpn[idx] = (gCPU.ibatl[idx] & gCPU.ibat_bl[idx]);
	}
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
	case 8:
		if (spr1 == 0) {
			gCPU.vrsave = gCPU.gpr[rS]; 
			return;
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
			writeDEC(gCPU.gpr[rS]);
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
		case 28: writeTBL(gCPU.gpr[rS]); return;
		case 29: writeTBU(gCPU.gpr[rS]); return;
/*		case 26: gCPU.gpr[rD] = gCPU.ear; return;
		case 31: gCPU.gpr[rD] = gCPU.pvr; return;*/
		}
		break;
	case 16:
		switch (spr1) {
		case 16:
			gCPU.ibatu[0] = gCPU.gpr[rS];
			ppc_opc_batu_helper(false, 0);
			return;
		case 17:
			gCPU.ibatl[0] = gCPU.gpr[rS];
			ppc_opc_batl_helper(false, 0);
			return;
		case 18:
			gCPU.ibatu[1] = gCPU.gpr[rS];
			ppc_opc_batu_helper(false, 1);
			return;
		case 19:
			gCPU.ibatl[1] = gCPU.gpr[rS];
			ppc_opc_batl_helper(false, 1);
			return;
		case 20:
			gCPU.ibatu[2] = gCPU.gpr[rS];
			ppc_opc_batu_helper(false, 2);
			return;
		case 21:
			gCPU.ibatl[2] = gCPU.gpr[rS];
			ppc_opc_batl_helper(false, 2);
			return;
		case 22:
			gCPU.ibatu[3] = gCPU.gpr[rS];
			ppc_opc_batu_helper(false, 3);
			return;
		case 23:
			gCPU.ibatl[3] = gCPU.gpr[rS];
			ppc_opc_batl_helper(false, 3);
			return;
		case 24:
			gCPU.dbatu[0] = gCPU.gpr[rS];
			ppc_opc_batu_helper(true, 0);
			return;
		case 25:
			gCPU.dbatl[0] = gCPU.gpr[rS];
			ppc_opc_batl_helper(true, 0);
			return;
		case 26:
			gCPU.dbatu[1] = gCPU.gpr[rS];
			ppc_opc_batu_helper(true, 1);
			return;
		case 27:
			gCPU.dbatl[1] = gCPU.gpr[rS];
			ppc_opc_batl_helper(true, 1);
			return;
		case 28:
			gCPU.dbatu[2] = gCPU.gpr[rS];
			ppc_opc_batu_helper(true, 2);
			return;
		case 29:
			gCPU.dbatl[2] = gCPU.gpr[rS];
			ppc_opc_batl_helper(true, 2);
			return;
		case 30:
			gCPU.dbatu[3] = gCPU.gpr[rS];
			ppc_opc_batu_helper(true, 3);
			return;
		case 31:
			gCPU.dbatl[3] = gCPU.gpr[rS];
			ppc_opc_batl_helper(true, 3);
			return;
		}
		break;
	case 29:
		switch(spr1) {
		case 17: return;
		case 24: return;
		case 25: return;
		case 26: return;
		}
	case 31:
		switch (spr1) {
		case 16:
//			PPC_OPC_WARN("write(%08x) to spr %d:%d (HID0) not supported! @%08x\n", gCPU.gpr[rS], spr1, spr2, gCPU.pc);
			gCPU.hid[0] = gCPU.gpr[rS];
			return;
		case 17:
			PPC_OPC_WARN("write(%08x) to spr %d:%d (HID1) not supported! @%08x\n", gCPU.gpr[rS], spr1, spr2, gCPU.pc);
			gCPU.hid[1] = gCPU.gpr[rS];
			return;
		case 18:
			PPC_OPC_ERR("write(%08x) to spr %d:%d (IABR) not supported!\n", gCPU.gpr[rS], spr1, spr2);
			return;
		case 21:
			PPC_OPC_ERR("write(%08x) to spr %d:%d (DABR) not supported!\n", gCPU.gpr[rS], spr1, spr2);
			return;
		case 22:
			PPC_OPC_ERR("write(%08x) to spr %d:%d (?) not supported!\n", gCPU.gpr[rS], spr1, spr2);
			return;
		case 23:
			PPC_OPC_ERR("write(%08x) to spr %d:%d (?) not supported!\n", gCPU.gpr[rS], spr1, spr2);
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
		case 31: 
			return;
		}
	}
	fprintf(stderr, "unknown mtspr: %i:%i\n", spr1, spr2);
	SINGLESTEP("unknown mtspr\n");
}

static void FASTCALL ppc_mmu_set_sdr1_check_error(uint32 newsdr1)
{
	if (!ppc_mmu_set_sdr1(newsdr1, true)) {
		PPC_OPC_ERR("cannot set sdr1\n");
	}
}

static inline void ppc_opc_gen_batu_helper(bool dbat, int idx)
{
	NativeReg reg = jitcGetClientRegister(dbat ? PPC_DBATU(idx) : PPC_IBATU(idx));
	NativeReg tmp = jitcAllocRegister();

	jitcClobberCarryAndFlags();
	jitcClobberRegister(NATIVE_REG | reg);

	asmALU32(X86_MOV, tmp, reg);

	asmALU32(X86_NOT, reg);
	asmShift32(X86_SHL, reg, 15);
	asmALU32(X86_AND, reg, 0xfffe0000);
	asmALU32(X86_MOV, dbat ? &gCPU.dbat_bl[idx] : &gCPU.ibat_bl[idx], reg);

	asmALU32(X86_AND, tmp, reg);
	asmALU32(X86_MOV, dbat ? &gCPU.dbat_bepi[idx] : &gCPU.ibat_bepi[idx], tmp);

	asmALU32(X86_MOV, tmp, dbat ? &gCPU.dbatl[idx] : &gCPU.ibatl[idx]);
	asmALU32(X86_AND, tmp, reg);
	asmALU32(X86_MOV, dbat ? &gCPU.dbat_brpn[idx] : &gCPU.ibat_brpn[idx], tmp);

	asmALU32(X86_NOT, reg);
	asmALU32(X86_MOV, dbat ? &gCPU.dbat_nbl[idx] : &gCPU.ibat_nbl[idx], reg);
}

static inline void ppc_opc_gen_batl_helper(bool dbat, int idx)
{
	NativeReg reg = jitcGetClientRegister(dbat ? PPC_DBATL(idx) : PPC_IBATL(idx));

	jitcClobberCarryAndFlags();
	jitcClobberRegister(NATIVE_REG | reg);

	asmALU32(X86_AND, reg, dbat ? &gCPU.dbat_bl[idx] : &gCPU.ibat_bl[idx]);

	asmALU32(X86_MOV, dbat ? &gCPU.dbat_brpn[idx] : &gCPU.ibat_brpn[idx], reg);
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
			asmALU32(X86_MOV, reg1, reg2);
			asmALU32(X86_AND, reg1, ~XER_CA);
			asmBTx32(X86_BT, reg2, 29);
			jitcMapCarryDirty();
			return flowContinue;
		}
		case 8:	move_reg(PPC_LR, PPC_GPR(rS)); return flowContinue;
		case 9:	move_reg(PPC_CTR, PPC_GPR(rS)); return flowContinue;
		}
	case 8:
		if (spr1 == 0) {
			move_reg(PPC_VRSAVE, PPC_GPR(rS)); 
			return flowContinue;
		}
	}
	ppc_opc_gen_check_privilege();
	switch (spr2) {
	case 0:
		switch (spr1) {
		case 22: {
			jitcGetClientRegister(PPC_GPR(rS), NATIVE_REG | EAX);
			jitcClobberAll();
			asmCALL((NativeAddress)writeDEC);
			return flowContinue;
		}
		case 25: {
			jitcGetClientRegister(PPC_GPR(rS), NATIVE_REG | EAX);
			jitcClobberAll();
			asmCALL((NativeAddress)ppc_mmu_set_sdr1_check_error);
			asmALU32(X86_MOV, EAX, gJITC.pc+4);
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
		case 28:
			jitcGetClientRegister(PPC_GPR(rS), NATIVE_REG | EAX);
			jitcClobberAll();
			asmCALL((NativeAddress)writeTBL);
			return flowContinue;
		case 29:
			jitcGetClientRegister(PPC_GPR(rS), NATIVE_REG | EAX);
			jitcClobberAll();
			asmCALL((NativeAddress)writeTBU);
			return flowContinue;
		}
		break;
	case 16: {
		switch (spr1) {
		case 16:
			move_reg(PPC_IBATU(0), PPC_GPR(rS));
			ppc_opc_gen_batu_helper(false, 0);
			break;
		case 17:
			move_reg(PPC_IBATL(0), PPC_GPR(rS));
			ppc_opc_gen_batl_helper(false, 0);
			break;
		case 18:
			move_reg(PPC_IBATU(1), PPC_GPR(rS));
			ppc_opc_gen_batu_helper(false, 1);
			break;
		case 19:
			move_reg(PPC_IBATL(1), PPC_GPR(rS));
			ppc_opc_gen_batl_helper(false, 1);
			break;
		case 20:
			move_reg(PPC_IBATU(2), PPC_GPR(rS));
			ppc_opc_gen_batu_helper(false, 2);
			break;
		case 21:
			move_reg(PPC_IBATL(2), PPC_GPR(rS));
			ppc_opc_gen_batl_helper(false, 2);
			break;
		case 22:
			move_reg(PPC_IBATU(3), PPC_GPR(rS));
			ppc_opc_gen_batu_helper(false, 3);
			break;
		case 23:
			move_reg(PPC_IBATL(3), PPC_GPR(rS));
			ppc_opc_gen_batl_helper(false, 3);
			break;
		case 24:
			move_reg(PPC_DBATU(0), PPC_GPR(rS));
			ppc_opc_gen_batu_helper(true, 0);
			break;
		case 25:
			move_reg(PPC_DBATL(0), PPC_GPR(rS));
			ppc_opc_gen_batl_helper(true, 0);
			break;
		case 26:
			move_reg(PPC_DBATU(1), PPC_GPR(rS));
			ppc_opc_gen_batu_helper(true, 1);
			break;
		case 27:
			move_reg(PPC_DBATL(1), PPC_GPR(rS));
			ppc_opc_gen_batl_helper(true, 1);
			break;
		case 28:
			move_reg(PPC_DBATU(2), PPC_GPR(rS));
			ppc_opc_gen_batu_helper(true, 2);
			break;
		case 29:
			move_reg(PPC_DBATL(2), PPC_GPR(rS));
			ppc_opc_gen_batl_helper(true, 2);
			break;
		case 30:
			move_reg(PPC_DBATU(3), PPC_GPR(rS));
			ppc_opc_gen_batu_helper(true, 3);
			break;
		case 31:
			move_reg(PPC_DBATL(3), PPC_GPR(rS));
			ppc_opc_gen_batl_helper(true, 3);
			break;
		default: goto invalid;
		}
		jitcClobberAll();
		asmCALL((NativeAddress)ppc_mmu_tlb_invalidate_all_asm);
		asmALU32(X86_MOV, EAX, gJITC.pc+4);
		asmJMP((NativeAddress)ppc_new_pc_rel_asm);
		return flowEndBlockUnreachable;
	}
	case 29:
		switch (spr1) {
		case 17: return flowContinue; //g4
		case 24: return flowContinue; //g4
		case 25: return flowContinue; //g4
		case 26: return flowContinue; //g4
		}
	case 31:
		switch (spr1) {
		case 16: move_reg(PPC_HID0, PPC_GPR(rS)); return flowContinue;
		case 17: return flowContinue; //g4
		case 18: return flowContinue;
		case 21: return flowContinue; //g4
		case 22: return flowContinue;
		case 23: return flowContinue;
		case 27: return flowContinue;
		case 28: return flowContinue;
		case 29: return flowContinue;
		case 30: return flowContinue;
		case 31: return flowContinue; //g4
		}
	}
	invalid:
	jitcClobberAll();
	asmALU32(X86_MOV, EAX, &gCPU.current_code_base);
	asmALU32(X86_ADD, EAX, gJITC.pc);
	asmALU32(X86_MOV, EDX, spr1);
	asmALU32(X86_MOV, ECX, spr2);
	asmCALL((NativeAddress)unknown_spr_warning);
	return flowEndBlock;
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
	jitcFlushRegister();
	ppc_opc_gen_check_privilege();
	int rS, SR, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, SR, rB);
	// FIXME: check insn
	move_reg(PPC_SR(SR & 0xf), PPC_GPR(rS));
	jitcClobberAll();
	asmCALL((NativeAddress)ppc_mmu_tlb_invalidate_all_asm);
	// sync
//	asmALU32(X86_MOV, EAX, gJITC.pc+4);
//	asmJMP((NativeAddress)ppc_new_pc_rel_asm);
//	return flowEndBlockUnreachable;	
	return flowContinue;
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
	jitcFlushRegister();
	ppc_opc_gen_check_privilege();
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, rA, rB);
	// FIXME: check insn
	NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
	NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
	if (b == s) {
		s = jitcAllocRegister();
		asmALU32(X86_MOV, s, b);
	}
	jitcClobberAll();
	asmShift32(X86_SHR, b, 28);
	// mov [4*b+sr], s
	asmALU32(X86_MOV, REG_NO, 4, b, (uint32)&gCPU.sr[0], s);
	asmCALL((NativeAddress)ppc_mmu_tlb_invalidate_all_asm);
	// sync
//	asmALU32(X86_MOV, EAX, gJITC.pc+4);
//	asmJMP((NativeAddress)ppc_new_pc_rel_asm);
//	return flowEndBlockUnreachable;	
	return flowContinue;
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
	ppc_set_msr(gCPU.srr[1] & MSR_RFI_SAVE_MASK);
	gCPU.npc = gCPU.srr[0] & 0xfffffffc;
}
JITCFlow ppc_opc_gen_rfi()
{
	jitcClobberCarryAndFlags();
	jitcFlushRegister();
	ppc_opc_gen_check_privilege();
	jitcGetClientRegister(PPC_SRR1, NATIVE_REG | EAX);
	asmALU32(X86_AND, EAX, MSR_RFI_SAVE_MASK);
	asmCALL((NativeAddress)ppc_set_msr_asm);
	asmALU32(X86_MOV, EAX, &gCPU.srr[0]);
	asmALU32(X86_AND, EAX, 0xfffffffc);
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
		gcard_osi(0);
		return;
	}
	ppc_exception(PPC_EXC_SC);
}
JITCFlow ppc_opc_gen_sc()
{
	jitcClobberCarryAndFlags();
	jitcFlushRegister();	
	
	NativeReg r1 = jitcGetClientRegister(PPC_GPR(3));
	asmALU32(X86_CMP, r1, 0x113724fa);
	asmALU32(X86_MOV, ESI, gJITC.pc+4);
	asmJxx(X86_NE, (NativeAddress)ppc_sc_exception_asm);

	jitcClobberRegister(NATIVE_REG | ESI);
	
	NativeReg r2 = jitcGetClientRegister(PPC_GPR(4));
	asmALU32(X86_CMP, r2, 0x77810f9b);
	if (r2 == ESI) {
		asmALU32(X86_MOV, ESI, gJITC.pc+4);
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
	jitcClobberAll();
	ppc_opc_gen_check_privilege();
	asmCALL((NativeAddress)ppc_mmu_tlb_invalidate_all_asm);
	asmALU32(X86_MOV, EAX, gJITC.pc+4);
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
	jitcFlushRegister();
	ppc_opc_gen_check_privilege();
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, rA, rB);
	jitcGetClientRegister(PPC_GPR(rB), NATIVE_REG | EAX);
	jitcClobberAll();
	asmCALL((NativeAddress)ppc_mmu_tlb_invalidate_entry_asm);
	asmALU32(X86_MOV, EAX, gJITC.pc+4);
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
	if (TO == 0x1f) {
		// TRAP always
		jitcClobberAll();
		asmALU32(X86_MOV, ESI, gJITC.pc);
		asmALU32(X86_MOV, ECX, PPC_EXC_PROGRAM_TRAP);
		asmJMP((NativeAddress)ppc_program_exception_asm);
		return flowEndBlockUnreachable;
	} else if (TO) {
		NativeReg a = jitcGetClientRegister(PPC_GPR(rA));
		NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
		jitcClobberAll();
		asmALU32(X86_CMP, a, b);
		NativeAddress fixup1=NULL, fixup2=NULL, fixup3=NULL, fixup4=NULL, fixup5=NULL;
		if (TO & 16) fixup1 = asmJxxFixup(X86_L);
		if (TO & 8) fixup2 = asmJxxFixup(X86_G);
		if (TO & 4) fixup3 = asmJxxFixup(X86_E);
		if (TO & 2) fixup4 = asmJxxFixup(X86_B);
		if (TO & 1) fixup5 = asmJxxFixup(X86_A);
		NativeAddress fixup6 = asmJMPFixup();
		if (fixup1) asmResolveFixup(fixup1, asmHERE());
		if (fixup2) asmResolveFixup(fixup2, asmHERE());
		if (fixup3) asmResolveFixup(fixup3, asmHERE());
		if (fixup4) asmResolveFixup(fixup4, asmHERE());
		if (fixup5) asmResolveFixup(fixup5, asmHERE());
		asmALU32(X86_MOV, ESI, gJITC.pc);
		asmALU32(X86_MOV, ECX, PPC_EXC_PROGRAM_TRAP);
		asmJMP((NativeAddress)ppc_program_exception_asm);
		asmResolveFixup(fixup6, asmHERE());
		return flowEndBlock;
	} else {
		// TRAP never
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
	if (TO == 0x1f) {
		// TRAP always
		jitcClobberAll();
		asmALU32(X86_MOV, ESI, gJITC.pc);
		asmALU32(X86_MOV, ECX, PPC_EXC_PROGRAM_TRAP);
		asmJMP((NativeAddress)ppc_program_exception_asm);
		return flowEndBlockUnreachable;
	} else if (TO) {
		NativeReg a = jitcGetClientRegister(PPC_GPR(rA));
		jitcClobberAll();
		asmALU32(X86_CMP, a, imm);
		NativeAddress fixup1=NULL, fixup2=NULL, fixup3=NULL, fixup4=NULL, fixup5=NULL;
		if (TO & 16) fixup1 = asmJxxFixup(X86_L);
		if (TO & 8) fixup2 = asmJxxFixup(X86_G);
		if (TO & 4) fixup3 = asmJxxFixup(X86_E);
		if (TO & 2) fixup4 = asmJxxFixup(X86_B);
		if (TO & 1) fixup5 = asmJxxFixup(X86_A);
		NativeAddress fixup6 = asmJMPFixup();
		if (fixup1) asmResolveFixup(fixup1, asmHERE());
		if (fixup2) asmResolveFixup(fixup2, asmHERE());
		if (fixup3) asmResolveFixup(fixup3, asmHERE());
		if (fixup4) asmResolveFixup(fixup4, asmHERE());
		if (fixup5) asmResolveFixup(fixup5, asmHERE());
		asmALU32(X86_MOV, ESI, gJITC.pc);
		asmALU32(X86_MOV, ECX, PPC_EXC_PROGRAM_TRAP);
		asmJMP((NativeAddress)ppc_program_exception_asm);
		asmResolveFixup(fixup6, asmHERE());
		return flowEndBlock;
	} else {
		// TRAP never
		return flowContinue;
	}
}

/*      dcba	    Data Cache Block Allocate
 *      .???
 */
void ppc_opc_dcba()
{
	// FIXME: check addr
}
JITCFlow ppc_opc_gen_dcba()
{
	// FIXME: check addr
	return flowContinue;
}
