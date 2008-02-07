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

static uint64 gDECwriteITB;
static uint64 gDECwriteValue;

static void readDEC(PPC_CPU_State &aCPU)
{
	uint64 itb = ppc_get_cpu_ideal_timebase() - gDECwriteITB;
	aCPU.dec = gDECwriteValue - itb;
//	PPC_OPC_WARN("read  dec=%08x\n", aCPU.dec);
}
extern bool blbl;
static void FASTCALL writeDEC(PPC_CPU_State &aCPU, uint32 newdec)
{
//	PPC_OPC_WARN("write dec=%08x\n", newdec);
	if (!(aCPU.dec & 0x80000000) && (newdec & 0x80000000)) {
		aCPU.dec = newdec;
		sys_set_timer(gDECtimer, 0, 0, false);
 	} else {
		aCPU.dec = newdec;
		/*
		 *	1000000000ULL and aCPU.dec are both smaller than 2^32
		 *	so this expression can't overflow
		 */
		uint64 q = 1000000000ULL*aCPU.dec / gClientTimeBaseFrequency;

		// FIXME: Occasionally, ppc seems to generate very large dec values
		// as a result of a memory overwrite or something else. Let's handle
		// that until we figure out why.
		if (q > 20 * 1000 * 1000) {
			PPC_OPC_WARN("write dec > 20 millisec := %08x (%qu)\n", aCPU.dec, q);
			q = 10 * 1000 * 1000;
		}
		blbl=true;
		sys_set_timer(gDECtimer, 0, q, false);
//		PPC_OPC_WARN("timer(%qu)\n", q);
	}
	gDECwriteValue = aCPU.dec;
	gDECwriteITB = ppc_get_cpu_ideal_timebase();
}

static void FASTCALL writeTBL(PPC_CPU_State &aCPU, uint32 newtbl)
{
	uint64 tbBase = ppc_get_cpu_timebase();
	aCPU.tb = (tbBase & 0xffffffff00000000ULL) | (uint64)newtbl;
}
									
static void FASTCALL writeTBU(PPC_CPU_State &aCPU, uint32 newtbu)
{
	uint64 tbBase = ppc_get_cpu_timebase();
	aCPU.tb = ((uint64)newtbu << 32) | (tbBase & 0xffffffff);
}

void ppc_set_msr(PPC_CPU_State &aCPU, uint32 newmsr)
{
/*	if ((newmsr & MSR_EE) && !(aCPU.msr & MSR_EE)) {
		if (pic_check_interrupt()) {
			aCPU.exception_pending = true;
			aCPU.ext_exception = true;
		}
	}*/
	ppc_mmu_tlb_invalidate(aCPU);
#ifndef PPC_CPU_ENABLE_SINGLESTEP
	if (newmsr & MSR_SE) {
		SINGLESTEP("");
		PPC_CPU_WARN("MSR[SE] (singlestep enable) set, but compiled w/o SE support.\n");
	}
#else 
	aCPU.singlestep_ignore = true;
#endif
	if (newmsr & PPC_CPU_UNSUPPORTED_MSR_BITS) {
		PPC_CPU_ERR("unsupported bits in MSR set: %08x @%08x\n", newmsr & PPC_CPU_UNSUPPORTED_MSR_BITS, aCPU.pc);
	}
	if (newmsr & MSR_POW) {
		// doze();
		newmsr &= ~MSR_POW;
	}
	aCPU.msr = newmsr;
	
}

void ppc_opc_gen_check_privilege(JITC &jitc)
{
	if (!jitc.checkedPriviledge) {
		jitc.clobberCarryAndFlags();
		jitc.floatRegisterClobberAll();
//		jitc.flushVectorRegister(); FIXME64
		NativeReg msr = jitc.getClientRegisterMapping(PPC_MSR);
		if (msr == REG_NO) {
			jitc.asmTEST32(curCPU(msr), MSR_PR);
		} else {
			jitc.asmALU32(X86_TEST, msr, MSR_PR);
		}
		NativeAddress fixup = jitc.asmJxxFixup(X86_Z);
		jitc.flushRegisterDirty();
		jitc.asmALU32(X86_MOV, RCX, PPC_EXC_PROGRAM_PRIV);
//		jitc.asmALU32(X86_MOV, RDX, jitc.current_opc);
		jitc.asmALU32(X86_MOV, RSI, jitc.pc);
		jitc.asmJMP((NativeAddress)ppc_program_exception_asm);
		jitc.asmResolveFixup(fixup);
		jitc.checkedPriviledge = true;
	}
}

static inline void ppc_opc_gen_set_pc_rel(JITC &jitc, uint32 li)
{
	li += jitc.pc;
	if (li < 4096) {		
		jitc.asmMOV32_NoFlags(RAX, li);
		jitc.asmCALL((NativeAddress)ppc_heartbeat_ext_rel_asm);
		/*
		 *	This will be patched:
		 */
		jitc.emitAssure(5+5);
		jitc.asmMOV32_NoFlags(RAX, li); // 5
		jitc.asmCALL((NativeAddress)ppc_new_pc_this_page_asm); // 5
	} else {
		jitc.asmALU32(X86_MOV, RAX, li);
		jitc.asmJMP((NativeAddress)ppc_new_pc_rel_asm);
	}
}

/*
 *	bx		Branch
 *	.435
 */
void ppc_opc_bx(PPC_CPU_State &aCPU)
{
	uint32 li;
	PPC_OPC_TEMPL_I(aCPU.current_opc, li);
	if (!(aCPU.current_opc & PPC_OPC_AA)) {
		li += aCPU.pc;
	}
	if (aCPU.current_opc & PPC_OPC_LK) {
		aCPU.lr = aCPU.pc + 4;
	}
	aCPU.npc = li;
}

JITCFlow ppc_opc_gen_bx(JITC &jitc)
{
	uint32 li;
	PPC_OPC_TEMPL_I(jitc.current_opc, li);
	jitc.clobberAll();
	if (jitc.current_opc & PPC_OPC_LK) {
		jitc.asmALU32(X86_MOV, RAX, curCPU(current_code_base));
		jitc.asmALU32(X86_ADD, RAX, jitc.pc+4);
		jitc.asmALU32(X86_MOV, curCPU(lr), RAX);
	}
	if (jitc.current_opc & PPC_OPC_AA) {
		jitc.asmALU32(X86_MOV, RAX, li);
		jitc.asmJMP((NativeAddress)ppc_new_pc_asm);
	} else {
		ppc_opc_gen_set_pc_rel(jitc, li);
	}
	return flowEndBlockUnreachable;
}

/*
 *	bcx		Branch Conditional
 *	.436
 */
void ppc_opc_bcx(PPC_CPU_State &aCPU)
{
	uint32 BO, BI, BD;
	PPC_OPC_TEMPL_B(aCPU.current_opc, BO, BI, BD);
	if (!(BO & 4)) {
		aCPU.ctr--;
	}
	bool bo2 = (BO & 2);
	bool bo8 = (BO & 8); // branch condition true
	bool cr = (aCPU.cr & (1<<(31-BI)));
	if (((BO & 4) || ((aCPU.ctr!=0) ^ bo2))
	&& ((BO & 16) || (!(cr ^ bo8)))) {
		if (!(aCPU.current_opc & PPC_OPC_AA)) {
			BD += aCPU.pc;
		}
		if (aCPU.current_opc & PPC_OPC_LK) {
			aCPU.lr = aCPU.pc + 4;
		}
		aCPU.npc = BD;
	}
}
JITCFlow ppc_opc_gen_bcx(JITC &jitc)
{
	uint32 BO, BI, BD;
	PPC_OPC_TEMPL_B(jitc.current_opc, BO, BI, BD);
	NativeAddress fixup = NULL;
	jitc.floatRegisterClobberAll();
//	jitc.flushVectorRegister(); FIX64
	if (!(BO & 16)) {
		// only branch if condition
		if (BO & 4) {
			// don't check ctr
			PPC_CRx cr = (PPC_CRx)(BI / 4);
			if (jitc.flagsMapped() && jitc.getFlagsMapping() == cr && (BI%4) != 3) {
				// x86 flags map to correct crX register
				// and not SO flag (which isnt mapped)
				jitc.clobberRegister(NATIVE_REG | RDI);
				NativeAddress fixup2=NULL;
				switch (BI%4) {
				case 0:
					// less than
					fixup = jitc.asmJxxFixup((BO & 8) ? X86_NS : X86_S);
					break;
				case 1:
					// greater than
					// there seems to be no equivalent instruction on the x86
					if (BO & 8) {
						fixup = jitc.asmJxxFixup(X86_S);
						fixup2 = jitc.asmJxxFixup(X86_Z);
					} else {
						NativeAddress fixup3 = jitc.asmJxxFixup(X86_S);
						NativeAddress fixup4 = jitc.asmJxxFixup(X86_Z);
						fixup = jitc.asmJMPFixup();
						jitc.asmResolveFixup(fixup3, jitc.asmHERE());
						jitc.asmResolveFixup(fixup4, jitc.asmHERE());
					}
					break;
				case 2:
					// equal
					fixup = jitc.asmJxxFixup((BO & 8) ? X86_NZ : X86_Z);
					break;
				}
				// FIXME: optimize me
				if (jitc.carryMapped()) {
					jitc.asmSET8(X86_C, curCPU(xer_ca));
				}
				jitc.asmCALL((NativeAddress)ppc_flush_flags_asm);
				jitc.flushRegisterDirty();
				if (jitc.current_opc & PPC_OPC_LK) {
					jitc.asmALU32(X86_MOV, RAX, curCPU(current_code_base));
					jitc.asmALU32(X86_ADD, RAX, jitc.pc+4);
					jitc.asmALU32(X86_MOV, curCPU(lr), RAX);
				}
				if (jitc.current_opc & PPC_OPC_AA) {
					jitc.asmALU32(X86_MOV, RAX, BD);
					jitc.asmJMP((NativeAddress)ppc_new_pc_asm);
				} else {
					ppc_opc_gen_set_pc_rel(jitc, BD);
				}
				jitc.asmResolveFixup(fixup, jitc.asmHERE());
				if (fixup2) {
					jitc.asmResolveFixup(fixup2, jitc.asmHERE());
				}
				return flowContinue;
			} else {
				jitc.clobberCarryAndFlags();
				// test specific crX bit
				jitc.asmTEST32(curCPU(cr), 1<<(31-BI));
				fixup = jitc.asmJxxFixup((BO & 8) ? X86_Z : X86_NZ);
			}
		} else {
			// decrement and check condition
			jitc.clobberCarryAndFlags();
			NativeReg ctr = jitc.getClientRegisterDirty(PPC_CTR);
			jitc.asmDEC32(ctr);
			NativeAddress fixup = jitc.asmJxxFixup((BO & 2) ? X86_NZ : X86_Z);
			jitc.asmTEST32(curCPU(cr), 1<<(31-BI));
			NativeAddress fixup2 = jitc.asmJxxFixup((BO & 8) ? X86_Z : X86_NZ);
			jitc.flushRegisterDirty();
			if (jitc.current_opc & PPC_OPC_LK) {
				jitc.asmALU32(X86_MOV, RAX, curCPU(current_code_base));
				jitc.asmALU32(X86_ADD, RAX, jitc.pc+4);
				jitc.asmALU32(X86_MOV, curCPU(lr), RAX);
			}
			if (jitc.current_opc & PPC_OPC_AA) {
				jitc.asmALU32(X86_MOV, RAX, BD);
				jitc.asmJMP((NativeAddress)ppc_new_pc_asm);
			} else {
				ppc_opc_gen_set_pc_rel(jitc, BD);
			}
			jitc.asmResolveFixup(fixup, jitc.asmHERE());
			jitc.asmResolveFixup(fixup2, jitc.asmHERE());
			return flowContinue;
		}
	} else {
		// don't check condition
		if (BO & 4) {
			// always branch
			jitc.clobberCarryAndFlags();
			jitc.flushRegister();
			if (jitc.current_opc & PPC_OPC_LK) {
				jitc.asmALU32(X86_MOV, RAX, curCPU(current_code_base));
				jitc.asmALU32(X86_ADD, RAX, jitc.pc+4);
				jitc.asmALU32(X86_MOV, curCPU(lr), RAX);
			}
			if (jitc.current_opc & PPC_OPC_AA) {
				jitc.asmALU32(X86_MOV, RAX, BD);
				jitc.asmJMP((NativeAddress)ppc_new_pc_asm);
		    	} else {
				ppc_opc_gen_set_pc_rel(jitc, BD);
			}
			return flowEndBlockUnreachable;
		} else {
			// decrement ctr and branch on ctr
			jitc.clobberCarryAndFlags();
			NativeReg ctr = jitc.getClientRegisterDirty(PPC_CTR);
			jitc.asmDEC32(ctr);
			fixup = jitc.asmJxxFixup((BO & 2) ? X86_NZ : X86_Z);
		}
	}
	jitc.flushRegisterDirty();
	if (jitc.current_opc & PPC_OPC_LK) {
		jitc.asmALU32(X86_MOV, RAX, curCPU(current_code_base));
		jitc.asmALU32(X86_ADD, RAX, jitc.pc+4);
		jitc.asmALU32(X86_MOV, curCPU(lr), RAX);
	}
	if (jitc.current_opc & PPC_OPC_AA) {
		jitc.asmALU32(X86_MOV, RAX, BD);
		jitc.asmJMP((NativeAddress)ppc_new_pc_asm);
	} else {
		ppc_opc_gen_set_pc_rel(jitc, BD);
	}
	jitc.asmResolveFixup(fixup, jitc.asmHERE());
	return flowContinue;
}

/*
 *	bcctrx		Branch Conditional to Count Register
 *	.438
 */
void ppc_opc_bcctrx(PPC_CPU_State &aCPU)
{
	uint32 BO, BI, BD;
	PPC_OPC_TEMPL_XL(aCPU.current_opc, BO, BI, BD);
	PPC_OPC_ASSERT(BD==0);
	PPC_OPC_ASSERT(!(BO & 2));     
	bool bo8 = (BO & 8);
	bool cr = (aCPU.cr & (1<<(31-BI)));
	if ((BO & 16) || (!(cr ^ bo8))) {
		if (aCPU.current_opc & PPC_OPC_LK) {
			aCPU.lr = aCPU.pc + 4;
		}
		aCPU.npc = aCPU.ctr & 0xfffffffc;
	}
}
JITCFlow ppc_opc_gen_bcctrx(JITC &jitc)
{
	uint32 BO, BI, BD;
	PPC_OPC_TEMPL_XL(jitc.current_opc, BO, BI, BD);
	jitc.floatRegisterClobberAll();
//	jitc.flushVectorRegister(); FIXME64
	if (BO & 16) {
		// branch always
		jitc.clobberCarryAndFlags();
		jitc.flushRegister();
		jitc.getClientRegister(PPC_CTR, NATIVE_REG | RAX);
		if (jitc.current_opc & PPC_OPC_LK) {
			jitc.asmALU32(X86_MOV, RCX, curCPU(current_code_base));
			jitc.asmALU32(X86_ADD, RCX, jitc.pc+4);
			jitc.asmALU32(X86_MOV, curCPU(lr), RCX);
		}
		jitc.asmALU32(X86_AND, RAX, 0xfffffffc);
		jitc.asmJMP((NativeAddress)ppc_new_pc_asm);
		return flowEndBlockUnreachable;
	} else {
		// test specific crX bit
		jitc.clobberCarryAndFlags();
		jitc.asmTEST32(curCPU(cr), 1<<(31-BI));
		jitc.getClientRegister(PPC_CTR, NATIVE_REG | RAX);
		NativeAddress fixup = jitc.asmJxxFixup((BO & 8) ? X86_Z : X86_NZ);
		jitc.flushRegisterDirty();
		if (jitc.current_opc & PPC_OPC_LK) {
			jitc.asmALU32(X86_MOV, RCX, curCPU(current_code_base));
			jitc.asmALU32(X86_ADD, RCX, jitc.pc+4);
			jitc.asmALU32(X86_MOV, curCPU(lr), RCX);
		}
		jitc.asmALU32(X86_AND, RAX, 0xfffffffc);
		jitc.asmJMP((NativeAddress)ppc_new_pc_asm);
		jitc.asmResolveFixup(fixup, jitc.asmHERE());	
		return flowContinue;
	}
}
/*
 *	bclrx		Branch Conditional to Link Register
 *	.440
 */
void ppc_opc_bclrx(PPC_CPU_State &aCPU)
{
	uint32 BO, BI, BD;
	PPC_OPC_TEMPL_XL(aCPU.current_opc, BO, BI, BD);
	PPC_OPC_ASSERT(BD==0);
	if (!(BO & 4)) {
		aCPU.ctr--;
	}
	bool bo2 = (BO & 2);
	bool bo8 = (BO & 8);
	bool cr = (aCPU.cr & (1<<(31-BI)));
	if (((BO & 4) || ((aCPU.ctr!=0) ^ bo2))
	&& ((BO & 16) || (!(cr ^ bo8)))) {
		BD = aCPU.lr & 0xfffffffc;
		if (aCPU.current_opc & PPC_OPC_LK) {
			aCPU.lr = aCPU.pc + 4;
		}
		aCPU.npc = BD;
	}
}
JITCFlow ppc_opc_gen_bclrx(JITC &jitc)
{
	uint32 BO, BI, BD;
	PPC_OPC_TEMPL_XL(jitc.current_opc, BO, BI, BD);
	if (!(BO & 4)) {
		PPC_OPC_ERR("not impl.: bclrx + BO&4\n");
	}
	jitc.floatRegisterClobberAll();
//	jitc.flushVectorRegister(); FIX64
	if (BO & 16) {
		// branch always
		jitc.clobberCarryAndFlags();
		jitc.flushRegister();
		jitc.getClientRegister(PPC_LR, NATIVE_REG | RAX);
		if (jitc.current_opc & PPC_OPC_LK) {
			jitc.asmALU32(X86_MOV, RCX, curCPU(current_code_base));
			jitc.asmALU32(X86_ADD, RCX, jitc.pc+4);
			jitc.asmALU32(X86_MOV, curCPU(lr), RCX);
		}
		jitc.asmALU32(X86_AND, RAX, 0xfffffffc);
		jitc.asmJMP((NativeAddress)ppc_new_pc_asm);
		return flowEndBlockUnreachable;
	} else {
		jitc.clobberCarryAndFlags();
		// test specific crX bit
		jitc.asmTEST32(curCPU(cr), 1<<(31-BI));
		jitc.getClientRegister(PPC_LR, NATIVE_REG | RAX);
		NativeAddress fixup = jitc.asmJxxFixup((BO & 8) ? X86_Z : X86_NZ);
		jitc.flushRegisterDirty();
		if (jitc.current_opc & PPC_OPC_LK) {
			jitc.asmALU32(X86_MOV, RCX, curCPU(current_code_base));
			jitc.asmALU32(X86_ADD, RCX, jitc.pc+4);
			jitc.asmALU32(X86_MOV, curCPU(lr), RCX);
		}
		jitc.asmALU32(X86_AND, RAX, 0xfffffffc);
		jitc.asmJMP((NativeAddress)ppc_new_pc_asm);
		jitc.asmResolveFixup(fixup, jitc.asmHERE());
		return flowContinue;
	}
}

/*
 *	dcbf		Data Cache Block Flush
 *	.458
 */
void ppc_opc_dcbf(PPC_CPU_State &aCPU)
{
	// NO-OP
}
JITCFlow ppc_opc_gen_dcbf(JITC &jitc)
{
	// NO-OP
	return flowContinue;
}

/*
 *	dcbi		Data Cache Block Invalidate
 *	.460
 */
void ppc_opc_dcbi(PPC_CPU_State &aCPU)
{
	if (aCPU.msr & MSR_PR) {
//		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
		return;
	}
	// FIXME: check addr
}
JITCFlow ppc_opc_gen_dcbi(JITC &jitc)
{
	ppc_opc_gen_check_privilege(jitc);
	return flowContinue;
}
/*
 *	dcbst		Data Cache Block Store
 *	.461
 */
void ppc_opc_dcbst(PPC_CPU_State &aCPU)
{
	// NO-OP
}
JITCFlow ppc_opc_gen_dcbst(JITC &jitc)
{
	// NO-OP
	return flowContinue;
}
/*
 *	dcbt		Data Cache Block Touch
 *	.462
 */
void ppc_opc_dcbt(PPC_CPU_State &aCPU)
{
	// NO-OP
}
JITCFlow ppc_opc_gen_dcbt(JITC &jitc)
{
	// NO-OP
	return flowContinue;
}
/*
 *	dcbtst		Data Cache Block Touch for Store
 *	.463
 */
void ppc_opc_dcbtst(PPC_CPU_State &aCPU)
{
	// NO-OP
}
JITCFlow ppc_opc_gen_dcbtst(JITC &jitc)
{
	// NO-OP
	return flowContinue;
}
/*
 *	eciwx		External Control In Word Indexed
 *	.474
 */
void ppc_opc_eciwx(PPC_CPU_State &aCPU)
{
	PPC_OPC_ERR("eciwx unimplemented.\n");
}
JITCFlow ppc_opc_gen_eciwx(JITC &jitc)
{
	PPC_OPC_ERR("eciwx unimplemented.\n");
	return flowContinue;
}
/*
 *	ecowx		External Control Out Word Indexed
 *	.476
 */
void ppc_opc_ecowx(PPC_CPU_State &aCPU)
{
	PPC_OPC_ERR("ecowx unimplemented.\n");
}
JITCFlow ppc_opc_gen_ecowx(JITC &jitc)
{
	PPC_OPC_ERR("ecowx unimplemented.\n");
	return flowContinue;
}
/*
 *	eieio		Enforce In-Order Execution of I/O
 *	.478
 */
void ppc_opc_eieio(PPC_CPU_State &aCPU)
{
	// NO-OP
}
JITCFlow ppc_opc_gen_eieio(JITC &jitc)
{
	// NO-OP
	return flowContinue;
}

/*
 *	icbi		Instruction Cache Block Invalidate
 *	.519
 */
void ppc_opc_icbi(PPC_CPU_State &aCPU)
{
	// FIXME: not a NOP with jitc
}
JITCFlow ppc_opc_gen_icbi(JITC &jitc)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
	jitc.flushAll();
	if (rA) {
		jitc.asmALU32(X86_MOV, RAX, curCPUreg(PPC_GPR(rA)));
		jitc.asmALU32(X86_ADD, RAX, curCPUreg(PPC_GPR(rB)));
	} else {
		jitc.getClientRegister(PPC_GPR(rB), NATIVE_REG | RAX);
	}
	jitc.asmALU32(X86_MOV, RSI, jitc.pc);
	jitc.asmCALL((NativeAddress)ppc_opc_icbi_asm);
	jitc.asmALU32(X86_MOV, RAX, jitc.pc+4);
	jitc.asmJMP((NativeAddress)ppc_new_pc_rel_asm);
	return flowEndBlockUnreachable;
}

/*
 *	isync		Instruction Synchronize
 *	.520
 */
void ppc_opc_isync(PPC_CPU_State &aCPU)
{
	// NO-OP
}
JITCFlow ppc_opc_gen_isync(JITC &jitc)
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
void ppc_opc_mcrf(PPC_CPU_State &aCPU)
{
	uint32 crD, crS, bla;
	PPC_OPC_TEMPL_X(aCPU.current_opc, crD, crS, bla);
	// FIXME: bla == 0
	crD>>=2;
	crS>>=2;
	crD = 7-crD;
	crS = 7-crS;
	uint32 c = (aCPU.cr>>(crS*4)) & 0xf;
	aCPU.cr &= ppc_cmp_and_mask[crD];
	aCPU.cr |= c<<(crD*4);
}
JITCFlow ppc_opc_gen_mcrf(JITC &jitc)
{
	ppc_opc_gen_interpret(jitc, ppc_opc_mcrf);
	return flowEndBlock;
}
/*
 *	mcrfs		Move to Condition Register from FPSCR
 *	.562
 */
void ppc_opc_mcrfs(PPC_CPU_State &aCPU)
{
	PPC_OPC_ERR("mcrfs unimplemented.\n");
}
JITCFlow ppc_opc_gen_mcrfs(JITC &jitc)
{
	PPC_OPC_ERR("mcrfs unimplemented.\n");
}
/*
 *	mcrxr		Move to Condition Register from XER
 *	.563
 */
void ppc_opc_mcrxr(PPC_CPU_State &aCPU)
{
	int crD, a, b;
	PPC_OPC_TEMPL_X(aCPU.current_opc, crD, a, b);
	crD >>= 2;
	crD = 7-crD;
	aCPU.cr &= ppc_cmp_and_mask[crD];
	aCPU.cr |= (((aCPU.xer & 0xf0000000) | (aCPU.xer_ca ? XER_CA : 0))>>28)<<(crD*4);
	aCPU.xer = ~0xf0000000;
	aCPU.xer_ca = 0;
}
JITCFlow ppc_opc_gen_mcrxr(JITC &jitc)
{
	ppc_opc_gen_interpret(jitc, ppc_opc_mcrxr);
	return flowEndBlock;
}

static void inline move_reg(JITC &jitc, PPC_Register creg1, PPC_Register creg2)
{
	NativeReg reg2 = jitc.getClientRegister(creg2);
	NativeReg reg1 = jitc.mapClientRegisterDirty(creg1);
	jitc.asmALU32(X86_MOV, reg1, reg2);
}

static void inline move_reg0(JITC &jitc, PPC_Register creg1)
{
	NativeReg reg1 = jitc.mapClientRegisterDirty(creg1);
	jitc.asmMOV32_NoFlags(reg1, 0);
}

/*
 *	mfcr		Move from Condition Register
 *	.564
 */
void ppc_opc_mfcr(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
	PPC_OPC_ASSERT(rA==0 && rB==0);
	aCPU.gpr[rD] = aCPU.cr;
}
JITCFlow ppc_opc_gen_mfcr(JITC &jitc)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
	jitc.clobberFlags();
	NativeReg d = jitc.mapClientRegisterDirty(PPC_GPR(rD));
	jitc.asmALU32(X86_MOV, d, curCPU(cr));
	return flowContinue;
}
/*
 *	mffs		Move from FPSCR
 *	.565
 */
void ppc_opc_mffsx(PPC_CPU_State &aCPU)
{
	int frD, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, frD, rA, rB);
	PPC_OPC_ASSERT(rA==0 && rB==0);
	aCPU.fpr[frD] = aCPU.fpscr;
	if (aCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_OPC_ERR("mffs. unimplemented.\n");
	}
}
JITCFlow ppc_opc_gen_mffsx(JITC &jitc)
{
	ppc_opc_gen_interpret(jitc, ppc_opc_mffsx);
	return flowEndBlock;
}

/*
 *	mfmsr		Move from Machine State Register
 *	.566
 */
void ppc_opc_mfmsr(PPC_CPU_State &aCPU)
{
	if (aCPU.msr & MSR_PR) {
//		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
		return;
	}
	int rD, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
	PPC_OPC_ASSERT((rA == 0) && (rB == 0));
	aCPU.gpr[rD] = aCPU.msr;
}
JITCFlow ppc_opc_gen_mfmsr(JITC &jitc)
{
	ppc_opc_gen_check_privilege(jitc);
	int rD, rA, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rD, rA, rB);
	move_reg(jitc, PPC_GPR(rD), PPC_MSR);
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
void ppc_opc_mfspr(PPC_CPU_State &aCPU)
{
	int rD, spr1, spr2;
	PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, spr1, spr2);
	switch (spr2) {
	case 0:
		switch (spr1) {
		case 1: aCPU.gpr[rD] = aCPU.xer | (aCPU.xer_ca ? XER_CA : 0); return;
		case 8: aCPU.gpr[rD] = aCPU.lr; return;
		case 9: aCPU.gpr[rD] = aCPU.ctr; return;
		}
	case 8:	// altivec makes this user visible
		if (spr1 == 0) {
			aCPU.gpr[rD] = aCPU.vrsave; 
			return;
		}
	}
	if (aCPU.msr & MSR_PR) {
//		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
		return;
	}
	switch (spr2) {
	case 0:
		switch (spr1) {
		case 18: aCPU.gpr[rD] = aCPU.dsisr; return;
		case 19: aCPU.gpr[rD] = aCPU.dar; return;
		case 22: aCPU.gpr[rD] = aCPU.dec; return;
		case 25: aCPU.gpr[rD] = aCPU.sdr1; return;
		case 26: aCPU.gpr[rD] = aCPU.srr[0]; return;
		case 27: aCPU.gpr[rD] = aCPU.srr[1]; return;
		}
		break;
	case 8:
		switch (spr1) {
		case 12: aCPU.gpr[rD] = ppc_get_cpu_timebase(); return;
		case 13: aCPU.gpr[rD] = ppc_get_cpu_timebase() >> 32; return;
		case 16: aCPU.gpr[rD] = aCPU.sprg[0]; return;
		case 17: aCPU.gpr[rD] = aCPU.sprg[1]; return;
		case 18: aCPU.gpr[rD] = aCPU.sprg[2]; return;
		case 19: aCPU.gpr[rD] = aCPU.sprg[3]; return;
		case 26: aCPU.gpr[rD] = aCPU.ear; return;
		case 31: aCPU.gpr[rD] = aCPU.pvr; return;
		}
		break;
	case 16:
		switch (spr1) {
		case 16: aCPU.gpr[rD] = aCPU.ibatu[0]; return;
		case 17: aCPU.gpr[rD] = aCPU.ibatl[0]; return;
		case 18: aCPU.gpr[rD] = aCPU.ibatu[1]; return;
		case 19: aCPU.gpr[rD] = aCPU.ibatl[1]; return;
		case 20: aCPU.gpr[rD] = aCPU.ibatu[2]; return;
		case 21: aCPU.gpr[rD] = aCPU.ibatl[2]; return;
		case 22: aCPU.gpr[rD] = aCPU.ibatu[3]; return;
		case 23: aCPU.gpr[rD] = aCPU.ibatl[3]; return;
		case 24: aCPU.gpr[rD] = aCPU.dbatu[0]; return;
		case 25: aCPU.gpr[rD] = aCPU.dbatl[0]; return;
		case 26: aCPU.gpr[rD] = aCPU.dbatu[1]; return;
		case 27: aCPU.gpr[rD] = aCPU.dbatl[1]; return;
		case 28: aCPU.gpr[rD] = aCPU.dbatu[2]; return;
		case 29: aCPU.gpr[rD] = aCPU.dbatl[2]; return;
		case 30: aCPU.gpr[rD] = aCPU.dbatu[3]; return;
		case 31: aCPU.gpr[rD] = aCPU.dbatl[3]; return;
		}
		break;
	case 29:
		switch (spr1) {
		case 16:
			aCPU.gpr[rD] = 0;
			return;
		case 17:
			aCPU.gpr[rD] = 0;
			return;
		case 18:
			aCPU.gpr[rD] = 0;
			return;
		case 24:
			aCPU.gpr[rD] = 0;
			return;
		case 25:
			aCPU.gpr[rD] = 0;
			return;
		case 26:
			aCPU.gpr[rD] = 0;
			return;
		case 28:
			aCPU.gpr[rD] = 0;
			return;
		case 29:
			aCPU.gpr[rD] = 0;
			return;
		case 30:
			aCPU.gpr[rD] = 0;
			return;
		}
	case 31:
		switch (spr1) {
		case 16:
//			PPC_OPC_WARN("read from spr %d:%d (HID0) not supported!\n", spr1, spr2);
			aCPU.gpr[rD] = aCPU.hid[0];
			return;
		case 17:
			PPC_OPC_WARN("read from spr %d:%d (HID1) not supported!\n", spr1, spr2);
			aCPU.gpr[rD] = aCPU.hid[1];
			return;
		case 22:
			aCPU.gpr[rD] = 0;
			return;
		case 23:
			aCPU.gpr[rD] = 0;
			return;
		case 25:
			PPC_OPC_WARN("read from spr %d:%d (L2CR) not supported! (from %08x)\n", spr1, spr2, aCPU.pc);
			aCPU.gpr[rD] = 0;
			return;
		case 27:
			PPC_OPC_WARN("read from spr %d:%d (ICTC) not supported!\n", spr1, spr2);
			aCPU.gpr[rD] = 0;
			return;
		case 28:
//			PPC_OPC_WARN("read from spr %d:%d (THRM1) not supported!\n", spr1, spr2);
			aCPU.gpr[rD] = 0;
			return;
		case 29:
//			PPC_OPC_WARN("read from spr %d:%d (THRM2) not supported!\n", spr1, spr2);
			aCPU.gpr[rD] = 0;
			return;
		case 30:
//			PPC_OPC_WARN("read from spr %d:%d (THRM3) not supported!\n", spr1, spr2);
			aCPU.gpr[rD] = 0;
			return;
		case 31:
//			PPC_OPC_WARN("read from spr %d:%d (???) not supported!\n", spr1, spr2);
			aCPU.gpr[rD] = 0;
			return;
		}
	}
	fprintf(stderr, "unknown mfspr: %i:%i\n", spr1, spr2);
	SINGLESTEP("invalid mfspr\n");
}

JITCFlow ppc_opc_gen_mfspr(JITC &jitc)
{
	int rD, spr1, spr2;
	PPC_OPC_TEMPL_X(jitc.current_opc, rD, spr1, spr2);
	switch (spr2) {
	case 0:
		switch (spr1) {
		case 1: {
			jitc.clobberFlags();
			jitc.getClientCarry();
			NativeReg reg2 = jitc.getClientRegister(PPC_XER);
			NativeReg reg1 = jitc.mapClientRegisterDirty(PPC_GPR(rD));
			jitc.asmALU32(X86_SBB, reg1, reg1);   // reg1 = CA ? -1 : 0
			jitc.asmALU32(X86_AND, reg1, XER_CA); // reg1 = CA ? XER_CA : 0
			jitc.asmALU32(X86_OR, reg1, reg2);
			jitc.clobberCarry();
			return flowContinue;
		}
		case 8: move_reg(jitc, PPC_GPR(rD), PPC_LR); return flowContinue;
		case 9: move_reg(jitc, PPC_GPR(rD), PPC_CTR); return flowContinue;
		}
	case 8:
		if (spr1 == 0) {
			move_reg(jitc, PPC_GPR(rD), PPC_VRSAVE); 
			return flowContinue;
		}
	}
	ppc_opc_gen_check_privilege(jitc);
	switch (spr2) {
	case 0:
		switch (spr1) {
		case 18: move_reg(jitc, PPC_GPR(rD), PPC_DSISR); return flowContinue;
		case 19: move_reg(jitc, PPC_GPR(rD), PPC_DAR); return flowContinue;
		case 22: {
			jitc.clobberAll();
			jitc.asmALU64(X86_LEA, RDI, curCPU(all));			
			jitc.asmCALL((NativeAddress)readDEC);
			move_reg(jitc, PPC_GPR(rD), PPC_DEC);
			return flowContinue;
		}
		case 25: move_reg(jitc, PPC_GPR(rD), PPC_SDR1); return flowContinue;
		case 26: move_reg(jitc, PPC_GPR(rD), PPC_SRR0); return flowContinue;
		case 27: move_reg(jitc, PPC_GPR(rD), PPC_SRR1); return flowContinue;
		}
		break;
	case 8:
		switch (spr1) {
		case 12: {
			jitc.clobberAll();
			jitc.asmCALL((NativeAddress)ppc_get_cpu_timebase);
			jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RAX);
			return flowContinue;
		}
		case 13: {
			jitc.clobberAll();
			jitc.asmCALL((NativeAddress)ppc_get_cpu_timebase);
			jitc.asmShift32(X86_SHR, RAX, 32);
			jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RAX);
			return flowContinue;
		}
		case 16: move_reg(jitc, PPC_GPR(rD), PPC_SPRG(0)); return flowContinue;
		case 17: move_reg(jitc, PPC_GPR(rD), PPC_SPRG(1)); return flowContinue;
		case 18: move_reg(jitc, PPC_GPR(rD), PPC_SPRG(2)); return flowContinue;
		case 19: move_reg(jitc, PPC_GPR(rD), PPC_SPRG(3)); return flowContinue;
		case 26: move_reg(jitc, PPC_GPR(rD), PPC_EAR); return flowContinue;
		case 31: move_reg(jitc, PPC_GPR(rD), PPC_PVR); return flowContinue;
		}
		break;
	case 16:
		switch (spr1) {
		case 16: move_reg(jitc, PPC_GPR(rD), PPC_IBATU(0)); return flowContinue;
		case 17: move_reg(jitc, PPC_GPR(rD), PPC_IBATL(0)); return flowContinue;
		case 18: move_reg(jitc, PPC_GPR(rD), PPC_IBATU(1)); return flowContinue;
		case 19: move_reg(jitc, PPC_GPR(rD), PPC_IBATL(1)); return flowContinue;
		case 20: move_reg(jitc, PPC_GPR(rD), PPC_IBATU(2)); return flowContinue;
		case 21: move_reg(jitc, PPC_GPR(rD), PPC_IBATL(2)); return flowContinue;
		case 22: move_reg(jitc, PPC_GPR(rD), PPC_IBATU(3)); return flowContinue;
		case 23: move_reg(jitc, PPC_GPR(rD), PPC_IBATL(3)); return flowContinue;
		case 24: move_reg(jitc, PPC_GPR(rD), PPC_DBATU(0)); return flowContinue;
		case 25: move_reg(jitc, PPC_GPR(rD), PPC_DBATL(0)); return flowContinue;
		case 26: move_reg(jitc, PPC_GPR(rD), PPC_DBATU(1)); return flowContinue;
		case 27: move_reg(jitc, PPC_GPR(rD), PPC_DBATL(1)); return flowContinue;
		case 28: move_reg(jitc, PPC_GPR(rD), PPC_DBATU(2)); return flowContinue;
		case 29: move_reg(jitc, PPC_GPR(rD), PPC_DBATL(2)); return flowContinue;
		case 30: move_reg(jitc, PPC_GPR(rD), PPC_DBATU(3)); return flowContinue;
		case 31: move_reg(jitc, PPC_GPR(rD), PPC_DBATL(3)); return flowContinue;
		}
		break;
	case 29:
		switch (spr1) {
		case 16: move_reg0(jitc, PPC_GPR(rD)); return flowContinue; //g4
		case 17: move_reg0(jitc, PPC_GPR(rD)); return flowContinue; //g4
		case 18: move_reg0(jitc, PPC_GPR(rD)); return flowContinue; //g4
		case 24: move_reg0(jitc, PPC_GPR(rD)); return flowContinue; //g4
		case 25: move_reg0(jitc, PPC_GPR(rD)); return flowContinue; //g4
		case 26: move_reg0(jitc, PPC_GPR(rD)); return flowContinue; //g4
		case 28: move_reg0(jitc, PPC_GPR(rD)); return flowContinue; //g4
		case 29: move_reg0(jitc, PPC_GPR(rD)); return flowContinue; //g4
		case 30: move_reg0(jitc, PPC_GPR(rD)); return flowContinue; //g4
		}
		break;
	case 31:
		switch (spr1) {
		case 16: move_reg(jitc, PPC_GPR(rD), PPC_HID0); return flowContinue;
		case 17: move_reg(jitc, PPC_GPR(rD), PPC_HID1); return flowContinue;
		case 22: move_reg0(jitc, PPC_GPR(rD)); return flowContinue; //g4
		case 23: move_reg0(jitc, PPC_GPR(rD)); return flowContinue; //g4
		case 25: move_reg0(jitc, PPC_GPR(rD)); return flowContinue;
		case 27: move_reg0(jitc, PPC_GPR(rD)); return flowContinue;
		case 28: move_reg0(jitc, PPC_GPR(rD)); return flowContinue;
		case 29: move_reg0(jitc, PPC_GPR(rD)); return flowContinue;
		case 30: move_reg0(jitc, PPC_GPR(rD)); return flowContinue;
		case 31: move_reg0(jitc, PPC_GPR(rD)); return flowContinue;
		}
	}
	move_reg0(jitc, PPC_GPR(rD));
	jitc.clobberAll();
	jitc.asmALU32(X86_MOV, RDI, curCPU(current_code_base));
	jitc.asmALU32(X86_ADD, RDI, jitc.pc);
	jitc.asmALU32(X86_MOV, RSI, spr1);
	jitc.asmALU32(X86_MOV, RDX, spr2);
	jitc.asmCALL((NativeAddress)unknown_spr_warning);
	return flowEndBlock;
}
/*
 *	mfsr		Move from Segment Register
 *	.570
 */
void ppc_opc_mfsr(PPC_CPU_State &aCPU)
{
	if (aCPU.msr & MSR_PR) {
//		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
		return;
	}
	int rD, SR, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rD, SR, rB);
	// FIXME: check insn
	aCPU.gpr[rD] = aCPU.sr[SR & 0xf];
}
JITCFlow ppc_opc_gen_mfsr(JITC &jitc)
{
	ppc_opc_gen_check_privilege(jitc);
	int rD, SR, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rD, SR, rB);
	move_reg(jitc, PPC_GPR(rD), PPC_SR(SR & 0xf));	
	return flowContinue;
}
/*
 *	mfsrin		Move from Segment Register Indirect
 *	.572
 */
void ppc_opc_mfsrin(PPC_CPU_State &aCPU)
{
	if (aCPU.msr & MSR_PR) {
//		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
		return;
	}
	int rD, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rD, rA, rB);
	// FIXME: check insn
	aCPU.gpr[rD] = aCPU.sr[aCPU.gpr[rB] >> 28];
}
JITCFlow ppc_opc_gen_mfsrin(JITC &jitc)
{
	ppc_opc_gen_check_privilege(jitc);
	int rD, SR, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rD, SR, rB);
	jitc.clobberCarryAndFlags();
	NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
	NativeReg d = jitc.mapClientRegisterDirty(PPC_GPR(rD));
	if (b != d) jitc.clobberRegister(NATIVE_REG | b);
	// no problem here if b==d 
	jitc.asmShift32(X86_SHR, b, 28);
	// mov d, [4*b+sr]
	jitc.asmALU32(X86_MOV, d, curCPUsib(sr, 4, b));
	return flowContinue;
}
/*
 *	mftb		Move from Time Base
 *	.574
 */
void ppc_opc_mftb(PPC_CPU_State &aCPU)
{
	int rD, spr1, spr2;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rD, spr1, spr2);
	switch (spr2) {
	case 8:
		switch (spr1) {
		case 12: aCPU.gpr[rD] = ppc_get_cpu_timebase(); return;
		case 13: aCPU.gpr[rD] = ppc_get_cpu_timebase() >> 32; return;
/*		case 12: aCPU.gpr[rD] = aCPU.tb; return;
		case 13: aCPU.gpr[rD] = aCPU.tb >> 32; return;*/
		}
		break;
	}
	SINGLESTEP("unknown mftb\n");
}
JITCFlow ppc_opc_gen_mftb(JITC &jitc)
{
	int rD, spr1, spr2;
	PPC_OPC_TEMPL_X(jitc.current_opc, rD, spr1, spr2);
	switch (spr2) {
	case 8:
		switch (spr1) {
		case 12:
			jitc.clobberAll();
			jitc.asmCALL((NativeAddress)ppc_get_cpu_timebase);
			jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RAX);
			return flowContinue;
							
		case 13:
			jitc.clobberAll();
			jitc.asmCALL((NativeAddress)ppc_get_cpu_timebase);
			jitc.asmShift64(X86_SHR, RAX, 32);
			jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RAX);
			return flowContinue;
		}
		break;
	}
	move_reg0(jitc, PPC_GPR(rD));
	jitc.clobberAll();
	jitc.asmALU32(X86_MOV, RDI, curCPU(current_code_base));
	jitc.asmALU32(X86_ADD, RDI, jitc.pc);
	jitc.asmALU32(X86_MOV, RSI, spr1);
	jitc.asmALU32(X86_MOV, RDX, spr2);
	jitc.asmCALL((NativeAddress)unknown_tbr_warning);
	return flowEndBlock;
}
/*
 *	mtcrf		Move to Condition Register Fields
 *	.576
 */
void ppc_opc_mtcrf(PPC_CPU_State &aCPU)
{
	int rS;
	uint32 crm;
	uint32 CRM;
	PPC_OPC_TEMPL_XFX(aCPU.current_opc, rS, crm);
	CRM = ((crm&0x80)?0xf0000000:0)|((crm&0x40)?0x0f000000:0)|((crm&0x20)?0x00f00000:0)|((crm&0x10)?0x000f0000:0)|
	      ((crm&0x08)?0x0000f000:0)|((crm&0x04)?0x00000f00:0)|((crm&0x02)?0x000000f0:0)|((crm&0x01)?0x0000000f:0);
	aCPU.cr = (aCPU.gpr[rS] & CRM) | (aCPU.cr & ~CRM);
}
JITCFlow ppc_opc_gen_mtcrf(JITC &jitc)
{
	int rS;
	uint32 crm;
	uint32 CRM;
	PPC_OPC_TEMPL_XFX(jitc.current_opc, rS, crm);
	CRM = ((crm&0x80)?0xf0000000:0)|((crm&0x40)?0x0f000000:0)|((crm&0x20)?0x00f00000:0)|((crm&0x10)?0x000f0000:0)|
	      ((crm&0x08)?0x0000f000:0)|((crm&0x04)?0x00000f00:0)|((crm&0x02)?0x000000f0:0)|((crm&0x01)?0x0000000f:0);
	jitc.clobberCarryAndFlags();
	NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
	jitc.clobberRegister(NATIVE_REG | s);
	jitc.asmALU32(X86_AND, s, CRM);
	jitc.asmALU32(X86_AND, curCPU(cr), ~CRM);
	jitc.asmALU32(X86_OR, curCPU(cr), s);
	return flowContinue;
}
/*
 *	mtfsb0x		Move to FPSCR Bit 0
 *	.577
 */
void ppc_opc_mtfsb0x(PPC_CPU_State &aCPU)
{
	int crbD, n1, n2;
	PPC_OPC_TEMPL_X(aCPU.current_opc, crbD, n1, n2);
	if (crbD != 1 && crbD != 2) {
		aCPU.fpscr &= ~(1<<(31-crbD));
	}
	if (aCPU.current_opc & PPC_OPC_Rc) {
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

static void ppc_opc_set_fpscr_roundmode(JITC &jitc, NativeReg r)
{
	jitc.asmALU32(X86_AND, r, 3); // RC
	jitc.asmALU32(X86_AND, curCPU(x87cw), ~0x0c00);
//	jitc.asmALU32(X86_MOV, r, REG_NO, 4, r, (uint32)&ppc_to_x86_roundmode));
	jitc.asmALU32(X86_OR, curCPU(x87cw), r);
///	asmFLDCWMem(modrm, x86_mem(modrm, REG_NO, (uint32)&aCPU.x87cw));
// FIXME64
}

JITCFlow ppc_opc_gen_mtfsb0x(JITC &jitc)
{
	int crbD, n1, n2;
	PPC_OPC_TEMPL_X(jitc.current_opc, crbD, n1, n2);
	if (crbD != 1 && crbD != 2) {
		jitc.getClientRegister(PPC_FPSCR, NATIVE_REG | RAX);
		jitc.clobberAll();
		jitc.asmALU32(X86_AND, RAX, ~(1<<(31-crbD)));
		jitc.asmALU32(X86_MOV, curCPU(fpscr), RAX);
		if (crbD == 30 || crbD == 31) {
			ppc_opc_set_fpscr_roundmode(jitc, RAX);
		}
	}
	return flowContinue;
}
/*
 *	mtfsb1x		Move to FPSCR Bit 1
 *	.578
 */
void ppc_opc_mtfsb1x(PPC_CPU_State &aCPU)
{
	int crbD, n1, n2;
	PPC_OPC_TEMPL_X(aCPU.current_opc, crbD, n1, n2);
	if (crbD != 1 && crbD != 2) {
		aCPU.fpscr |= 1<<(31-crbD);
	}
	if (aCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_OPC_ERR("mtfsb1. unimplemented.\n");
	}
}
/*JITCFlow ppc_opc_gen_mtfsb1x()
{
	ppc_opc_gen_interpret(ppc_opc_mtfsb1x);
	return flowEndBlock;
}*/
JITCFlow ppc_opc_gen_mtfsb1x(JITC &jitc)
{
	int crbD, n1, n2;
	PPC_OPC_TEMPL_X(jitc.current_opc, crbD, n1, n2);
	if (crbD != 1 && crbD != 2) {
		jitc.getClientRegister(PPC_FPSCR, NATIVE_REG | RAX);
		jitc.clobberAll();
		jitc.asmALU32(X86_OR, RAX, 1<<(31-crbD));
		jitc.asmALU32(X86_MOV, curCPU(fpscr), RAX);
		if (crbD == 30 || crbD == 31) {
			ppc_opc_set_fpscr_roundmode(jitc, RAX);
		}
	}
	return flowContinue;
}
/*
 *	mtfsfx		Move to FPSCR Fields
 *	.579
 */
void ppc_opc_mtfsfx(PPC_CPU_State &aCPU)
{
	int frB;
	uint32 fm, FM;
	PPC_OPC_TEMPL_XFL(aCPU.current_opc, frB, fm);
	FM = ((fm&0x80)?0xf0000000:0)|((fm&0x40)?0x0f000000:0)|((fm&0x20)?0x00f00000:0)|((fm&0x10)?0x000f0000:0)|
	     ((fm&0x08)?0x0000f000:0)|((fm&0x04)?0x00000f00:0)|((fm&0x02)?0x000000f0:0)|((fm&0x01)?0x0000000f:0);
	aCPU.fpscr = (aCPU.fpr[frB] & FM) | (aCPU.fpscr & ~FM);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_OPC_ERR("mtfsf. unimplemented.\n");
	}
}
/*JITCFlow ppc_opc_gen_mtfsfx()
{
	ppc_opc_gen_interpret(ppc_opc_mtfsfx);
	return flowEndBlock;
}*/
JITCFlow ppc_opc_gen_mtfsfx(JITC &jitc)
{
	int frB;
	uint32 fm, FM;
	PPC_OPC_TEMPL_XFL(jitc.current_opc, frB, fm);
	FM = ((fm&0x80)?0xf0000000:0)|((fm&0x40)?0x0f000000:0)|((fm&0x20)?0x00f00000:0)|((fm&0x10)?0x000f0000:0)|
	     ((fm&0x08)?0x0000f000:0)|((fm&0x04)?0x00000f00:0)|((fm&0x02)?0x000000f0:0)|((fm&0x01)?0x0000000f:0);
	     
	NativeReg fpscr = jitc.getClientRegister(PPC_FPSCR);
	NativeReg b = jitc.getClientRegister(PPC_FPR(frB));
	jitc.clobberAll();
	jitc.asmALU32(X86_AND, b, FM);
	jitc.asmALU32(X86_AND, fpscr, ~FM);
	jitc.asmALU32(X86_OR, fpscr, b);
	if (fm & 1) {
		jitc.asmALU32(X86_MOV, curCPU(fpscr), fpscr);
		ppc_opc_set_fpscr_roundmode(jitc, fpscr);
	} else {
		jitc.mapClientRegisterDirty(PPC_FPSCR, NATIVE_REG | fpscr);
	}
	if (jitc.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_OPC_ERR("mtfsf. unimplemented.\n");
	}
	return flowContinue;
}
/*
 *	mtfsfix		Move to FPSCR Field Immediate
 *	.580
 */
void ppc_opc_mtfsfix(PPC_CPU_State &aCPU)
{
	int crfD, n1;
	uint32 imm;
	PPC_OPC_TEMPL_X(aCPU.current_opc, crfD, n1, imm);
	crfD >>= 2;
	imm >>= 1;
	crfD = 7-crfD;
	aCPU.fpscr &= ppc_cmp_and_mask[crfD];
	aCPU.fpscr |= imm<<(crfD*4);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_OPC_ERR("mtfsfi. unimplemented.\n");
	}
}
/*JITCFlow ppc_opc_gen_mtfsfix()
{
	ppc_opc_gen_interpret(ppc_opc_mtfsfix);
	return flowEndBlock;
}*/
JITCFlow ppc_opc_gen_mtfsfix(JITC &jitc)
{
	int crfD, n1;
	uint32 imm;
	PPC_OPC_TEMPL_X(jitc.current_opc, crfD, n1, imm);
	crfD >>= 2;
	imm >>= 1;
	crfD = 7-crfD;
	NativeReg fpscr = jitc.getClientRegister(PPC_FPSCR);
	jitc.clobberAll();
	jitc.asmALU32(X86_AND, fpscr, ppc_cmp_and_mask[crfD]);
	jitc.asmALU32(X86_OR, fpscr, imm<<(crfD*4));
	if (crfD == 0) {
		jitc.asmALU32(X86_MOV, curCPU(fpscr), fpscr);
		ppc_opc_set_fpscr_roundmode(jitc, fpscr);
	} else {
		jitc.mapClientRegisterDirty(PPC_FPSCR, NATIVE_REG | fpscr);
	}
	if (jitc.current_opc & PPC_OPC_Rc) {
		// update cr1 flags
		PPC_OPC_ERR("mtfsfi. unimplemented.\n");
	}
	return flowContinue;
}
/*
 *	mtmsr		Move to Machine State Register
 *	.581
 */
void ppc_opc_mtmsr(PPC_CPU_State &aCPU)
{
	if (aCPU.msr & MSR_PR) {
//		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
		return;
	}
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	PPC_OPC_ASSERT((rA == 0) && (rB == 0));
	ppc_set_msr(aCPU, aCPU.gpr[rS]);
}
JITCFlow ppc_opc_gen_mtmsr(JITC &jitc)
{
	jitc.clobberCarryAndFlags();
	jitc.flushRegister();
	ppc_opc_gen_check_privilege(jitc);
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
	jitc.getClientRegister(PPC_GPR(rS), NATIVE_REG | RAX);
	jitc.clobberAll();
	jitc.asmALU64(X86_LEA, RDI, curCPU(all));
	jitc.asmCALL((NativeAddress)ppc_set_msr_asm);
	return flowContinue;
}


static inline void ppc_opc_batu_helper(PPC_CPU_State &aCPU, bool dbat, int idx)
{
	if (dbat) {
		aCPU.dbat_bl[idx] = ((~aCPU.dbatu[idx] << 15) & 0xfffe0000);
		aCPU.dbat_nbl[idx] = ~aCPU.dbat_bl[idx];
		aCPU.dbat_bepi[idx] = (aCPU.dbatu[idx] & aCPU.dbat_bl[idx]);
	} else {
		aCPU.ibat_bl[idx] = ((~aCPU.ibatu[idx] << 15) & 0xfffe0000);
		aCPU.ibat_bepi[idx] = (aCPU.ibatu[idx] & aCPU.ibat_bl[idx]);
	}
}

static inline void ppc_opc_batl_helper(PPC_CPU_State &aCPU, bool dbat, int idx)
{
	if (dbat) {
		aCPU.dbat_brpn[idx] = (aCPU.dbatl[idx] & aCPU.dbat_bl[idx]);
	} else {
		aCPU.ibat_brpn[idx] = (aCPU.ibatl[idx] & aCPU.ibat_bl[idx]);
	}
}

/*
 *	mtspr		Move to Special-Purpose Register
 *	.584
 */
void ppc_opc_mtspr(PPC_CPU_State &aCPU)
{
	int rS, spr1, spr2;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, spr1, spr2);
	switch (spr2) {
	case 0:
		switch (spr1) {
		case 1:
			aCPU.xer = aCPU.gpr[rS] & ~XER_CA;
			aCPU.xer_ca = !!(aCPU.gpr[rS] & XER_CA);
			return;
		case 8:	aCPU.lr = aCPU.gpr[rS]; return;
		case 9:	aCPU.ctr = aCPU.gpr[rS]; return;
		}
	case 8:
		if (spr1 == 0) {
			aCPU.vrsave = aCPU.gpr[rS]; 
			return;
		}
	}
	if (aCPU.msr & MSR_PR) {
//		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
		return;
	}
	switch (spr2) {
	case 0:
		switch (spr1) {
/*		case 18: aCPU.gpr[rD] = aCPU.dsisr; return;
		case 19: aCPU.gpr[rD] = aCPU.dar; return;*/
		case 22: {
			writeDEC(aCPU, aCPU.gpr[rS]);
			return;
		}
		case 25: 
			if (!ppc_mmu_set_sdr1(aCPU, aCPU.gpr[rS], true)) {
				PPC_OPC_ERR("cannot set sdr1\n");
			}
			return;
		case 26: aCPU.srr[0] = aCPU.gpr[rS]; return;
		case 27: aCPU.srr[1] = aCPU.gpr[rS]; return;
		}
		break;
	case 8:
		switch (spr1) {
		case 16: aCPU.sprg[0] = aCPU.gpr[rS]; return;
		case 17: aCPU.sprg[1] = aCPU.gpr[rS]; return;
		case 18: aCPU.sprg[2] = aCPU.gpr[rS]; return;
		case 19: aCPU.sprg[3] = aCPU.gpr[rS]; return;
		case 28: writeTBL(aCPU, aCPU.gpr[rS]); return;
		case 29: writeTBU(aCPU, aCPU.gpr[rS]); return;
/*		case 26: aCPU.gpr[rD] = aCPU.ear; return;
		case 31: aCPU.gpr[rD] = aCPU.pvr; return;*/
		}
		break;
	case 16:
		switch (spr1) {
		case 16:
			aCPU.ibatu[0] = aCPU.gpr[rS];
			ppc_opc_batu_helper(aCPU, false, 0);
			return;
		case 17:
			aCPU.ibatl[0] = aCPU.gpr[rS];
			ppc_opc_batl_helper(aCPU, false, 0);
			return;
		case 18:
			aCPU.ibatu[1] = aCPU.gpr[rS];
			ppc_opc_batu_helper(aCPU, false, 1);
			return;
		case 19:
			aCPU.ibatl[1] = aCPU.gpr[rS];
			ppc_opc_batl_helper(aCPU, false, 1);
			return;
		case 20:
			aCPU.ibatu[2] = aCPU.gpr[rS];
			ppc_opc_batu_helper(aCPU, false, 2);
			return;
		case 21:
			aCPU.ibatl[2] = aCPU.gpr[rS];
			ppc_opc_batl_helper(aCPU, false, 2);
			return;
		case 22:
			aCPU.ibatu[3] = aCPU.gpr[rS];
			ppc_opc_batu_helper(aCPU, false, 3);
			return;
		case 23:
			aCPU.ibatl[3] = aCPU.gpr[rS];
			ppc_opc_batl_helper(aCPU, false, 3);
			return;
		case 24:
			aCPU.dbatu[0] = aCPU.gpr[rS];
			ppc_opc_batu_helper(aCPU, true, 0);
			return;
		case 25:
			aCPU.dbatl[0] = aCPU.gpr[rS];
			ppc_opc_batl_helper(aCPU, true, 0);
			return;
		case 26:
			aCPU.dbatu[1] = aCPU.gpr[rS];
			ppc_opc_batu_helper(aCPU, true, 1);
			return;
		case 27:
			aCPU.dbatl[1] = aCPU.gpr[rS];
			ppc_opc_batl_helper(aCPU, true, 1);
			return;
		case 28:
			aCPU.dbatu[2] = aCPU.gpr[rS];
			ppc_opc_batu_helper(aCPU, true, 2);
			return;
		case 29:
			aCPU.dbatl[2] = aCPU.gpr[rS];
			ppc_opc_batl_helper(aCPU, true, 2);
			return;
		case 30:
			aCPU.dbatu[3] = aCPU.gpr[rS];
			ppc_opc_batu_helper(aCPU, true, 3);
			return;
		case 31:
			aCPU.dbatl[3] = aCPU.gpr[rS];
			ppc_opc_batl_helper(aCPU, true, 3);
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
//			PPC_OPC_WARN("write(%08x) to spr %d:%d (HID0) not supported! @%08x\n", aCPU.gpr[rS], spr1, spr2, aCPU.pc);
			aCPU.hid[0] = aCPU.gpr[rS];
			return;
		case 17:
			PPC_OPC_WARN("write(%08x) to spr %d:%d (HID1) not supported! @%08x\n", aCPU.gpr[rS], spr1, spr2, aCPU.pc);
			aCPU.hid[1] = aCPU.gpr[rS];
			return;
		case 18:
			PPC_OPC_ERR("write(%08x) to spr %d:%d (IABR) not supported!\n", aCPU.gpr[rS], spr1, spr2);
			return;
		case 21:
			PPC_OPC_ERR("write(%08x) to spr %d:%d (DABR) not supported!\n", aCPU.gpr[rS], spr1, spr2);
			return;
		case 22:
			PPC_OPC_ERR("write(%08x) to spr %d:%d (?) not supported!\n", aCPU.gpr[rS], spr1, spr2);
			return;
		case 23:
			PPC_OPC_ERR("write(%08x) to spr %d:%d (?) not supported!\n", aCPU.gpr[rS], spr1, spr2);
			return;
		case 27:
			PPC_OPC_WARN("write(%08x) to spr %d:%d (ICTC) not supported!\n", aCPU.gpr[rS], spr1, spr2);
			return;
		case 28:
//			PPC_OPC_WARN("write(%08x) to spr %d:%d (THRM1) not supported!\n", aCPU.gpr[rS], spr1, spr2);
			return;
		case 29:
//			PPC_OPC_WARN("write(%08x) to spr %d:%d (THRM2) not supported!\n", aCPU.gpr[rS], spr1, spr2);
			return;
		case 30:
//			PPC_OPC_WARN("write(%08x) to spr %d:%d (THRM3) not supported!\n", aCPU.gpr[rS], spr1, spr2);
			return;
		case 31: 
			return;
		}
	}
	fprintf(stderr, "unknown mtspr: %i:%i\n", spr1, spr2);
	SINGLESTEP("unknown mtspr\n");
}

static void FASTCALL ppc_mmu_set_sdr1_check_error(PPC_CPU_State &aCPU, uint32 newsdr1)
{
	if (!ppc_mmu_set_sdr1(aCPU, newsdr1, true)) {
		PPC_OPC_ERR("cannot set sdr1\n");
	}
}

static inline void ppc_opc_gen_batu_helper(JITC &jitc, bool dbat, int idx)
{
	NativeReg reg = jitc.getClientRegister(dbat ? PPC_DBATU(idx) : PPC_IBATU(idx));
	NativeReg tmp = jitc.allocRegister();

	jitc.clobberCarryAndFlags();
	jitc.clobberRegister(NATIVE_REG | reg);

	jitc.asmALU32(X86_MOV, tmp, reg);

	jitc.asmALU32(X86_NOT, reg);
	jitc.asmShift32(X86_SHL, reg, 15);
	jitc.asmALU32(X86_AND, reg, 0xfffe0000);
	if (dbat) {
		jitc.asmALU32(X86_MOV, curCPU(dbat_bl) + 4*idx, reg);
		jitc.asmALU32(X86_AND, tmp, reg);
		jitc.asmALU32(X86_MOV, curCPU(dbat_bepi) + 4*idx, tmp);

		jitc.asmALU32(X86_MOV, tmp, curCPU(dbatl) + 4*idx);
		jitc.asmALU32(X86_AND, tmp, reg);
		jitc.asmALU32(X86_MOV, curCPU(dbat_brpn) + 4*idx, tmp);

		jitc.asmALU32(X86_NOT, reg);
		jitc.asmALU32(X86_MOV, curCPU(dbat_nbl) + 4*idx, reg);
	} else {
		jitc.asmALU32(X86_MOV, curCPU(ibat_bl) + 4*idx, reg);
		jitc.asmALU32(X86_AND, tmp, reg);
		jitc.asmALU32(X86_MOV, curCPU(ibat_bepi) + 4*idx, tmp);

		jitc.asmALU32(X86_MOV, tmp, curCPU(ibatl) + 4*idx);
		jitc.asmALU32(X86_AND, tmp, reg);
		jitc.asmALU32(X86_MOV, curCPU(ibat_brpn) + 4*idx, tmp);

		jitc.asmALU32(X86_NOT, reg);
		jitc.asmALU32(X86_MOV, curCPU(ibat_nbl) + 4*idx, reg);
	}
}

static inline void ppc_opc_gen_batl_helper(JITC &jitc, bool dbat, int idx)
{
	NativeReg reg = jitc.getClientRegister(dbat ? PPC_DBATL(idx) : PPC_IBATL(idx));

	jitc.clobberCarryAndFlags();
	jitc.clobberRegister(NATIVE_REG | reg);

	if (dbat) {
		jitc.asmALU32(X86_AND, reg, curCPU(dbat_bl) + 4*idx);
		jitc.asmALU32(X86_MOV, curCPU(dbat_brpn) + 4*idx, reg);
	} else {
		jitc.asmALU32(X86_AND, reg, curCPU(ibat_bl) + 4*idx);
		jitc.asmALU32(X86_MOV, curCPU(ibat_brpn) + 4*idx, reg);
	}
}


JITCFlow ppc_opc_gen_mtspr(JITC &jitc)
{
	int rS, spr1, spr2;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, spr1, spr2);
	switch (spr2) {
	case 0:
		switch (spr1) {
		case 1: {
			jitc.clobberFlags();
			NativeReg reg2 = jitc.getClientRegister(PPC_GPR(rS));
			NativeReg reg1 = jitc.mapClientRegisterDirty(PPC_XER);
			jitc.asmALU32(X86_MOV, reg1, reg2);
			jitc.asmALU32(X86_AND, reg1, ~XER_CA);
			jitc.asmBTx32(X86_BT, reg2, 29);
			jitc.mapCarryDirty();
			return flowContinue;
		}
		case 8:	move_reg(jitc, PPC_LR, PPC_GPR(rS)); return flowContinue;
		case 9:	move_reg(jitc, PPC_CTR, PPC_GPR(rS)); return flowContinue;
		}
	case 8:
		if (spr1 == 0) {
			move_reg(jitc, PPC_VRSAVE, PPC_GPR(rS)); 
			return flowContinue;
		}
	}
	ppc_opc_gen_check_privilege(jitc);
	switch (spr2) {
	case 0:
		switch (spr1) {
		case 22: {
			jitc.getClientRegister(PPC_GPR(rS), NATIVE_REG | RSI);
			jitc.clobberAll();
			jitc.asmALU64(X86_LEA, RDI, curCPU(all));
			jitc.asmCALL((NativeAddress)writeDEC);
			return flowContinue;
		}
		case 25: {
			jitc.getClientRegister(PPC_GPR(rS), NATIVE_REG | RSI);
			jitc.clobberAll();
			jitc.asmALU64(X86_LEA, RDI, curCPU(all));
			jitc.asmCALL((NativeAddress)ppc_mmu_set_sdr1_check_error);
			jitc.asmALU32(X86_LEA, RDI, curCPU(all));
			jitc.asmALU32(X86_MOV, RAX, jitc.pc+4);
			jitc.asmJMP((NativeAddress)ppc_new_pc_rel_asm);
			return flowEndBlockUnreachable;
		}
		case 26: move_reg(jitc, PPC_SRR0, PPC_GPR(rS)); return flowContinue;
		case 27: move_reg(jitc, PPC_SRR1, PPC_GPR(rS)); return flowContinue;
		}
		break;
	case 8:
		switch (spr1) {
		case 16: move_reg(jitc, PPC_SPRG(0), PPC_GPR(rS)); return flowContinue;
		case 17: move_reg(jitc, PPC_SPRG(1), PPC_GPR(rS)); return flowContinue;
		case 18: move_reg(jitc, PPC_SPRG(2), PPC_GPR(rS)); return flowContinue;
		case 19: move_reg(jitc, PPC_SPRG(3), PPC_GPR(rS)); return flowContinue;
		case 28:
			jitc.getClientRegister(PPC_GPR(rS), NATIVE_REG | RSI);
			jitc.clobberAll();
			jitc.asmALU64(X86_LEA, RDI, curCPU(all));
			jitc.asmCALL((NativeAddress)writeTBL);
			return flowContinue;
		case 29:
			jitc.getClientRegister(PPC_GPR(rS), NATIVE_REG | RSI);
			jitc.clobberAll();
			jitc.asmALU64(X86_LEA, RDI, curCPU(all));
			jitc.asmCALL((NativeAddress)writeTBU);
			return flowContinue;
		}
		break;
	case 16: {
		switch (spr1) {
		case 16:
			move_reg(jitc, PPC_IBATU(0), PPC_GPR(rS));
			ppc_opc_gen_batu_helper(jitc, false, 0);
			break;
		case 17:
			move_reg(jitc, PPC_IBATL(0), PPC_GPR(rS));
			ppc_opc_gen_batl_helper(jitc, false, 0);
			break;
		case 18:
			move_reg(jitc, PPC_IBATU(1), PPC_GPR(rS));
			ppc_opc_gen_batu_helper(jitc, false, 1);
			break;
		case 19:
			move_reg(jitc, PPC_IBATL(1), PPC_GPR(rS));
			ppc_opc_gen_batl_helper(jitc, false, 1);
			break;
		case 20:
			move_reg(jitc, PPC_IBATU(2), PPC_GPR(rS));
			ppc_opc_gen_batu_helper(jitc, false, 2);
			break;
		case 21:
			move_reg(jitc, PPC_IBATL(2), PPC_GPR(rS));
			ppc_opc_gen_batl_helper(jitc, false, 2);
			break;
		case 22:
			move_reg(jitc, PPC_IBATU(3), PPC_GPR(rS));
			ppc_opc_gen_batu_helper(jitc, false, 3);
			break;
		case 23:
			move_reg(jitc, PPC_IBATL(3), PPC_GPR(rS));
			ppc_opc_gen_batl_helper(jitc, false, 3);
			break;
		case 24:
			move_reg(jitc, PPC_DBATU(0), PPC_GPR(rS));
			ppc_opc_gen_batu_helper(jitc, true, 0);
			break;
		case 25:
			move_reg(jitc, PPC_DBATL(0), PPC_GPR(rS));
			ppc_opc_gen_batl_helper(jitc, true, 0);
			break;
		case 26:
			move_reg(jitc, PPC_DBATU(1), PPC_GPR(rS));
			ppc_opc_gen_batu_helper(jitc, true, 1);
			break;
		case 27:
			move_reg(jitc, PPC_DBATL(1), PPC_GPR(rS));
			ppc_opc_gen_batl_helper(jitc, true, 1);
			break;
		case 28:
			move_reg(jitc, PPC_DBATU(2), PPC_GPR(rS));
			ppc_opc_gen_batu_helper(jitc, true, 2);
			break;
		case 29:
			move_reg(jitc, PPC_DBATL(2), PPC_GPR(rS));
			ppc_opc_gen_batl_helper(jitc, true, 2);
			break;
		case 30:
			move_reg(jitc, PPC_DBATU(3), PPC_GPR(rS));
			ppc_opc_gen_batu_helper(jitc, true, 3);
			break;
		case 31:
			move_reg(jitc, PPC_DBATL(3), PPC_GPR(rS));
			ppc_opc_gen_batl_helper(jitc, true, 3);
			break;
		default: goto invalid;
		}
		jitc.clobberAll();
		jitc.asmALU64(X86_LEA, RDI, curCPU(all));
		jitc.asmCALL((NativeAddress)ppc_mmu_tlb_invalidate_all_asm);
		jitc.asmALU32(X86_MOV, RAX, jitc.pc+4);
		jitc.asmJMP((NativeAddress)ppc_new_pc_rel_asm);
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
		case 16: move_reg(jitc, PPC_HID0, PPC_GPR(rS)); return flowContinue;
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
	jitc.clobberAll();
	jitc.asmALU32(X86_MOV, RDI, curCPU(current_code_base));
	jitc.asmALU32(X86_ADD, RDI, jitc.pc);
	jitc.asmALU32(X86_MOV, RSI, spr1);
	jitc.asmALU32(X86_MOV, RDX, spr2);
	jitc.asmCALL((NativeAddress)unknown_spr_warning);
	return flowEndBlock;
}
/*
 *	mtsr		Move to Segment Register
 *	.587
 */
void ppc_opc_mtsr(PPC_CPU_State &aCPU)
{
	if (aCPU.msr & MSR_PR) {
//		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
		return;
	}
	int rS, SR, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, SR, rB);
	// FIXME: check insn
	aCPU.sr[SR & 0xf] = aCPU.gpr[rS];
}
JITCFlow ppc_opc_gen_mtsr(JITC &jitc)
{
	jitc.flushRegister();
	ppc_opc_gen_check_privilege(jitc);
	int rS, SR, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, SR, rB);
	// FIXME: check insn
	move_reg(jitc, PPC_SR(SR & 0xf), PPC_GPR(rS));
	jitc.clobberAll();
	jitc.asmALU64(X86_LEA, RDI, curCPU(all));
	jitc.asmCALL((NativeAddress)ppc_mmu_tlb_invalidate_all_asm);
	// sync
//	jitc.asmALU32(X86_MOV, EAX, jitc.pc+4);
//	jitc.asmJMP((NativeAddress)ppc_new_pc_rel_asm);
//	return flowEndBlockUnreachable;	
	return flowContinue;
}
/*
 *	mtsrin		Move to Segment Register Indirect
 *	.591
 */
void ppc_opc_mtsrin(PPC_CPU_State &aCPU)
{
	if (aCPU.msr & MSR_PR) {
//		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
		return;
	}
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	// FIXME: check insn
	aCPU.sr[aCPU.gpr[rB] >> 28] = aCPU.gpr[rS];
}
JITCFlow ppc_opc_gen_mtsrin(JITC &jitc)
{
	jitc.flushRegister();
	ppc_opc_gen_check_privilege(jitc);
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
	// FIXME: check insn
	NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
	NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
	if (b == s) {
		s = jitc.allocRegister();
		jitc.asmALU32(X86_MOV, s, b);
	}
	jitc.clobberAll();
	jitc.asmShift32(X86_SHR, b, 28);
	// mov [4*b+sr], s
	jitc.asmALU32(X86_MOV, curCPUsib(sr, 4, b), s);
	jitc.asmALU64(X86_LEA, RDI, curCPU(all));
	jitc.asmCALL((NativeAddress)ppc_mmu_tlb_invalidate_all_asm);
	// sync
//	jitc.asmALU32(X86_MOV, EAX, jitc.pc+4);
//	jitc.asmJMP((NativeAddress)ppc_new_pc_rel_asm);
//	return flowEndBlockUnreachable;	
	return flowContinue;
}

/*
 *	rfi		Return from Interrupt
 *	.607
 */
void ppc_opc_rfi(PPC_CPU_State &aCPU)
{
	if (aCPU.msr & MSR_PR) {
//		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
		return;
	}
	ppc_set_msr(aCPU, aCPU.srr[1] & MSR_RFI_SAVE_MASK);
	aCPU.npc = aCPU.srr[0] & 0xfffffffc;
}
JITCFlow ppc_opc_gen_rfi(JITC &jitc)
{
	jitc.clobberCarryAndFlags();
	jitc.flushRegister();
	ppc_opc_gen_check_privilege(jitc);
	jitc.getClientRegister(PPC_SRR1, NATIVE_REG | RAX);
	jitc.asmALU64(X86_LEA, RDI, curCPU(all));
	jitc.asmALU32(X86_AND, RAX, MSR_RFI_SAVE_MASK);
	jitc.asmCALL((NativeAddress)ppc_set_msr_asm);
	jitc.asmALU32(X86_MOV, RAX, curCPUreg(PPC_SRR0));
	jitc.asmALU32(X86_AND, RAX, 0xfffffffc);
	jitc.asmJMP((NativeAddress)ppc_new_pc_asm);
	return flowEndBlockUnreachable;
}

/*
 *	sc		System Call
 *	.621
 */
#include "io/graphic/gcard.h"
void ppc_opc_sc(PPC_CPU_State &aCPU)
{
	if (aCPU.gpr[3] == 0x113724fa && aCPU.gpr[4] == 0x77810f9b) {
		gcard_osi(0);
		return;
	}
//	ppc_exception(PPC_EXC_SC);
}
JITCFlow ppc_opc_gen_sc(JITC &jitc)
{
	jitc.clobberCarryAndFlags();
	jitc.flushRegister();	
	
	NativeReg r1 = jitc.getClientRegister(PPC_GPR(3));
	jitc.asmALU32(X86_CMP, r1, 0x113724fa);
	jitc.asmALU32(X86_MOV, RSI, jitc.pc+4);
	jitc.asmJxx(X86_NE, (NativeAddress)ppc_sc_exception_asm);

	jitc.clobberRegister(NATIVE_REG | RSI);
	
	NativeReg r2 = jitc.getClientRegister(PPC_GPR(4));
	jitc.asmALU32(X86_CMP, r2, 0x77810f9b);
	if (r2 == RSI) {
		jitc.asmALU32(X86_MOV, RSI, jitc.pc+4);
	}
	jitc.asmJxx(X86_NE, (NativeAddress)ppc_sc_exception_asm);

	jitc.asmCALL((NativeAddress)gcard_osi);

	jitc.clobberRegister();
	return flowEndBlock;
}

/*
 *	sync		Synchronize
 *	.672
 */
void ppc_opc_sync(PPC_CPU_State &aCPU)
{
	// NO-OP
}
JITCFlow ppc_opc_gen_sync(JITC &jitc)
{
	// NO-OP
	return flowContinue;
}

/*
 *	tlbia		Translation Lookaside Buffer Invalidate All
 *	.676
 */
void ppc_opc_tlbia(PPC_CPU_State &aCPU)
{
	if (aCPU.msr & MSR_PR) {
//		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
		return;
	}
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	// FIXME: check rS.. for 0
	ppc_mmu_tlb_invalidate(aCPU);
}
JITCFlow ppc_opc_gen_tlbia(JITC &jitc)
{
	jitc.clobberAll();
	ppc_opc_gen_check_privilege(jitc);
	jitc.asmALU64(X86_LEA, RDI, curCPU(all));
	jitc.asmCALL((NativeAddress)ppc_mmu_tlb_invalidate_all_asm);
	jitc.asmALU32(X86_MOV, RAX, jitc.pc+4);
	jitc.asmJMP((NativeAddress)ppc_new_pc_rel_asm);
	return flowEndBlockUnreachable;
}

/*
 *	tlbie		Translation Lookaside Buffer Invalidate Entry
 *	.676
 */
void ppc_opc_tlbie(PPC_CPU_State &aCPU)
{
	if (aCPU.msr & MSR_PR) {
//		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
		return;
	}
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	// FIXME: check rS.. for 0
	ppc_mmu_tlb_invalidate(aCPU);
}
JITCFlow ppc_opc_gen_tlbie(JITC &jitc)
{
	jitc.flushRegister();
	ppc_opc_gen_check_privilege(jitc);
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
	jitc.getClientRegister(PPC_GPR(rB), NATIVE_REG | RAX);
	jitc.clobberAll();
	jitc.asmALU64(X86_MOV, RDI, curCPU(jitc));
	jitc.asmCALL((NativeAddress)ppc_mmu_tlb_invalidate_entry_asm);
	jitc.asmALU32(X86_MOV, RAX, jitc.pc+4);
	jitc.asmJMP((NativeAddress)ppc_new_pc_rel_asm);
	return flowEndBlockUnreachable;
}

/*
 *	tlbsync		Translation Lookaside Buffer Syncronize
 *	.677
 */
void ppc_opc_tlbsync(PPC_CPU_State &aCPU)
{
	if (aCPU.msr & MSR_PR) {
//		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_PRIV);
		return;
	}
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	// FIXME: check rS.. for 0     
}
JITCFlow ppc_opc_gen_tlbsync(JITC &jitc)
{
	ppc_opc_gen_check_privilege(jitc);
	return flowContinue;
}

/*
 *	tw		Trap Word
 *	.678
 */
void ppc_opc_tw(PPC_CPU_State &aCPU)
{
	int TO, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, TO, rA, rB);
	uint32 a = aCPU.gpr[rA];
	uint32 b = aCPU.gpr[rB];
	if (((TO & 16) && ((sint32)a < (sint32)b)) 
	|| ((TO & 8) && ((sint32)a > (sint32)b)) 
	|| ((TO & 4) && (a == b)) 
	|| ((TO & 2) && (a < b)) 
	|| ((TO & 1) && (a > b))) {
//		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_TRAP);
	}
}
JITCFlow ppc_opc_gen_tw(JITC &jitc)
{
	int TO, rA, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, TO, rA, rB);
	if (TO == 0x1f) {
		// TRAP always
		jitc.clobberAll();
		jitc.asmALU32(X86_MOV, RSI, jitc.pc);
		jitc.asmALU32(X86_MOV, RCX, PPC_EXC_PROGRAM_TRAP);
		jitc.asmJMP((NativeAddress)ppc_program_exception_asm);
		return flowEndBlockUnreachable;
	} else if (TO) {
		NativeReg a = jitc.getClientRegister(PPC_GPR(rA));
		NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
		jitc.clobberAll();
		jitc.asmALU32(X86_CMP, a, b);
		NativeAddress fixup1=NULL, fixup2=NULL, fixup3=NULL, fixup4=NULL, fixup5=NULL;
		if (TO & 16) fixup1 = jitc.asmJxxFixup(X86_L);
		if (TO & 8) fixup2 = jitc.asmJxxFixup(X86_G);
		if (TO & 4) fixup3 = jitc.asmJxxFixup(X86_E);
		if (TO & 2) fixup4 = jitc.asmJxxFixup(X86_B);
		if (TO & 1) fixup5 = jitc.asmJxxFixup(X86_A);
		NativeAddress fixup6 = jitc.asmJMPFixup();
		if (fixup1) jitc.asmResolveFixup(fixup1, jitc.asmHERE());
		if (fixup2) jitc.asmResolveFixup(fixup2, jitc.asmHERE());
		if (fixup3) jitc.asmResolveFixup(fixup3, jitc.asmHERE());
		if (fixup4) jitc.asmResolveFixup(fixup4, jitc.asmHERE());
		if (fixup5) jitc.asmResolveFixup(fixup5, jitc.asmHERE());
		jitc.asmALU32(X86_MOV, RSI, jitc.pc);
		jitc.asmALU32(X86_MOV, RCX, PPC_EXC_PROGRAM_TRAP);
		jitc.asmJMP((NativeAddress)ppc_program_exception_asm);
		jitc.asmResolveFixup(fixup6, jitc.asmHERE());
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
void ppc_opc_twi(PPC_CPU_State &aCPU)
{
	int TO, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, TO, rA, imm);
	uint32 a = aCPU.gpr[rA];
	if (((TO & 16) && ((sint32)a < (sint32)imm)) 
	|| ((TO & 8) && ((sint32)a > (sint32)imm)) 
	|| ((TO & 4) && (a == imm)) 
	|| ((TO & 2) && (a < imm)) 
	|| ((TO & 1) && (a > imm))) {
//		ppc_exception(PPC_EXC_PROGRAM, PPC_EXC_PROGRAM_TRAP);
	}
}
JITCFlow ppc_opc_gen_twi(JITC &jitc)
{
	int TO, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, TO, rA, imm);
	if (TO == 0x1f) {
		// TRAP always
		jitc.clobberAll();
		jitc.asmALU32(X86_MOV, RSI, jitc.pc);
		jitc.asmALU32(X86_MOV, RCX, PPC_EXC_PROGRAM_TRAP);
		jitc.asmJMP((NativeAddress)ppc_program_exception_asm);
		return flowEndBlockUnreachable;
	} else if (TO) {
		NativeReg a = jitc.getClientRegister(PPC_GPR(rA));
		jitc.clobberAll();
		jitc.asmALU32(X86_CMP, a, imm);
		NativeAddress fixup1=NULL, fixup2=NULL, fixup3=NULL, fixup4=NULL, fixup5=NULL;
		if (TO & 16) fixup1 = jitc.asmJxxFixup(X86_L);
		if (TO & 8) fixup2 = jitc.asmJxxFixup(X86_G);
		if (TO & 4) fixup3 = jitc.asmJxxFixup(X86_E);
		if (TO & 2) fixup4 = jitc.asmJxxFixup(X86_B);
		if (TO & 1) fixup5 = jitc.asmJxxFixup(X86_A);
		NativeAddress fixup6 = jitc.asmJMPFixup();
		if (fixup1) jitc.asmResolveFixup(fixup1, jitc.asmHERE());
		if (fixup2) jitc.asmResolveFixup(fixup2, jitc.asmHERE());
		if (fixup3) jitc.asmResolveFixup(fixup3, jitc.asmHERE());
		if (fixup4) jitc.asmResolveFixup(fixup4, jitc.asmHERE());
		if (fixup5) jitc.asmResolveFixup(fixup5, jitc.asmHERE());
		jitc.asmALU32(X86_MOV, RSI, jitc.pc);
		jitc.asmALU32(X86_MOV, RCX, PPC_EXC_PROGRAM_TRAP);
		jitc.asmJMP((NativeAddress)ppc_program_exception_asm);
		jitc.asmResolveFixup(fixup6, jitc.asmHERE());
		return flowEndBlock;
	} else {
		// TRAP never
		return flowContinue;
	}
}

/*      dcba	    Data Cache Block Allocate
 *      .???
 */
void ppc_opc_dcba(PPC_CPU_State &aCPU)
{
	// FIXME: check addr
}
JITCFlow ppc_opc_gen_dcba(JITC &jitc)
{
	// FIXME: check addr
	return flowContinue;
}
