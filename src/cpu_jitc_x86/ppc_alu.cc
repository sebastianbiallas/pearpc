/*
 *	PearPC
 *	ppc_alu.cc
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
#include "ppc_alu.h"
#include "ppc_dec.h"
#include "ppc_exc.h"
#include "ppc_cpu.h"
#include "ppc_opc.h"
#include "ppc_tools.h"

#include "jitc.h"
#include "jitc_asm.h"
#include "jitc_debug.h"
#include "x86asm.h"

static inline uint32 ppc_mask(int MB, int ME)
{
	uint32 mask;
	if (MB <= ME) {
		if (ME-MB == 31) {
			mask = 0xffffffff;
		} else {
			mask = ((1<<(ME-MB+1))-1)<<(31-ME);
		}
	} else {
		mask = ppc_word_rotl((1<<(32-MB+ME+1))-1, 31-ME);
	}
	return mask;
}

static JITCFlow ppc_opc_gen_ori_oris_xori_xoris(X86ALUopc opc, uint32 imm, int rS, int rA)
{
	if (imm) {
		jitcClobberCarryAndFlags();
		if (rA == rS) {
			NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
			asmALURegImm(opc, a, imm);
		} else {
			NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
			NativeReg a = jitcMapClientRegisterDirty(PPC_GPR(rA));
			asmALURegReg(X86_MOV, a, s);
			asmALURegImm(opc, a, imm);
		}
	} else {
		if (rA == rS) {
			/* nop */
		} else {
			/* mov */
			NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
			NativeReg a = jitcMapClientRegisterDirty(PPC_GPR(rA));
			asmALURegReg(X86_MOV, a, s);
		}
	}
	return flowContinue;
}

/*
 *	addx		Add
 *	.422
 */
void ppc_opc_addx()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
	gCPU.gpr[rD] = gCPU.gpr[rA] + gCPU.gpr[rB];
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rD]);
	}
}
static JITCFlow ppc_opc_gen_add()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gJITC.current_opc, rD, rA, rB);
	NativeReg a = jitcGetClientRegister(PPC_GPR(rA));
	NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
	if (rD == rA) {
		// add rA, rA, rB
		jitcClobberCarryAndFlags();
		asmALURegReg(X86_ADD, a, b);
		jitcDirtyRegister(a);
	} else if (rD == rB) {
		// add rB, rA, rB
		jitcClobberCarryAndFlags();
		asmALURegReg(X86_ADD, b, a);
		jitcDirtyRegister(b);
	} else {
		// add rD, rA, rB
		NativeReg result = jitcMapClientRegisterDirty(PPC_GPR(rD));
		// lea result, [a+1*b+0]
		byte modrm[6];
		asmLEA(result, modrm, x86_mem_sib_r(modrm, a, 1, b, 0));
		// result already is dirty
	}
	return flowContinue;
}
static JITCFlow ppc_opc_gen_addp()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gJITC.current_opc, rD, rA, rB);
	jitcClobberCarry();
	NativeReg a = jitcGetClientRegister(PPC_GPR(rA));
	NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
	if (rD == rA) {
		// add r1, r1, r2
		asmALURegReg(X86_ADD, a, b);
		jitcDirtyRegister(a);
	} else if (rD == rB) {
		// add r1, r2, r1
		asmALURegReg(X86_ADD, b, a);
		jitcDirtyRegister(b);
	} else {
		// add r3, r1, r2
		NativeReg result = jitcMapClientRegisterDirty(PPC_GPR(rD));
		// lea doesn't update the flags
		asmALURegReg(X86_MOV, result, a);
		asmALURegReg(X86_ADD, result, b);
	}
	jitcMapFlagsDirty();
	return flowContinue;
}
JITCFlow ppc_opc_gen_addx()
{
	if (gJITC.current_opc & PPC_OPC_Rc) {
		return ppc_opc_gen_addp();
	} else {
		return ppc_opc_gen_add();
	}
}
/*
 *	addox		Add with Overflow
 *	.422
 */
void ppc_opc_addox()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
	gCPU.gpr[rD] = gCPU.gpr[rA] + gCPU.gpr[rB];
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rD]);
	}
	// update XER flags
	PPC_ALU_ERR("addox unimplemented\n");
}
/*
 *	addcx		Add Carrying
 *	.423
 */
void ppc_opc_addcx()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
	uint32 a = gCPU.gpr[rA];
	gCPU.gpr[rD] = a + gCPU.gpr[rB];
	gCPU.xer_ca = (gCPU.gpr[rD] < a);
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rD]);
	}
}
JITCFlow ppc_opc_gen_addcx()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gJITC.current_opc, rD, rA, rB);
	if (!(gJITC.current_opc & PPC_OPC_Rc)) jitcClobberFlags();
	NativeReg a = jitcGetClientRegister(PPC_GPR(rA));
	NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
	if (rD == rA) {
		// add r1, r1, r2
		asmALURegReg(X86_ADD, a, b);
		jitcDirtyRegister(a);
	} else if (rD == rB) {
		// add r1, r2, r1
		asmALURegReg(X86_ADD, b, a);
		jitcDirtyRegister(b);
	} else {
		// add r3, r1, r2
		NativeReg result = jitcMapClientRegisterDirty(PPC_GPR(rD));
		// lea doesn't update the carry
		asmALURegReg(X86_MOV, result, a);
		asmALURegReg(X86_ADD, result, b);
	}
	jitcMapCarryDirty();
	if (gJITC.current_opc & PPC_OPC_Rc) jitcMapFlagsDirty();
	return flowContinue;
}
/*
 *	addcox		Add Carrying with Overflow
 *	.423
 */
void ppc_opc_addcox()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
	uint32 a = gCPU.gpr[rA];
	gCPU.gpr[rD] = a + gCPU.gpr[rB];
	gCPU.xer_ca = (gCPU.gpr[rD] < a);
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rD]);
	}
	// update XER flags
	PPC_ALU_ERR("addcox unimplemented\n");
}
/*
 *	addex		Add Extended
 *	.424
 */
void ppc_opc_addex()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
	uint32 a = gCPU.gpr[rA];
	uint32 b = gCPU.gpr[rB];
	uint32 ca = gCPU.xer_ca;
	gCPU.gpr[rD] = a + b + ca;
	gCPU.xer_ca = ppc_carry_3(a, b, ca);
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rD]);
	}
}
JITCFlow ppc_opc_gen_addex()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gJITC.current_opc, rD, rA, rB);
	if (!(gJITC.current_opc & PPC_OPC_Rc)) jitcClobberFlags();
	jitcGetClientCarry();
	NativeReg a = jitcGetClientRegister(PPC_GPR(rA));
	NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
	if (rD == rA) {
		// add r1, r1, r2
		asmALURegReg(X86_ADC, a, b);
		jitcDirtyRegister(a);
	} else if (rD == rB) {
		// add r1, r2, r1
		asmALURegReg(X86_ADC, b, a);
		jitcDirtyRegister(b);
	} else {
		// add r3, r1, r2
		NativeReg result = jitcMapClientRegisterDirty(PPC_GPR(rD));
		// lea doesn't update the carry
		asmALURegReg(X86_MOV, result, a);
		asmALURegReg(X86_ADC, result, b);
	}
	jitcMapCarryDirty();
	if (gJITC.current_opc & PPC_OPC_Rc) jitcMapFlagsDirty();
	return flowContinue;
}
/*
 *	addeox		Add Extended with Overflow
 *	.424
 */
void ppc_opc_addeox()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
	uint32 a = gCPU.gpr[rA];
	uint32 b = gCPU.gpr[rB];
	uint32 ca = gCPU.xer_ca;
	gCPU.gpr[rD] = a + b + ca;
	gCPU.xer_ca = ppc_carry_3(a, b, ca);
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rD]);
	}
	// update XER flags
	PPC_ALU_ERR("addeox unimplemented\n");
}
/*
 *	addi		Add Immediate
 *	.425
 */
void ppc_opc_addi()
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, rD, rA, imm);
	gCPU.gpr[rD] = (rA ? gCPU.gpr[rA] : 0) + imm;
}
JITCFlow ppc_opc_gen_addi_addis(int rD, int rA, uint32 imm)
{
	if (rA == 0) {
		NativeReg d = jitcMapClientRegisterDirty(PPC_GPR(rD));
		if (imm == 0 && !jitcFlagsMapped() && !jitcCarryMapped()) {
			jitcClobberCarryAndFlags();
			asmALURegReg(X86_XOR, d, d);
		} else {
			asmMOVRegImm_NoFlags(d, imm);
		}
	} else {
		if (rD == rA) {
			NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
			if (!imm) {
				// empty
			} else if (imm == 1) {
				// inc / dec doesn't clobber carry
				jitcClobberFlags();
				asmINCReg(a);
			} else if (imm == 0xffffffff) {
				jitcClobberFlags();
				asmDECReg(a);
			} else {
				if (jitcFlagsMapped() || jitcCarryMapped()) {
					// lea rA, [rB+imm]
					byte modrm[6];
					asmLEA(a, modrm, x86_mem_r(modrm, a, imm));
				} else {
					jitcClobberCarryAndFlags();
					asmALURegImm(X86_ADD, a, imm);
				}
			}
		} else {
			if (imm) {
				NativeReg a = jitcGetClientRegister(PPC_GPR(rA));
				// lea d, [a+imm]
				NativeReg d = jitcMapClientRegisterDirty(PPC_GPR(rD));
				byte modrm[6];
				asmLEA(d, modrm, x86_mem_r(modrm, a, imm));
			} else {
				NativeReg a = jitcGetClientRegister(PPC_GPR(rA));
				// mov d, a
				NativeReg d = jitcMapClientRegisterDirty(PPC_GPR(rD));
				asmALURegReg(X86_MOV, d, a);
			}
		}
	}
	return flowContinue;
}
JITCFlow ppc_opc_gen_addi()
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, rD, rA, imm);
	return ppc_opc_gen_addi_addis(rD, rA, imm);
}	
/*
 *	addic		Add Immediate Carrying
 *	.426
 */
void ppc_opc_addic()
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, rD, rA, imm);
	uint32 a = gCPU.gpr[rA];
	gCPU.gpr[rD] = a + imm;	
	gCPU.xer_ca = (gCPU.gpr[rD] < a);
}
JITCFlow ppc_opc_gen_addic()
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, rD, rA, imm);
	jitcClobberFlags();
	if (rD == rA) {
		NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
		asmALURegImm(X86_ADD, a, imm);
	} else {
		NativeReg a = jitcGetClientRegister(PPC_GPR(rA));
		NativeReg d = jitcMapClientRegisterDirty(PPC_GPR(rD));
		asmALURegReg(X86_MOV, d, a);
		asmALURegImm(X86_ADD, d, imm);
	}
	jitcMapCarryDirty();
	return flowContinue;
}
/*
 *	addic.		Add Immediate Carrying and Record
 *	.427
 */
void ppc_opc_addic_()
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, rD, rA, imm);
	uint32 a = gCPU.gpr[rA];
	gCPU.gpr[rD] = a + imm;
	gCPU.xer_ca = (gCPU.gpr[rD] < a);
	// update cr0 flags
	ppc_update_cr0(gCPU.gpr[rD]);
}
JITCFlow ppc_opc_gen_addic_()
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, rD, rA, imm);
	if (rD == rA) {
		NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
		asmALURegImm(X86_ADD, a, imm);
	} else {
		NativeReg a = jitcGetClientRegister(PPC_GPR(rA));
		NativeReg d = jitcMapClientRegisterDirty(PPC_GPR(rD));
		asmALURegReg(X86_MOV, d, a);
		asmALURegImm(X86_ADD, d, imm);
	}
	jitcMapCarryDirty();
	jitcMapFlagsDirty();
	return flowContinue;
}
/*
 *	addis		Add Immediate Shifted
 *	.428
 */
void ppc_opc_addis()
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_Shift16(gCPU.current_opc, rD, rA, imm);
	gCPU.gpr[rD] = (rA ? gCPU.gpr[rA] : 0) + imm;
}
JITCFlow ppc_opc_gen_addis()
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_Shift16(gJITC.current_opc, rD, rA, imm);
	return ppc_opc_gen_addi_addis(rD, rA, imm);
}	
/*
 *	addmex		Add to Minus One Extended
 *	.429
 */
void ppc_opc_addmex()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
	PPC_OPC_ASSERT(rB == 0);
	uint32 a = gCPU.gpr[rA];
	uint32 ca = gCPU.xer_ca;
	gCPU.gpr[rD] = a + ca + 0xffffffff;
	gCPU.xer_ca = a || ca;
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rD]);
	}
}
JITCFlow ppc_opc_gen_addmex()
{
	ppc_opc_gen_interpret(ppc_opc_addmex);
	return flowEndBlock;
	
}
/*
 *	addmeox		Add to Minus One Extended with Overflow
 *	.429
 */
void ppc_opc_addmeox()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
	PPC_OPC_ASSERT(rB == 0);
	uint32 a = gCPU.gpr[rA];
	uint32 ca = (gCPU.xer_ca);
	gCPU.gpr[rD] = a + ca + 0xffffffff;
	gCPU.xer_ca = (a || ca);
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rD]);
	}
	// update XER flags
	PPC_ALU_ERR("addmeox unimplemented\n");
}
/*
 *	addzex		Add to Zero Extended
 *	.430
 */
void ppc_opc_addzex()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
	PPC_OPC_ASSERT(rB == 0);
	uint32 a = gCPU.gpr[rA];
	uint32 ca = gCPU.xer_ca;
	gCPU.gpr[rD] = a + ca;
	gCPU.xer_ca = ((a == 0xffffffff) && ca);
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rD]);
	}
}
JITCFlow ppc_opc_gen_addzex()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gJITC.current_opc, rD, rA, rB);
	if (!(gJITC.current_opc & PPC_OPC_Rc)) jitcClobberFlags();
	jitcGetClientCarry();
	NativeReg d;
	if (rA == rD) {
		d = jitcGetClientRegisterDirty(PPC_GPR(rD));
	} else {
		NativeReg a = jitcGetClientRegister(PPC_GPR(rA));
		d = jitcMapClientRegisterDirty(PPC_GPR(rD));
		asmALURegReg(X86_MOV, d, a);
	}
	asmALURegImm(X86_ADC, d, 0);
	jitcMapCarryDirty();
	if (gJITC.current_opc & PPC_OPC_Rc) jitcMapFlagsDirty();
	return flowContinue;
}
/*
 *	addzeox		Add to Zero Extended with Overflow
 *	.430
 */
void ppc_opc_addzeox()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
	PPC_OPC_ASSERT(rB == 0);
	uint32 a = gCPU.gpr[rA];
	uint32 ca = gCPU.xer_ca;
	gCPU.gpr[rD] = a + ca;
	gCPU.xer_ca = ((a == 0xffffffff) && ca);
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rD]);
	}
	// update XER flags
	PPC_ALU_ERR("addzeox unimplemented\n");
}

/*
 *	andx		AND
 *	.431
 */
void ppc_opc_andx()
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, rB);
	gCPU.gpr[rA] = gCPU.gpr[rS] & gCPU.gpr[rB];
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_andx()
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, rA, rB);
	if (gJITC.current_opc & PPC_OPC_Rc) {
		jitcClobberCarry();
	} else {
		jitcClobberCarryAndFlags();
	}
	if (rA == rS) {
		NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
		NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
		asmALURegReg(X86_AND, a, b);
	} else if (rA == rB) {
		NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
		NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
		asmALURegReg(X86_AND, a, s);
	} else {
		NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
		NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
		NativeReg a = jitcMapClientRegisterDirty(PPC_GPR(rA));
		asmALURegReg(X86_MOV, a, s);
		asmALURegReg(X86_AND, a, b);
	}
	if (gJITC.current_opc & PPC_OPC_Rc) {
		jitcMapFlagsDirty();
	}
	return flowContinue;
}
/*
 *	andcx		AND with Complement
 *	.432
 */
void ppc_opc_andcx()
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, rB);
	gCPU.gpr[rA] = gCPU.gpr[rS] & ~gCPU.gpr[rB];
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_andcx()
{
	ppc_opc_gen_interpret(ppc_opc_andcx);
	return flowEndBlock;
}
/*
 *	andi.		AND Immediate
 *	.433
 */
void ppc_opc_andi_()
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_UImm(gCPU.current_opc, rS, rA, imm);
	gCPU.gpr[rA] = gCPU.gpr[rS] & imm;
	// update cr0 flags
	ppc_update_cr0(gCPU.gpr[rA]);
}
JITCFlow ppc_opc_gen_andi_()
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_UImm(gJITC.current_opc, rS, rA, imm);
	jitcClobberCarry();
	if (rS == rA) {
		NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
		asmALURegImm(X86_AND, a, imm);
	} else {
		NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
		NativeReg a = jitcMapClientRegisterDirty(PPC_GPR(rA));
		asmALURegReg(X86_MOV, a, s);
		asmALURegImm(X86_AND, a, imm);
	}
	jitcMapFlagsDirty();
	return flowContinue;
}
/*
 *	andis.		AND Immediate Shifted
 *	.434
 */
void ppc_opc_andis_()
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_Shift16(gCPU.current_opc, rS, rA, imm);
	gCPU.gpr[rA] = gCPU.gpr[rS] & imm;
	// update cr0 flags
	ppc_update_cr0(gCPU.gpr[rA]);
}
JITCFlow ppc_opc_gen_andis_()
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_Shift16(gJITC.current_opc, rS, rA, imm);
	jitcClobberCarry();
	if (rS == rA) {
		NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
		asmALURegImm(X86_AND, a, imm);
	} else {
		NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
		NativeReg a = jitcMapClientRegisterDirty(PPC_GPR(rA));
		asmALURegReg(X86_MOV, a, s);
		asmALURegImm(X86_AND, a, imm);
	}
	jitcMapFlagsDirty();
	return flowContinue;
}

/*
 *	cmp		Compare
 *	.442
 */
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

void ppc_opc_cmp()
{
	uint32 cr;
	int rA, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, cr, rA, rB);
	cr >>= 2;
	sint32 a = gCPU.gpr[rA];
	sint32 b = gCPU.gpr[rB];
	uint32 c;
	if (a < b) {
		c = 8;
	} else if (a > b) {
		c = 4;
	} else {
		c = 2;
	}
	if (gCPU.xer & XER_SO) c |= 1;
	cr = 7-cr;
	gCPU.cr &= ppc_cmp_and_mask[cr];
	gCPU.cr |= c<<(cr*4);
}
JITCFlow ppc_opc_gen_cmp()
{
	uint32 cr;
	int rA, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, cr, rA, rB);
	cr >>= 2;
	jitcClobberCarryAndFlags();
	NativeReg a = jitcGetClientRegister(PPC_GPR(rA));
	NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
	asmALURegReg(X86_CMP, a, b);
	if (cr == 0) {
		asmCALL((NativeAddress)ppc_flush_flags_signed_0_asm);
	} else {
		jitcClobberRegister(EAX | NATIVE_REG);
		asmMOVRegImm_NoFlags(EAX, (7-cr)/2);
		asmCALL((cr & 1) ? (NativeAddress)ppc_flush_flags_signed_odd_asm : (NativeAddress)ppc_flush_flags_signed_even_asm);
	}
	return flowContinue;
}
/*
 *	cmpi		Compare Immediate
 *	.443
 */
void ppc_opc_cmpi()
{
	uint32 cr;
	int rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, cr, rA, imm);
	cr >>= 2;
	sint32 a = gCPU.gpr[rA];
	sint32 b = imm;
	uint32 c;
	if (a < b) {
		c = 8;
	} else if (a > b) {
		c = 4;
	} else {
		c = 2;
	}
	if (gCPU.xer & XER_SO) c |= 1;
	cr = 7-cr;
	gCPU.cr &= ppc_cmp_and_mask[cr];
	gCPU.cr |= c<<(cr*4);
}
JITCFlow ppc_opc_gen_cmpi()
{
	uint32 cr;
	int rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, cr, rA, imm);
	cr >>= 2;
	jitcClobberCarryAndFlags();
	NativeReg a = jitcGetClientRegister(PPC_GPR(rA));
	asmALURegImm(X86_CMP, a, imm);
	if (cr == 0) {
		asmCALL((NativeAddress)ppc_flush_flags_signed_0_asm);
	} else {
		jitcClobberRegister(EAX | NATIVE_REG);
		asmMOVRegImm_NoFlags(EAX, (7-cr)/2);
		asmCALL((cr & 1) ? (NativeAddress)ppc_flush_flags_signed_odd_asm : (NativeAddress)ppc_flush_flags_signed_even_asm);
	}
	return flowContinue;
}
/*
 *	cmpl		Compare Logical
 *	.444
 */
void ppc_opc_cmpl()
{
	uint32 cr;
	int rA, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, cr, rA, rB);
	cr >>= 2;
	uint32 a = gCPU.gpr[rA];
	uint32 b = gCPU.gpr[rB];
	uint32 c;
	if (a < b) {
		c = 8;
	} else if (a > b) {
		c = 4;
	} else {
		c = 2;
	}
	if (gCPU.xer & XER_SO) c |= 1;
	cr = 7-cr;
	gCPU.cr &= ppc_cmp_and_mask[cr];
	gCPU.cr |= c<<(cr*4);
}
JITCFlow ppc_opc_gen_cmpl()
{
	uint32 cr;
	int rA, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, cr, rA, rB);
	cr >>= 2;
	jitcClobberCarryAndFlags();
	NativeReg a = jitcGetClientRegister(PPC_GPR(rA));
	NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
	asmALURegReg(X86_CMP, a, b);
	if (cr == 0) {
		asmCALL((NativeAddress)ppc_flush_flags_unsigned_0_asm);
	} else {
		jitcClobberRegister(EAX | NATIVE_REG);
		asmMOVRegImm_NoFlags(EAX, (7-cr)/2);
		asmCALL((cr & 1) ? (NativeAddress)ppc_flush_flags_unsigned_odd_asm : (NativeAddress)ppc_flush_flags_unsigned_even_asm);
	}
	return flowContinue;
}
/*
 *	cmpli		Compare Logical Immediate
 *	.445
 */
void ppc_opc_cmpli()
{
	uint32 cr;
	int rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_UImm(gCPU.current_opc, cr, rA, imm);
	cr >>= 2;
	uint32 a = gCPU.gpr[rA];
	uint32 b = imm;
	uint32 c;
	if (a < b) {
		c = 8;
	} else if (a > b) {
		c = 4;
	} else {
		c = 2;
	}
	if (gCPU.xer & XER_SO) c |= 1;
	cr = 7-cr;
	gCPU.cr &= ppc_cmp_and_mask[cr];
	gCPU.cr |= c<<(cr*4);
}
JITCFlow ppc_opc_gen_cmpli()
{
	uint32 cr;
	int rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_UImm(gJITC.current_opc, cr, rA, imm);
	cr >>= 2;
	jitcClobberCarryAndFlags();
	NativeReg a = jitcGetClientRegister(PPC_GPR(rA));
	asmALURegImm(X86_CMP, a, imm);
	if (cr == 0) {
		asmCALL((NativeAddress)ppc_flush_flags_unsigned_0_asm);
	} else {
		jitcClobberRegister(EAX | NATIVE_REG);
		asmMOVRegImm_NoFlags(EAX, (7-cr)/2);
		asmCALL((cr & 1) ? (NativeAddress)ppc_flush_flags_unsigned_odd_asm : (NativeAddress)ppc_flush_flags_unsigned_even_asm);
	}
	return flowContinue;
}

/*
 *	cntlzwx		Count Leading Zeros Word
 *	.447
 */
void ppc_opc_cntlzwx()
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, rB);
	PPC_OPC_ASSERT(rB==0);
	uint32 n=0;
	uint32 x=0x80000000;
	uint32 v=gCPU.gpr[rS];
	while (!(v & x)) {
		n++;
		if (n==32) break;
		x>>=1;
	}
	gCPU.gpr[rA] = n;
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_cntlzwx()
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, rA, rB);
	jitcClobberCarryAndFlags();
	NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
	NativeReg a = jitcMapClientRegisterDirty(PPC_GPR(rA));
	NativeReg z = jitcAllocRegister();
	asmALURegImm(X86_MOV, z, 0xffffffff);
	asmBSxRegReg(X86_BSR, a, s);
	asmCMOVRegReg(X86_Z, a, z);
	asmALUReg(X86_NEG, a);
	asmALURegImm(X86_ADD, a, 31);
	if (gJITC.current_opc & PPC_OPC_Rc) {
		jitcMapFlagsDirty();
	}
	return flowContinue;
}

/*
 *	crand		Condition Register AND
 *	.448
 */
void ppc_opc_crand()
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, crD, crA, crB);
	if ((gCPU.cr & (1<<(31-crA))) && (gCPU.cr & (1<<(31-crB)))) {
		gCPU.cr |= (1<<(31-crD));
	} else {
		gCPU.cr &= ~(1<<(31-crD));
	}
}
JITCFlow ppc_opc_gen_crand()
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, crD, crA, crB);
	jitcClobberCarryAndFlags();
	asmTESTDMemImm((uint32)&gCPU.cr, 1<<(31-crA));	
	NativeAddress nocrA = asmJxxFixup(X86_Z);
		asmTESTDMemImm((uint32)&gCPU.cr, 1<<(31-crB));
		NativeAddress nocrB = asmJxxFixup(X86_Z);
			asmORDMemImm((uint32)&gCPU.cr, 1<<(31-crD));
			NativeAddress end1 = asmJMPFixup();
	asmResolveFixup(nocrB, asmHERE());
	asmResolveFixup(nocrA, asmHERE());
		asmANDDMemImm((uint32)&gCPU.cr, ~(1<<(31-crD)));
	asmResolveFixup(end1, asmHERE());
	return flowContinue;
}
/*
 *	crandc		Condition Register AND with Complement
 *	.449
 */
void ppc_opc_crandc()
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, crD, crA, crB);
	if ((gCPU.cr & (1<<(31-crA))) && !(gCPU.cr & (1<<(31-crB)))) {
		gCPU.cr |= (1<<(31-crD));
	} else {
		gCPU.cr &= ~(1<<(31-crD));
	}
}
JITCFlow ppc_opc_gen_crandc()
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, crD, crA, crB);
	jitcClobberCarryAndFlags();
	asmTESTDMemImm((uint32)&gCPU.cr, 1<<(31-crA));	
	NativeAddress nocrA = asmJxxFixup(X86_Z);
		asmTESTDMemImm((uint32)&gCPU.cr, 1<<(31-crB));
		NativeAddress nocrB = asmJxxFixup(X86_NZ);
			asmORDMemImm((uint32)&gCPU.cr, 1<<(31-crD));
			NativeAddress end1 = asmJMPFixup();
	asmResolveFixup(nocrB, asmHERE());
	asmResolveFixup(nocrA, asmHERE());
		asmANDDMemImm((uint32)&gCPU.cr, ~(1<<(31-crD)));
	asmResolveFixup(end1, asmHERE());
	return flowContinue;
}
/*
 *	creqv		Condition Register Equivalent
 *	.450
 */
void ppc_opc_creqv()
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, crD, crA, crB);
	if (((gCPU.cr & (1<<(31-crA))) && (gCPU.cr & (1<<(31-crB))))
	  || (!(gCPU.cr & (1<<(31-crA))) && !(gCPU.cr & (1<<(31-crB))))) {
		gCPU.cr |= (1<<(31-crD));
	} else {
		gCPU.cr &= ~(1<<(31-crD));
	}
}
JITCFlow ppc_opc_gen_creqv()
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, crD, crA, crB);
	jitcClobberCarryAndFlags();
	if (crA == crB) {
		asmORDMemImm((uint32)&gCPU.cr, 1<<(31-crD));
	} else {
		// crD = crA ? (crB ? 1 : 0) : (crB ? 0 : 1) 
		asmTESTDMemImm((uint32)&gCPU.cr, 1<<(31-crA));
		NativeAddress nocrA = asmJxxFixup(X86_Z);
			asmTESTDMemImm((uint32)&gCPU.cr, 1<<(31-crB));
			NativeAddress nocrB1 = asmJxxFixup(X86_Z);
				asmORDMemImm((uint32)&gCPU.cr, 1<<(31-crD));
				NativeAddress end1 = asmJMPFixup();
			asmResolveFixup(nocrB1, asmHERE());
				asmANDDMemImm((uint32)&gCPU.cr, ~(1<<(31-crD)));
				NativeAddress end2 = asmJMPFixup();
		asmResolveFixup(nocrA, asmHERE());
			asmTESTDMemImm((uint32)&gCPU.cr, 1<<(31-crB));
			NativeAddress nocrB2 = asmJxxFixup(X86_Z);
				asmANDDMemImm((uint32)&gCPU.cr, ~(1<<(31-crD)));
				NativeAddress end3 = asmJMPFixup();
			asmResolveFixup(nocrB2, asmHERE());
				asmORDMemImm((uint32)&gCPU.cr, 1<<(31-crD));
		asmResolveFixup(end1, asmHERE());
		asmResolveFixup(end2, asmHERE());
		asmResolveFixup(end3, asmHERE());
	}
	return flowContinue;
}
/*
 *	crnand		Condition Register NAND
 *	.451
 */
void ppc_opc_crnand()
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, crD, crA, crB);
	if (!((gCPU.cr & (1<<(31-crA))) && (gCPU.cr & (1<<(31-crB))))) {
		gCPU.cr |= (1<<(31-crD));
	} else {
		gCPU.cr &= ~(1<<(31-crD));
	}
}
JITCFlow ppc_opc_gen_crnand()
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, crD, crA, crB);
	jitcClobberCarryAndFlags();
	asmTESTDMemImm((uint32)&gCPU.cr, 1<<(31-crA));	
	NativeAddress nocrA = asmJxxFixup(X86_Z);
		asmTESTDMemImm((uint32)&gCPU.cr, 1<<(31-crB));
		NativeAddress nocrB = asmJxxFixup(X86_Z);
			asmANDDMemImm((uint32)&gCPU.cr, ~(1<<(31-crD)));
			NativeAddress end1 = asmJMPFixup();
	asmResolveFixup(nocrB, asmHERE());
	asmResolveFixup(nocrA, asmHERE());
		asmORDMemImm((uint32)&gCPU.cr, 1<<(31-crD));
	asmResolveFixup(end1, asmHERE());
	return flowContinue;
}
/*
 *	crnor		Condition Register NOR
 *	.452
 */
void ppc_opc_crnor()
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, crD, crA, crB);
	uint32 t = (1<<(31-crA)) | (1<<(31-crB));
	if (!(gCPU.cr & t)) {
		gCPU.cr |= (1<<(31-crD));
	} else {
		gCPU.cr &= ~(1<<(31-crD));
	}
}
JITCFlow ppc_opc_gen_crnor()
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, crD, crA, crB);
	jitcClobberCarryAndFlags();
	asmTESTDMemImm((uint32)&gCPU.cr, (1<<(31-crA)) | (1<<(31-crB)));
	NativeAddress notset = asmJxxFixup(X86_Z);
		asmANDDMemImm((uint32)&gCPU.cr, ~(1<<(31-crD)));
		NativeAddress end1 = asmJMPFixup();
	asmResolveFixup(notset, asmHERE());
		asmORDMemImm((uint32)&gCPU.cr, 1<<(31-crD));
	asmResolveFixup(end1, asmHERE());
	return flowContinue;
}
/*
 *	cror		Condition Register OR
 *	.453
 */
void ppc_opc_cror()
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, crD, crA, crB);
	uint32 t = (1<<(31-crA)) | (1<<(31-crB));
	if (gCPU.cr & t) {
		gCPU.cr |= (1<<(31-crD));
	} else {
		gCPU.cr &= ~(1<<(31-crD));
	}
}
JITCFlow ppc_opc_gen_cror()
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, crD, crA, crB);
	jitcClobberCarryAndFlags();
	asmTESTDMemImm((uint32)&gCPU.cr, (1<<(31-crA)) | (1<<(31-crB)));
	NativeAddress notset = asmJxxFixup(X86_Z);
		asmORDMemImm((uint32)&gCPU.cr, 1<<(31-crD));
		NativeAddress end1 = asmJMPFixup();
	asmResolveFixup(notset, asmHERE());
		asmANDDMemImm((uint32)&gCPU.cr, ~(1<<(31-crD)));
	asmResolveFixup(end1, asmHERE());
	return flowContinue;
}
/*
 *	crorc		Condition Register OR with Complement
 *	.454
 */
void ppc_opc_crorc()
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, crD, crA, crB);
	if ((gCPU.cr & (1<<(31-crA))) || !(gCPU.cr & (1<<(31-crB)))) {
		gCPU.cr |= (1<<(31-crD));
	} else {
		gCPU.cr &= ~(1<<(31-crD));
	}
}

JITCFlow ppc_opc_gen_crorc()
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, crD, crA, crB);
	jitcClobberCarryAndFlags();
	asmTESTDMemImm((uint32)&gCPU.cr, 1<<(31-crA));	
	NativeAddress crAset = asmJxxFixup(X86_NZ);
		asmTESTDMemImm((uint32)&gCPU.cr, 1<<(31-crB));
		NativeAddress nocrB = asmJxxFixup(X86_Z);
			asmANDDMemImm((uint32)&gCPU.cr, ~(1<<(31-crD)));
			NativeAddress end1 = asmJMPFixup();
	asmResolveFixup(nocrB, asmHERE());
	asmResolveFixup(crAset, asmHERE());
		asmORDMemImm((uint32)&gCPU.cr, 1<<(31-crD));
	asmResolveFixup(end1, asmHERE());
	return flowContinue;
}
/*
 *	crxor		Condition Register XOR
 *	.448
 */
void ppc_opc_crxor()
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, crD, crA, crB);
	if ((!(gCPU.cr & (1<<(31-crA))) && (gCPU.cr & (1<<(31-crB))))
	  || ((gCPU.cr & (1<<(31-crA))) && !(gCPU.cr & (1<<(31-crB))))) {
		gCPU.cr |= (1<<(31-crD));
	} else {
		gCPU.cr &= ~(1<<(31-crD));
	}
}
JITCFlow ppc_opc_gen_crxor()
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, crD, crA, crB);
	jitcClobberCarryAndFlags();
	if (crA == crB) {
		asmANDDMemImm((uint32)&gCPU.cr, ~(1<<(31-crD)));
	} else {
		PPC_ALU_WARN("crxor untested!\n");
		// crD = crA ? (crB ? 0 : 1) : (crB ? 1 : 0) 
		asmTESTDMemImm((uint32)&gCPU.cr, 1<<(31-crA));
		NativeAddress nocrA = asmJxxFixup(X86_Z);
			asmTESTDMemImm((uint32)&gCPU.cr, 1<<(31-crB));
			NativeAddress nocrB1 = asmJxxFixup(X86_Z);
				asmANDDMemImm((uint32)&gCPU.cr, ~(1<<(31-crD)));
				NativeAddress end1 = asmJMPFixup();
			asmResolveFixup(nocrB1, asmHERE());
				asmORDMemImm((uint32)&gCPU.cr, 1<<(31-crD));
				NativeAddress end2 = asmJMPFixup();
		asmResolveFixup(nocrA, asmHERE());
			asmTESTDMemImm((uint32)&gCPU.cr, 1<<(31-crB));
			NativeAddress nocrB2 = asmJxxFixup(X86_Z);
				asmORDMemImm((uint32)&gCPU.cr, 1<<(31-crD));
				NativeAddress end3 = asmJMPFixup();
			asmResolveFixup(nocrB2, asmHERE());
				asmANDDMemImm((uint32)&gCPU.cr, ~(1<<(31-crD)));
		asmResolveFixup(end1, asmHERE());
		asmResolveFixup(end2, asmHERE());
		asmResolveFixup(end3, asmHERE());
	}
	return flowContinue;
}

/*
 *	divwx		Divide Word
 *	.470
 */
void ppc_opc_divwx()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
	if (!gCPU.gpr[rB]) {
		PPC_ALU_WARN("division by zero @%08x\n", gCPU.pc);
		SINGLESTEP("");
	} else {
		sint32 a = gCPU.gpr[rA];
		sint32 b = gCPU.gpr[rB];
		gCPU.gpr[rD] = a / b;
	}
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rD]);
	}
}
JITCFlow ppc_opc_gen_divwx()
{
	ppc_opc_gen_interpret(ppc_opc_divwx);
	return flowEndBlock;
}
/*
 *	divwox		Divide Word with Overflow
 *	.470
 */
void ppc_opc_divwox()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
	if (!gCPU.gpr[rB]) {
		PPC_ALU_WARN("division by zero\n");
	} else {
		sint32 a = gCPU.gpr[rA];
		sint32 b = gCPU.gpr[rB];
		gCPU.gpr[rD] = a / b;
	}
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rD]);
	}
	// update XER flags
	PPC_ALU_ERR("divwox unimplemented\n");
}
/*
 *	divwux		Divide Word Unsigned
 *	.472
 */
void ppc_opc_divwux()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
	if (!gCPU.gpr[rB]) {
		PPC_ALU_WARN("division by zero @%08x\n", gCPU.pc);
		SINGLESTEP("");
	} else {
		gCPU.gpr[rD] = gCPU.gpr[rA] / gCPU.gpr[rB];
	}
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rD]);
	}
}
JITCFlow ppc_opc_gen_divwux()
{
	ppc_opc_gen_interpret(ppc_opc_divwux);
	return flowEndBlock;
}
/*
 *	divwuox		Divide Word Unsigned with Overflow
 *	.472
 */
void ppc_opc_divwuox()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
	if (!gCPU.gpr[rB]) {
		PPC_ALU_WARN("division by zero @%08x\n", gCPU.pc);
	} else {
		gCPU.gpr[rD] = gCPU.gpr[rA] / gCPU.gpr[rB];
	}
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rD]);
	}
	// update XER flags
	PPC_ALU_ERR("divwuox unimplemented\n");
}

/*
 *	eqvx		Equivalent
 *	.480
 */
void ppc_opc_eqvx()
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, rB);
	gCPU.gpr[rA] = ~(gCPU.gpr[rS] ^ gCPU.gpr[rB]);
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_eqvx()
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, rA, rB);
	if (gJITC.current_opc & PPC_OPC_Rc) {
		jitcClobberCarry();
	} else {
		jitcClobberCarryAndFlags();
	}
	NativeReg a;
	if (rA == rS) {
		a = jitcGetClientRegisterDirty(PPC_GPR(rA));
		NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
		asmALURegReg(X86_XOR, a, b);
	} else if (rA == rB) {
		a = jitcGetClientRegisterDirty(PPC_GPR(rA));
		NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
		asmALURegReg(X86_XOR, a, s);
	} else {
		NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
		NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
		a = jitcMapClientRegisterDirty(PPC_GPR(rA));
		asmALURegReg(X86_MOV, a, s);
		asmALURegReg(X86_XOR, a, b);
	}
	asmALUReg(X86_NOT, a);
	if (gJITC.current_opc & PPC_OPC_Rc) {
		// "NOT" doesn't update the flags
		asmALURegReg(X86_OR, a, a);
		jitcMapFlagsDirty();
	}
	return flowContinue;
}

/*
 *	extsbx		Extend Sign Byte
 *	.481
 */
void ppc_opc_extsbx()
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, rB);
	PPC_OPC_ASSERT(rB==0);
	gCPU.gpr[rA] = gCPU.gpr[rS];
	if (gCPU.gpr[rA] & 0x80) {
		gCPU.gpr[rA] |= 0xffffff00;
	} else {
		gCPU.gpr[rA] &= ~0xffffff00;
	}
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_extsbx()
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, rA, rB);
	NativeReg8 s = (NativeReg8)jitcGetClientRegister(PPC_GPR(rS), NATIVE_REG_8);
	NativeReg a = jitcMapClientRegisterDirty(PPC_GPR(rA));
	asmMOVxxRegReg8(X86_MOVSX, a, s);
	if (gJITC.current_opc & PPC_OPC_Rc) {
		jitcClobberCarry();
		asmALURegReg(X86_OR, a, a);
		jitcMapFlagsDirty();
	}
	return flowContinue;
}
/*
 *	extshx		Extend Sign Half Word
 *	.482
 */
void ppc_opc_extshx()
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, rB);
	PPC_OPC_ASSERT(rB==0);
	gCPU.gpr[rA] = gCPU.gpr[rS];
	if (gCPU.gpr[rA] & 0x8000) {
		gCPU.gpr[rA] |= 0xffff0000;
	} else {
		gCPU.gpr[rA] &= ~0xffff0000;
	}
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_extshx()
{
	ppc_opc_gen_interpret(ppc_opc_extshx);
	return flowEndBlock;
}

/*
 *	mulhwx		Multiply High Word
 *	.595
 */
void ppc_opc_mulhwx()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
	sint64 a = (sint32)gCPU.gpr[rA];
	sint64 b = (sint32)gCPU.gpr[rB];
	sint64 c = a*b;
	gCPU.gpr[rD] = ((uint64)c)>>32;
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rD]);
//		PPC_ALU_WARN("mulhw. correct?\n");
	}
}
JITCFlow ppc_opc_gen_mulhwx()
{
	ppc_opc_gen_interpret(ppc_opc_mulhwx);
	return flowEndBlock;
}
/*
 *	mulhwux		Multiply High Word Unsigned
 *	.596
 */
void ppc_opc_mulhwux()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
	uint64 a = gCPU.gpr[rA];
	uint64 b = gCPU.gpr[rB];
	uint64 c = a*b;
	gCPU.gpr[rD] = c>>32;
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rD]);
	}
}
JITCFlow ppc_opc_gen_mulhwux()
{
	ppc_opc_gen_interpret(ppc_opc_mulhwux);
	return flowEndBlock;
}
/*
 *	mulli		Multiply Low Immediate
 *	.598
 */
void ppc_opc_mulli()
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, rD, rA, imm);
	// FIXME: signed / unsigned correct?
	gCPU.gpr[rD] = gCPU.gpr[rA] * imm;
}
JITCFlow ppc_opc_gen_mulli()
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, rD, rA, imm);
	jitcClobberCarryAndFlags();
	NativeReg a = jitcGetClientRegister(PPC_GPR(rA));
	NativeReg d = jitcMapClientRegisterDirty(PPC_GPR(rD));
	asmIMULRegRegImm(d, a, imm);
	return flowContinue;
}
/*
 *	mullwx		Multiply Low Word
 *	.599
 */
void ppc_opc_mullwx()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
	gCPU.gpr[rD] = gCPU.gpr[rA] * gCPU.gpr[rB];
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rD]);
	}
	if (gCPU.current_opc & PPC_OPC_OE) {
		// update XER flags
		PPC_ALU_ERR("mullwx unimplemented\n");
	}
}
JITCFlow ppc_opc_gen_mullwx()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gJITC.current_opc, rD, rA, rB);
	if (gJITC.current_opc & PPC_OPC_Rc) {
		jitcClobberCarry();
	} else {
		jitcClobberCarryAndFlags();
	}
	NativeReg a = jitcGetClientRegister(PPC_GPR(rA));
	NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
	NativeReg d;
	if (rA == rD) {
		d = a;
		jitcDirtyRegister(a);
	} else if (rB == rD) {
		d = b;
		jitcDirtyRegister(b);
		b = a;
	} else {
		d = jitcMapClientRegisterDirty(PPC_GPR(rD));
		asmALURegReg(X86_MOV, d, a);
	}
	// now: d *= b
	asmIMULRegReg(d, b);
	if (gJITC.current_opc & PPC_OPC_Rc) {
		asmALURegReg(X86_OR, d, d);
		jitcMapFlagsDirty();
	}
	return flowContinue;
/*	ppc_opc_gen_interpret(ppc_opc_mullwx);
	return flowEndBlock;*/
}

/*
 *	nandx		NAND
 *	.600
 */
void ppc_opc_nandx()
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, rB);
	gCPU.gpr[rA] = ~(gCPU.gpr[rS] & gCPU.gpr[rB]);
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_nandx()
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, rA, rB);
	if (gJITC.current_opc & PPC_OPC_Rc) {
		jitcClobberCarry();
	} else {
		jitcClobberCarryAndFlags();
	}
	NativeReg a;
	if (rS == rB) {
	        if (rA == rS) {
			a = jitcGetClientRegisterDirty(PPC_GPR(rA));
		} else {
			NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
			a = jitcMapClientRegisterDirty(PPC_GPR(rA));
			asmALURegReg(X86_MOV, a, s);
		}
	} else if (rA == rS) {
		a = jitcGetClientRegisterDirty(PPC_GPR(rA));
		NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
		asmALURegReg(X86_AND, a, b);
	} else if (rA == rB) {
		a = jitcGetClientRegisterDirty(PPC_GPR(rA));
		NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
		asmALURegReg(X86_AND, a, s);
	} else {
		NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
		NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
		a = jitcMapClientRegisterDirty(PPC_GPR(rA));
		asmALURegReg(X86_MOV, a, s);
		asmALURegReg(X86_AND, a, b);
	}
	asmALUReg(X86_NOT, a);
	if (gJITC.current_opc & PPC_OPC_Rc) {
		// "NOT" doesn't update the flags
		asmALURegReg(X86_OR, a, a);
		jitcMapFlagsDirty();
	}
	return flowContinue;
}

/*
 *	negx		Negate
 *	.601
 */
void ppc_opc_negx()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
	PPC_OPC_ASSERT(rB == 0);
	gCPU.gpr[rD] = -gCPU.gpr[rA];
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rD]);
	}
}
JITCFlow ppc_opc_gen_negx()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gJITC.current_opc, rD, rA, rB);
	if (gJITC.current_opc & PPC_OPC_Rc) {
		jitcClobberCarry();
	} else {
		jitcClobberCarryAndFlags();
	}
	if (rA == rD) {
		NativeReg d = jitcGetClientRegisterDirty(PPC_GPR(rD));
		asmALUReg(X86_NEG, d);
	} else {
		NativeReg a = jitcGetClientRegister(PPC_GPR(rA));
		NativeReg d = jitcMapClientRegisterDirty(PPC_GPR(rD));
		asmALURegReg(X86_MOV, d, a);
		asmALUReg(X86_NEG, d);
	}
	if (gJITC.current_opc & PPC_OPC_Rc) {
		jitcMapFlagsDirty();
	}
	return flowContinue;
}
/*
 *	negox		Negate with Overflow
 *	.601
 */
void ppc_opc_negox()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
	PPC_OPC_ASSERT(rB == 0);
	gCPU.gpr[rD] = -gCPU.gpr[rA];
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rD]);
	}
	// update XER flags
	PPC_ALU_ERR("negox unimplemented\n");
}
/*
 *	norx		NOR
 *	.602
 */
void ppc_opc_norx()
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, rB);
	gCPU.gpr[rA] = ~(gCPU.gpr[rS] | gCPU.gpr[rB]);
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_norx()
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, rA, rB);
	NativeReg a;
	if (rS == rB) {
		// norx rA, rS, rS == not rA, rS
		// not doen't clobber the flags
		if (gJITC.current_opc & PPC_OPC_Rc) {
			jitcClobberCarry();
		}
	        if (rA == rS) {
			a = jitcGetClientRegisterDirty(PPC_GPR(rA));
		} else {
			NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
			a = jitcMapClientRegisterDirty(PPC_GPR(rA));
			asmALURegReg(X86_MOV, a, s);
		}
	} else {
		if (gJITC.current_opc & PPC_OPC_Rc) {
			jitcClobberCarry();
		} else {
			jitcClobberCarryAndFlags();
		}
		if (rA == rS) {
			a = jitcGetClientRegisterDirty(PPC_GPR(rA));
			NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
			asmALURegReg(X86_OR, a, b);
		} else if (rA == rB) {
			a = jitcGetClientRegisterDirty(PPC_GPR(rA));
			NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
			asmALURegReg(X86_OR, a, s);
		} else {
			NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
			NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
			a = jitcMapClientRegisterDirty(PPC_GPR(rA));
			asmALURegReg(X86_MOV, a, s);
			asmALURegReg(X86_OR, a, b);
		}
	}
	asmALUReg(X86_NOT, a);
	if (gJITC.current_opc & PPC_OPC_Rc) {
		// "NOT" doesn't update the flags
		asmALURegReg(X86_OR, a, a);
		jitcMapFlagsDirty();
	}
	return flowContinue;
}

/*
 *	orx		OR
 *	.603
 */
void ppc_opc_orx()
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, rB);
	gCPU.gpr[rA] = gCPU.gpr[rS] | gCPU.gpr[rB];
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_or()
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, rA, rB);
	if (rS == rB) {
		if (rS == rA) {
			/* nop */
		} else {
			/* mr rA, rS*/
			NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
			NativeReg a = jitcMapClientRegisterDirty(PPC_GPR(rA));
			asmALURegReg(X86_MOV, a, s);
		}
	} else {
		if (rA == rS) {
			// or a, a, b
			NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
			jitcClobberCarryAndFlags();
			NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
			asmALURegReg(X86_OR, a, b);
		} else if (rA == rB) {
			// or a, s, a
			NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
			jitcClobberCarryAndFlags();
			NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
			asmALURegReg(X86_OR, a, s);
		} else {
			// or a, s, b
			NativeReg a = jitcMapClientRegisterDirty(PPC_GPR(rA));
			jitcClobberCarryAndFlags();
			NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
			NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
			asmALURegReg(X86_MOV, a, s);
			asmALURegReg(X86_OR, a, b);			
		}
	}
	return flowContinue;
}
JITCFlow ppc_opc_gen_orp()
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, rA, rB);
	jitcClobberCarry();
	if (rS == rB) {
		if (rS == rA) {
			/* mr. rA, rA */
			NativeReg a = jitcGetClientRegister(PPC_GPR(rA));
			asmALURegReg(X86_OR, a, a);
		} else {
			/* mr. rA, rS*/
			NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
			NativeReg a = jitcMapClientRegisterDirty(PPC_GPR(rA));
			asmALURegReg(X86_MOV, a, s);
			asmALURegReg(X86_OR, a, a);
		}
	} else {
		if (rA == rS) {
			// or a, a, b
			NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
			NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
			asmALURegReg(X86_OR, a, b);
		} else if (rA == rB) {
			// or a, s, a
			NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
			NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
			asmALURegReg(X86_OR, a, s);
		} else {
			// or a, s, b
			NativeReg a = jitcMapClientRegisterDirty(PPC_GPR(rA));
			NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
			NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
			asmALURegReg(X86_MOV, a, s);
			asmALURegReg(X86_OR, a, b);			
		}
	}
	jitcMapFlagsDirty();
	return flowContinue;
}
JITCFlow ppc_opc_gen_orx()
{
	if (gJITC.current_opc & PPC_OPC_Rc) {
		return ppc_opc_gen_orp();
	} else {
		return ppc_opc_gen_or();
	}
}
/*
 *	orcx		OR with Complement
 *	.604
 */
void ppc_opc_orcx()
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, rB);
	gCPU.gpr[rA] = gCPU.gpr[rS] | ~gCPU.gpr[rB];
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_orcx()
{
	ppc_opc_gen_interpret(ppc_opc_orcx);
	return flowEndBlock;
}
/*
 *	ori		OR Immediate
 *	.605
 */
void ppc_opc_ori()
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_UImm(gCPU.current_opc, rS, rA, imm);
	gCPU.gpr[rA] = gCPU.gpr[rS] | imm;
}
JITCFlow ppc_opc_gen_ori()
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_UImm(gJITC.current_opc, rS, rA, imm);
	return ppc_opc_gen_ori_oris_xori_xoris(X86_OR, imm, rS, rA);
}
/*
 *	oris		OR Immediate Shifted
 *	.606
 */
void ppc_opc_oris()
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_Shift16(gCPU.current_opc, rS, rA, imm);
	gCPU.gpr[rA] = gCPU.gpr[rS] | imm;
}
JITCFlow ppc_opc_gen_oris()
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_Shift16(gJITC.current_opc, rS, rA, imm);
	return ppc_opc_gen_ori_oris_xori_xoris(X86_OR, imm, rS, rA);
}
/*
 *	rlwimix		Rotate Left Word Immediate then Mask Insert
 *	.617
 */
void ppc_opc_rlwimix()
{
	int rS, rA, SH, MB, ME;
	PPC_OPC_TEMPL_M(gCPU.current_opc, rS, rA, SH, MB, ME);
	uint32 v = ppc_word_rotl(gCPU.gpr[rS], SH);
	uint32 mask = ppc_mask(MB, ME);
	gCPU.gpr[rA] = (v & mask) | (gCPU.gpr[rA] & ~mask);
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_rlwimix()
{
	int rS, rA, SH, MB, ME;
	PPC_OPC_TEMPL_M(gJITC.current_opc, rS, rA, SH, MB, ME);
	if (gJITC.current_opc & PPC_OPC_Rc) {
		jitcClobberCarry();
	} else {
		jitcClobberCarryAndFlags();
	}
	uint32 mask = ppc_mask(MB, ME);
	NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
	NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
	NativeReg tmp = jitcAllocRegister();
	asmALURegReg(X86_MOV, tmp, s);
	if (SH & 0x1f) asmShiftRegImm(X86_ROL, tmp, SH & 0x1f);
	asmALURegImm(X86_AND, a, ~mask);
	asmALURegImm(X86_AND, tmp, mask);
	asmALURegReg(X86_OR, a, tmp);
	if (gJITC.current_opc & PPC_OPC_Rc) {
		jitcMapFlagsDirty();
	}
	return flowContinue;
}
/*
 *	rlwinmx		Rotate Left Word Immediate then AND with Mask
 *	.618
 */
void ppc_opc_rlwinmx()
{
	int rS, rA, SH;
	uint32 MB, ME;
	PPC_OPC_TEMPL_M(gCPU.current_opc, rS, rA, SH, MB, ME);
	uint32 v = ppc_word_rotl(gCPU.gpr[rS], SH);
	uint32 mask = ppc_mask(MB, ME);
	gCPU.gpr[rA] = v & mask;
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_rlwinmx()
{
	int rS, rA, SH;
	uint32 MB, ME;
	PPC_OPC_TEMPL_M(gJITC.current_opc, rS, rA, SH, MB, ME);
	if (gJITC.current_opc & PPC_OPC_Rc) {
		jitcClobberCarry();
	} else {
		jitcClobberCarryAndFlags();
	}
	NativeReg a;
	if (rS == rA) {
		a = jitcGetClientRegisterDirty(PPC_GPR(rA));
	} else {
		NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
		a = jitcMapClientRegisterDirty(PPC_GPR(rA));
		asmALURegReg(X86_MOV, a, s);
	}
	if (SH & 0x1f) asmShiftRegImm(X86_ROL, a, SH & 0x1f);
	uint32 mask = ppc_mask(MB, ME);
	asmALURegImm(X86_AND, a, mask);
	if (gJITC.current_opc & PPC_OPC_Rc) {
		jitcMapFlagsDirty();
	}
	return flowContinue;
}
/*
 *	rlwnmx		Rotate Left Word then AND with Mask
 *	.620
 */
void ppc_opc_rlwnmx()
{
	int rS, rA, rB, MB, ME;
	PPC_OPC_TEMPL_M(gCPU.current_opc, rS, rA, rB, MB, ME);
	uint32 v = ppc_word_rotl(gCPU.gpr[rS], gCPU.gpr[rB]);
	uint32 mask = ppc_mask(MB, ME);
	gCPU.gpr[rA] = v & mask;
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_rlwnmx()
{
	int rS, rA, rB, MB, ME;
	PPC_OPC_TEMPL_M(gJITC.current_opc, rS, rA, rB, MB, ME);
	if (gJITC.current_opc & PPC_OPC_Rc) {
		jitcClobberCarry();
	} else {
		jitcClobberCarryAndFlags();
	}
	NativeReg a;
	jitcGetClientRegister(PPC_GPR(rB), NATIVE_REG | ECX);
	if (rS == rA) {
		a = jitcGetClientRegisterDirty(PPC_GPR(rA));
	} else {
		NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
		if (rA == rB) {
			a = jitcAllocRegister();
		} else {
			a = jitcMapClientRegisterDirty(PPC_GPR(rA));
		}
		asmALURegReg(X86_MOV, a, s);
	}
	asmShiftRegCL(X86_ROL, a);
	if (rA != rS && rA == rB) {
		jitcMapClientRegisterDirty(PPC_GPR(rA), NATIVE_REG | a);
	}
	uint32 mask = ppc_mask(MB, ME);
	asmALURegImm(X86_AND, a, mask);
	if (gJITC.current_opc & PPC_OPC_Rc) {
		/*
		 *	Important side-node:
		 *	ROL doesn't update the flags, so beware if you want to
		 *	get rid of the above AND
		 */
		jitcMapFlagsDirty();
	}
	return flowContinue;
}

/*
 *	slwx		Shift Left Word
 *	.625
 */
void ppc_opc_slwx()
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, rB);
	uint32 s = gCPU.gpr[rB] & 0x3f;
	if (s > 31) {
		gCPU.gpr[rA] = 0;
	} else {
		gCPU.gpr[rA] = gCPU.gpr[rS] << s;
	}
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_slwx()
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, rA, rB);
	if (gJITC.current_opc & PPC_OPC_Rc) {
		jitcClobberCarry();
	} else {
		jitcClobberCarryAndFlags();
	}
	NativeReg a;
	NativeReg b = jitcGetClientRegister(PPC_GPR(rB), NATIVE_REG | ECX);
	asmALURegImm(X86_TEST, b, 0x20);
	if (rA == rS) {
		a = jitcGetClientRegisterDirty(PPC_GPR(rA));
	} else {
		NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
		if (rA == rB) {
			a = jitcAllocRegister();
		} else {
			a = jitcMapClientRegisterDirty(PPC_GPR(rA));
		}
		asmALURegReg(X86_MOV, a, s);
	}
	NativeAddress fixup = asmJxxFixup(X86_Z);
	asmALURegImm(X86_MOV, a, 0);
	asmResolveFixup(fixup, asmHERE());
	asmShiftRegCL(X86_SHL, a);
	if (rA != rS && rA == rB) {
		jitcMapClientRegisterDirty(PPC_GPR(rA), NATIVE_REG | a);
	}
	if (gJITC.current_opc & PPC_OPC_Rc) {
		/*
		 *	Welcome to the wonderful world of braindead 
		 *	processor design.
		 *	(shl x, cl doesn't update the flags in case of cl==0)
		 */
		asmALURegReg(X86_OR, a, a);
		jitcMapFlagsDirty();
	}
	return flowContinue;
/*	ppc_opc_gen_interpret(ppc_opc_slwx);
	return flowEndBlock;*/
}
/*
 *	srawx		Shift Right Algebraic Word
 *	.628
 */
void ppc_opc_srawx()
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, rB);
	uint32 SH = gCPU.gpr[rB] & 0x3f;
	gCPU.gpr[rA] = gCPU.gpr[rS];
	gCPU.xer_ca = 0;
	if (gCPU.gpr[rA] & 0x80000000) {
		uint32 ca = 0;
		for (uint i=0; i < SH; i++) {
			if (gCPU.gpr[rA] & 1) ca = 1;
			gCPU.gpr[rA] >>= 1;
			gCPU.gpr[rA] |= 0x80000000;
		}
		if (ca) gCPU.xer_ca = 1;
	} else {
		if (SH > 31) {
			gCPU.gpr[rA] = 0;
		} else {
			gCPU.gpr[rA] >>= SH;
		}
	}     
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_srawx()
{
	ppc_opc_gen_interpret(ppc_opc_srawx);
	return flowEndBlock;
}
/*
 *	srawix		Shift Right Algebraic Word Immediate
 *	.629
 */
void ppc_opc_srawix()
{
	int rS, rA;
	uint32 SH;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, SH);
	gCPU.gpr[rA] = gCPU.gpr[rS];
	gCPU.xer_ca = 0;
	if (gCPU.gpr[rA] & 0x80000000) {
		uint32 ca = 0;
		for (uint i=0; i < SH; i++) {
			if (gCPU.gpr[rA] & 1) ca = 1;
			gCPU.gpr[rA] >>= 1;
			gCPU.gpr[rA] |= 0x80000000;
		}
		if (ca) gCPU.xer_ca = 1;
	} else {
		gCPU.gpr[rA] >>= SH;
	}     
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_srawix()
{
	int rS, rA;
	uint32 SH;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, rA, SH);
	if (gJITC.current_opc & PPC_OPC_Rc) {
		jitcClobberCarry();
	} else {
		jitcClobberCarryAndFlags();
	}
	NativeReg a = REG_NO;
	if (SH) {
		NativeReg t;
		if (rS != rA) {
			NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
			a = jitcMapClientRegisterDirty(PPC_GPR(rA));
			asmALURegReg(X86_MOV, a, s);
			t = jitcAllocRegister();
			asmALURegReg(X86_MOV, t, s);
		} else {
			a = jitcGetClientRegisterDirty(PPC_GPR(rA));
			t = jitcAllocRegister();
			asmALURegReg(X86_MOV, t, a);
		}
		asmShiftRegImm(X86_SAR, t, 31);
		asmALURegReg(X86_AND, t, a);
		asmShiftRegImm(X86_SAR, a, SH);
		asmALURegImm(X86_TEST, t, (1<<SH)-1);
		byte modrm[6];
    		asmSETMem(X86_NZ, modrm, x86_mem(modrm, REG_NO, (uint32)&gCPU.xer_ca));
	} else {
		if (rS != rA) {
			NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
			a = jitcMapClientRegisterDirty(PPC_GPR(rA));
			asmALURegReg(X86_MOV, a, s);
		} else if (gJITC.current_opc & PPC_OPC_Rc) {
			a = jitcGetClientRegister(PPC_GPR(rA));
		}
		byte modrm[6];
		asmALUMemImm8(X86_MOV, modrm, x86_mem(modrm, REG_NO, (uint32)&gCPU.xer_ca), 0);
	}
	if (gJITC.current_opc & PPC_OPC_Rc) {
		asmALURegReg(X86_TEST, a, a);
		jitcMapFlagsDirty();
	}
	return flowContinue;
}
/*
 *	srwx		Shift Right Word
 *	.631
 */
void ppc_opc_srwx()
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, rB);
	uint32 v = gCPU.gpr[rB] & 0x3f;
	if (v > 31) {
		gCPU.gpr[rA] = 0;
	} else {
		gCPU.gpr[rA] = gCPU.gpr[rS] >> v;
	}
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_srwx()
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, rA, rB);
	if (gJITC.current_opc & PPC_OPC_Rc) {
		jitcClobberCarry();
	} else {
		jitcClobberCarryAndFlags();
	}
	NativeReg a;
	NativeReg b = jitcGetClientRegister(PPC_GPR(rB), NATIVE_REG | ECX);
	asmALURegImm(X86_TEST, b, 0x20);
	if (rA == rS) {
		a = jitcGetClientRegisterDirty(PPC_GPR(rA));
	} else {
		NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
		if (rA == rB) {
			a = jitcAllocRegister();
		} else {
			a = jitcMapClientRegisterDirty(PPC_GPR(rA));
		}
		asmALURegReg(X86_MOV, a, s);
	}
	NativeAddress fixup = asmJxxFixup(X86_Z);
	asmALURegImm(X86_MOV, a, 0);
	asmResolveFixup(fixup, asmHERE());
	asmShiftRegCL(X86_SHR, a);
	if (rA != rS && rA == rB) {
		jitcMapClientRegisterDirty(PPC_GPR(rA), NATIVE_REG | a);
	}
	if (gJITC.current_opc & PPC_OPC_Rc) {
		/*
		 *	Welcome to the wonderful world of braindead 
		 *	processor design.
		 *	(shr x, cl doesn't update the flags in case of cl==0)
		 */
		asmALURegReg(X86_OR, a, a);
		jitcMapFlagsDirty();
	}
	return flowContinue;
/*	ppc_opc_gen_interpret(ppc_opc_srwx);
	return flowEndBlock;*/
}

/*
 *	subfx		Subtract From
 *	.666
 */
void ppc_opc_subfx()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
	gCPU.gpr[rD] = ~gCPU.gpr[rA] + gCPU.gpr[rB] + 1;
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rD]);
	}
}
JITCFlow ppc_opc_gen_subfx()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gJITC.current_opc, rD, rA, rB);
	if (gJITC.current_opc & PPC_OPC_Rc) {
		jitcClobberCarry();
	} else {
		jitcClobberCarryAndFlags();
	}
	NativeReg a = jitcGetClientRegister(PPC_GPR(rA));
	NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
	if (rD == rA) {
		if (rD == rB) {
			asmALURegImm(X86_MOV, a, 0);
		} else {
			// subf rA, rA, rB (a = b - a)
			asmALUReg(X86_NEG, a);
			asmALURegReg(X86_ADD, a, b);
		}
		jitcDirtyRegister(a);
	} else if (rD == rB) {
		// subf rB, rA, rB (b = b - a)
		asmALURegReg(X86_SUB, b, a);
		jitcDirtyRegister(b);
	} else {
		// subf rD, rA, rB (d = b - a)
		NativeReg d = jitcMapClientRegisterDirty(PPC_GPR(rD));
		asmALURegReg(X86_MOV, d, b);
		asmALURegReg(X86_SUB, d, a);
	}
	if (gJITC.current_opc & PPC_OPC_Rc) {
		jitcMapFlagsDirty();
	}
	return flowContinue;
}
/*
 *	subfox		Subtract From with Overflow
 *	.666
 */
void ppc_opc_subfox()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
	gCPU.gpr[rD] = ~gCPU.gpr[rA] + gCPU.gpr[rB] + 1;
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rD]);
	}
	// update XER flags
	PPC_ALU_ERR("subfox unimplemented\n");
}
/*
 *	subfcx		Subtract From Carrying
 *	.667
 */
void ppc_opc_subfcx()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
	uint32 a = gCPU.gpr[rA];
	uint32 b = gCPU.gpr[rB];
	gCPU.gpr[rD] = ~a + b + 1;
	gCPU.xer_ca = ppc_carry_3(~a, b, 1);
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rD]);
	}
}
JITCFlow ppc_opc_gen_subfcx()
{
	ppc_opc_gen_interpret(ppc_opc_subfcx);
	return flowEndBlock;
}
/*
 *	subfcox		Subtract From Carrying with Overflow
 *	.667
 */
void ppc_opc_subfcox()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
	uint32 a = gCPU.gpr[rA];
	uint32 b = gCPU.gpr[rB];
	gCPU.gpr[rD] = ~a + b + 1;
	gCPU.xer_ca = (ppc_carry_3(~a, b, 1));
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rD]);
	}
	// update XER flags
	PPC_ALU_ERR("subfcox unimplemented\n");
}
/*
 *	subfex		Subtract From Extended
 *	.668
 */
void ppc_opc_subfex()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
	uint32 a = gCPU.gpr[rA];
	uint32 b = gCPU.gpr[rB];
	uint32 ca = (gCPU.xer_ca);
	gCPU.gpr[rD] = ~a + b + ca;
	gCPU.xer_ca = (ppc_carry_3(~a, b, ca));
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rD]);
	}
}
JITCFlow ppc_opc_gen_subfex()
{
	ppc_opc_gen_interpret(ppc_opc_subfex);
	return flowEndBlock;
}
/*
 *	subfeox		Subtract From Extended with Overflow
 *	.668
 */
void ppc_opc_subfeox()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
	uint32 a = gCPU.gpr[rA];
	uint32 b = gCPU.gpr[rB];
	uint32 ca = gCPU.xer_ca;
	gCPU.gpr[rD] = ~a + b + ca;
	gCPU.xer_ca = (ppc_carry_3(~a, b, ca));
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rD]);
	}
	// update XER flags
	PPC_ALU_ERR("subfeox unimplemented\n");
}
/*
 *	subfic		Subtract From Immediate Carrying
 *	.669
 */
void ppc_opc_subfic()
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gCPU.current_opc, rD, rA, imm);
	uint32 a = gCPU.gpr[rA];
	gCPU.gpr[rD] = ~a + imm + 1;
	gCPU.xer_ca = (ppc_carry_3(~a, imm, 1));
}
JITCFlow ppc_opc_gen_subfic()
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(gJITC.current_opc, rD, rA, imm);
	jitcClobberFlags();
	NativeReg d;
	if (rA == rD) {
		d = jitcGetClientRegisterDirty(PPC_GPR(rD));
	} else {
		NativeReg a = jitcGetClientRegister(PPC_GPR(rA));
		d = jitcMapClientRegisterDirty(PPC_GPR(rD));
		asmALURegReg(X86_MOV, d, a);
	}
	asmALUReg(X86_NOT, d);
	if (imm == 0xffffffff) {
		asmSimple(X86_STC);
	} else {
		asmALURegImm(X86_ADD, d, imm+1);
	}
	jitcMapCarryDirty();
	return flowContinue;
}
/*
 *	subfmex		Subtract From Minus One Extended
 *	.670
 */
void ppc_opc_subfmex()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
	PPC_OPC_ASSERT(rB == 0);
	uint32 a = gCPU.gpr[rA];
	uint32 ca = gCPU.xer_ca;
	gCPU.gpr[rD] = ~a + ca + 0xffffffff;
	gCPU.xer_ca = ((a!=0xffffffff) || ca);
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rD]);
	}
}
JITCFlow ppc_opc_gen_subfmex()
{
	ppc_opc_gen_interpret(ppc_opc_subfmex);
	return flowEndBlock;
}
/*
 *	subfmeox	Subtract From Minus One Extended with Overflow
 *	.670
 */
void ppc_opc_subfmeox()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
	PPC_OPC_ASSERT(rB == 0);
	uint32 a = gCPU.gpr[rA];
	uint32 ca = gCPU.xer_ca;
	gCPU.gpr[rD] = ~a + ca + 0xffffffff;
	gCPU.xer_ca = ((a!=0xffffffff) || ca);
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rD]);
	}
	// update XER flags
	PPC_ALU_ERR("subfmeox unimplemented\n");
}
/*
 *	subfzex		Subtract From Zero Extended
 *	.671
 */
void ppc_opc_subfzex()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
	PPC_OPC_ASSERT(rB == 0);
	uint32 a = gCPU.gpr[rA];
	uint32 ca = gCPU.xer_ca;
	gCPU.gpr[rD] = ~a + ca;
	gCPU.xer_ca = (!a && ca);
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rD]);
	}
}
JITCFlow ppc_opc_gen_subfzex()
{
	ppc_opc_gen_interpret(ppc_opc_subfzex);
	return flowEndBlock;
}
/*
 *	subfzeox	Subtract From Zero Extended with Overflow
 *	.671
 */
void ppc_opc_subfzeox()
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(gCPU.current_opc, rD, rA, rB);
	PPC_OPC_ASSERT(rB == 0);
	uint32 a = gCPU.gpr[rA];
	uint32 ca = gCPU.xer_ca;
	gCPU.gpr[rD] = ~a + ca;
	gCPU.xer_ca = (!a && ca);
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rD]);
	}
	// update XER flags
	PPC_ALU_ERR("subfzeox unimplemented\n");
}

/*
 *	xorx		XOR
 *	.680
 */
void ppc_opc_xorx()
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, rS, rA, rB);
	gCPU.gpr[rA] = gCPU.gpr[rS] ^ gCPU.gpr[rB];
	if (gCPU.current_opc & PPC_OPC_Rc) {
		// update cr0 flags
		ppc_update_cr0(gCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_xorx()
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, rS, rA, rB);
	if (gJITC.current_opc & PPC_OPC_Rc) {
		jitcClobberCarry();
	} else {
		jitcClobberCarryAndFlags();
	}
	if (rA == rS) {
		NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
		NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
		asmALURegReg(X86_XOR, a, b);
	} else if (rA == rB) {
		NativeReg a = jitcGetClientRegisterDirty(PPC_GPR(rA));
		NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
		asmALURegReg(X86_XOR, a, s);
	} else {
		NativeReg s = jitcGetClientRegister(PPC_GPR(rS));
		NativeReg b = jitcGetClientRegister(PPC_GPR(rB));
		NativeReg a = jitcMapClientRegisterDirty(PPC_GPR(rA));
		asmALURegReg(X86_MOV, a, s);
		asmALURegReg(X86_XOR, a, b);
	}
	if (gJITC.current_opc & PPC_OPC_Rc) {
		jitcMapFlagsDirty();
	}
	return flowContinue;
}
/*
 *	xori		XOR Immediate
 *	.681
 */
void ppc_opc_xori()
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_UImm(gCPU.current_opc, rS, rA, imm);
	gCPU.gpr[rA] = gCPU.gpr[rS] ^ imm;
}
JITCFlow ppc_opc_gen_xori()
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_UImm(gJITC.current_opc, rS, rA, imm);
	return ppc_opc_gen_ori_oris_xori_xoris(X86_XOR, imm, rS, rA);
}
/*
 *	xoris		XOR Immediate Shifted
 *	.682
 */
void ppc_opc_xoris()
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_Shift16(gCPU.current_opc, rS, rA, imm);
	gCPU.gpr[rA] = gCPU.gpr[rS] ^ imm;
}
JITCFlow ppc_opc_gen_xoris()
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_Shift16(gJITC.current_opc, rS, rA, imm);
	return ppc_opc_gen_ori_oris_xori_xoris(X86_XOR, imm, rS, rA);
}
