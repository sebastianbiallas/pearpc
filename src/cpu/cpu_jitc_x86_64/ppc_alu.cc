/*
 *	PearPC
 *	ppc_alu.cc
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

static JITCFlow ppc_opc_gen_ori_oris_xori_xoris(JITC &jitc, X86ALUopc opc, uint32 imm, int rS, int rA)
{
	if (imm) {
		jitc.clobberCarryAndFlags();
		if (rA == rS) {
			NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
			jitc.asmALU32(opc, a, imm);
		} else {
			NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
			NativeReg a = jitc.mapClientRegisterDirty(PPC_GPR(rA));
			jitc.asmALU32(X86_MOV, a, s);
			jitc.asmALU32(opc, a, imm);
		}
	} else {
		if (rA == rS) {
			/* nop */
		} else {
			/* mov */
			NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
			NativeReg a = jitc.mapClientRegisterDirty(PPC_GPR(rA));
			jitc.asmALU32(X86_MOV, a, s);
		}
	}
	return flowContinue;
}

/*
 *	addx		Add
 *	.422
 */
void ppc_opc_addx(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
	aCPU.gpr[rD] = aCPU.gpr[rA] + aCPU.gpr[rB];
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rD]);
	}
}
static JITCFlow ppc_opc_gen_add(JITC &jitc)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(jitc.current_opc, rD, rA, rB);
	NativeReg a = jitc.getClientRegister(PPC_GPR(rA));
	NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
	if (rD == rA) {
		// add rA, rA, rB
		jitc.clobberCarryAndFlags();
		jitc.asmALU32(X86_ADD, a, b);
		jitc.dirtyRegister(a);
	} else if (rD == rB) {
		// add rB, rA, rB
		jitc.clobberCarryAndFlags();
		jitc.asmALU32(X86_ADD, b, a);
		jitc.dirtyRegister(b);
	} else {
		// add rD, rA, rB
		NativeReg result = jitc.mapClientRegisterDirty(PPC_GPR(rD));
		// lea result, [a+1*b+0]
		jitc.asmALU32(X86_LEA, result, a, 1, b, 0);
		// result already is dirty
	}
	return flowContinue;
}
static JITCFlow ppc_opc_gen_addp(JITC &jitc)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(jitc.current_opc, rD, rA, rB);
	jitc.clobberCarry();
	NativeReg a = jitc.getClientRegister(PPC_GPR(rA));
	NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
	if (rD == rA) {
		// add r1, r1, r2
		jitc.asmALU32(X86_ADD, a, b);
		jitc.dirtyRegister(a);
	} else if (rD == rB) {
		// add r1, r2, r1
		jitc.asmALU32(X86_ADD, b, a);
		jitc.dirtyRegister(b);
	} else {
		// add r3, r1, r2
		NativeReg result = jitc.mapClientRegisterDirty(PPC_GPR(rD));
		// lea doesn't update the flags
		jitc.asmALU32(X86_MOV, result, a);
		jitc.asmALU32(X86_ADD, result, b);
	}
	jitc.mapFlagsDirty();
	return flowContinue;
}
JITCFlow ppc_opc_gen_addx(JITC &jitc)
{
	if (jitc.current_opc & PPC_OPC_Rc) {
		return ppc_opc_gen_addp(jitc);
	} else {
		return ppc_opc_gen_add(jitc);
	}
}
/*
 *	addox		Add with Overflow
 *	.422
 */
void ppc_opc_addox(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
	aCPU.gpr[rD] = aCPU.gpr[rA] + aCPU.gpr[rB];
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rD]);
	}
	// update XER flags
	PPC_ALU_ERR("addox unimplemented\n");
}
/*
 *	addcx		Add Carrying
 *	.423
 */
void ppc_opc_addcx(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
	uint32 a = aCPU.gpr[rA];
	aCPU.gpr[rD] = a + aCPU.gpr[rB];
	aCPU.xer_ca = (aCPU.gpr[rD] < a);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rD]);
	}
}
JITCFlow ppc_opc_gen_addcx(JITC &jitc)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(jitc.current_opc, rD, rA, rB);
	if (!(jitc.current_opc & PPC_OPC_Rc)) jitc.clobberFlags();
	NativeReg a = jitc.getClientRegister(PPC_GPR(rA));
	NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
	if (rD == rA) {
		// add r1, r1, r2
		jitc.asmALU32(X86_ADD, a, b);
		jitc.dirtyRegister(a);
	} else if (rD == rB) {
		// add r1, r2, r1
		jitc.asmALU32(X86_ADD, b, a);
		jitc.dirtyRegister(b);
	} else {
		// add r3, r1, r2
		NativeReg result = jitc.mapClientRegisterDirty(PPC_GPR(rD));
		// lea doesn't update the carry
		jitc.asmALU32(X86_MOV, result, a);
		jitc.asmALU32(X86_ADD, result, b);
	}
	jitc.mapCarryDirty();
	if (jitc.current_opc & PPC_OPC_Rc) jitc.mapFlagsDirty();
	return flowContinue;
}
/*
 *	addcox		Add Carrying with Overflow
 *	.423
 */
void ppc_opc_addcox(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
	uint32 a = aCPU.gpr[rA];
	aCPU.gpr[rD] = a + aCPU.gpr[rB];
	aCPU.xer_ca = (aCPU.gpr[rD] < a);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rD]);
	}
	// update XER flags
	PPC_ALU_ERR("addcox unimplemented\n");
}
/*
 *	addex		Add Extended
 *	.424
 */
void ppc_opc_addex(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
	uint32 a = aCPU.gpr[rA];
	uint32 b = aCPU.gpr[rB];
	uint32 ca = aCPU.xer_ca;
	aCPU.gpr[rD] = a + b + ca;
	aCPU.xer_ca = ppc_carry_3(a, b, ca);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rD]);
	}
}
JITCFlow ppc_opc_gen_addex(JITC &jitc)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(jitc.current_opc, rD, rA, rB);
	if (!(jitc.current_opc & PPC_OPC_Rc)) jitc.clobberFlags();
	jitc.getClientCarry();
	NativeReg a = jitc.getClientRegister(PPC_GPR(rA));
	NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
	if (rD == rA) {
		// add r1, r1, r2
		jitc.asmALU32(X86_ADC, a, b);
		jitc.dirtyRegister(a);
	} else if (rD == rB) {
		// add r1, r2, r1
		jitc.asmALU32(X86_ADC, b, a);
		jitc.dirtyRegister(b);
	} else {
		// add r3, r1, r2
		NativeReg result = jitc.mapClientRegisterDirty(PPC_GPR(rD));
		// lea doesn't update the carry
		jitc.asmALU32(X86_MOV, result, a);
		jitc.asmALU32(X86_ADC, result, b);
	}
	jitc.mapCarryDirty();
	if (jitc.current_opc & PPC_OPC_Rc) jitc.mapFlagsDirty();
	return flowContinue;
}
/*
 *	addeox		Add Extended with Overflow
 *	.424
 */
void ppc_opc_addeox(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
	uint32 a = aCPU.gpr[rA];
	uint32 b = aCPU.gpr[rB];
	uint32 ca = aCPU.xer_ca;
	aCPU.gpr[rD] = a + b + ca;
	aCPU.xer_ca = ppc_carry_3(a, b, ca);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rD]);
	}
	// update XER flags
	PPC_ALU_ERR("addeox unimplemented\n");
}
/*
 *	addi		Add Immediate
 *	.425
 */
void ppc_opc_addi(PPC_CPU_State &aCPU)
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
	aCPU.gpr[rD] = (rA ? aCPU.gpr[rA] : 0) + imm;
}
JITCFlow ppc_opc_gen_addi_addis(JITC &jitc, int rD, int rA, uint32 imm)
{
	if (rA == 0) {
		NativeReg d = jitc.mapClientRegisterDirty(PPC_GPR(rD));
		if (!jitc.flagsMapped() && !jitc.carryMapped()) {
			if (imm == 0) {
				jitc.clobberCarryAndFlags();
				jitc.asmALU32(X86_XOR, d, d);
			} else if (imm == 0xffffffff) {
				jitc.asmALU32(X86_OR, d, -1);
			} else {
				jitc.asmMOV32_NoFlags(d, imm);
			}
		} else {
			jitc.asmMOV32_NoFlags(d, imm);
		}
	} else {
		if (rD == rA) {
			NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
			if (!imm) {
				// empty
			} else if (imm == 1) {
				// inc / dec doesn't clobber carry
				jitc.clobberFlags();
				jitc.asmINC32(a);
			} else if (imm == 0xffffffff) {
				jitc.clobberFlags();
				jitc.asmDEC32(a);
			} else {
				if (jitc.flagsMapped() || jitc.carryMapped()) {
					// lea rA, [rB+imm]
					jitc.asmALU32(X86_LEA, a, a, imm);
				} else {
					jitc.clobberCarryAndFlags();
					jitc.asmALU32(X86_ADD, a, imm);
				}
			}
		} else {
			if (imm) {
				NativeReg a = jitc.getClientRegister(PPC_GPR(rA));
				// lea d, [a+imm]
				NativeReg d = jitc.mapClientRegisterDirty(PPC_GPR(rD));
				jitc.asmALU32(X86_LEA, d, a, imm);
			} else {
				NativeReg a = jitc.getClientRegister(PPC_GPR(rA));
				// mov d, a
				NativeReg d = jitc.mapClientRegisterDirty(PPC_GPR(rD));
				jitc.asmALU32(X86_MOV, d, a);
			}
		}
	}
	return flowContinue;
}
JITCFlow ppc_opc_gen_addi(JITC &jitc)
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
	return ppc_opc_gen_addi_addis(jitc, rD, rA, imm);
}	
/*
 *	addic		Add Immediate Carrying
 *	.426
 */
void ppc_opc_addic(PPC_CPU_State &aCPU)
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
	uint32 a = aCPU.gpr[rA];
	aCPU.gpr[rD] = a + imm;	
	aCPU.xer_ca = (aCPU.gpr[rD] < a);
}
JITCFlow ppc_opc_gen_addic(JITC &jitc)
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
	jitc.clobberFlags();
	if (rD == rA) {
		NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
		jitc.asmALU32(X86_ADD, a, imm);
	} else {
		NativeReg a = jitc.getClientRegister(PPC_GPR(rA));
		NativeReg d = jitc.mapClientRegisterDirty(PPC_GPR(rD));
		jitc.asmALU32(X86_MOV, d, a);
		jitc.asmALU32(X86_ADD, d, imm);
	}
	jitc.mapCarryDirty();
	return flowContinue;
}
/*
 *	addic.		Add Immediate Carrying and Record
 *	.427
 */
void ppc_opc_addic_(PPC_CPU_State &aCPU)
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
	uint32 a = aCPU.gpr[rA];
	aCPU.gpr[rD] = a + imm;
	aCPU.xer_ca = (aCPU.gpr[rD] < a);
	ppc_update_cr0(aCPU, aCPU.gpr[rD]);
}
JITCFlow ppc_opc_gen_addic_(JITC &jitc)
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
	if (rD == rA) {
		NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
		jitc.asmALU32(X86_ADD, a, imm);
	} else {
		NativeReg a = jitc.getClientRegister(PPC_GPR(rA));
		NativeReg d = jitc.mapClientRegisterDirty(PPC_GPR(rD));
		jitc.asmALU32(X86_MOV, d, a);
		jitc.asmALU32(X86_ADD, d, imm);
	}
	jitc.mapCarryDirty();
	jitc.mapFlagsDirty();
	return flowContinue;
}
/*
 *	addis		Add Immediate Shifted
 *	.428
 */
void ppc_opc_addis(PPC_CPU_State &aCPU)
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_Shift16(aCPU.current_opc, rD, rA, imm);
	aCPU.gpr[rD] = (rA ? aCPU.gpr[rA] : 0) + imm;
}
JITCFlow ppc_opc_gen_addis(JITC &jitc)
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_Shift16(jitc.current_opc, rD, rA, imm);
	return ppc_opc_gen_addi_addis(jitc, rD, rA, imm);
}	
/*
 *	addmex		Add to Minus One Extended
 *	.429
 */
void ppc_opc_addmex(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
	PPC_OPC_ASSERT(rB == 0);
	uint32 a = aCPU.gpr[rA];
	uint32 ca = aCPU.xer_ca;
	aCPU.gpr[rD] = a + ca + 0xffffffff;
	aCPU.xer_ca = a || ca;
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rD]);
	}
}
JITCFlow ppc_opc_gen_addmex(JITC &jitc)
{
	ppc_opc_gen_interpret(jitc, ppc_opc_addmex);
	return flowEndBlock;
	
}
/*
 *	addmeox		Add to Minus One Extended with Overflow
 *	.429
 */
void ppc_opc_addmeox(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
	PPC_OPC_ASSERT(rB == 0);
	uint32 a = aCPU.gpr[rA];
	uint32 ca = (aCPU.xer_ca);
	aCPU.gpr[rD] = a + ca + 0xffffffff;
	aCPU.xer_ca = (a || ca);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rD]);
	}
	// update XER flags
	PPC_ALU_ERR("addmeox unimplemented\n");
}
/*
 *	addzex		Add to Zero Extended
 *	.430
 */
void ppc_opc_addzex(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
	PPC_OPC_ASSERT(rB == 0);
	uint32 a = aCPU.gpr[rA];
	uint32 ca = aCPU.xer_ca;
	aCPU.gpr[rD] = a + ca;
	aCPU.xer_ca = ((a == 0xffffffff) && ca);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rD]);
	}
}
JITCFlow ppc_opc_gen_addzex(JITC &jitc)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(jitc.current_opc, rD, rA, rB);
	if (!(jitc.current_opc & PPC_OPC_Rc)) jitc.clobberFlags();
	jitc.getClientCarry();
	NativeReg d;
	if (rA == rD) {
		d = jitc.getClientRegisterDirty(PPC_GPR(rD));
	} else {
		NativeReg a = jitc.getClientRegister(PPC_GPR(rA));
		d = jitc.mapClientRegisterDirty(PPC_GPR(rD));
		jitc.asmALU32(X86_MOV, d, a);
	}
	jitc.asmALU32(X86_ADC, d, 0);
	jitc.mapCarryDirty();
	if (jitc.current_opc & PPC_OPC_Rc) jitc.mapFlagsDirty();
	return flowContinue;
}
/*
 *	addzeox		Add to Zero Extended with Overflow
 *	.430
 */
void ppc_opc_addzeox(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
	PPC_OPC_ASSERT(rB == 0);
	uint32 a = aCPU.gpr[rA];
	uint32 ca = aCPU.xer_ca;
	aCPU.gpr[rD] = a + ca;
	aCPU.xer_ca = ((a == 0xffffffff) && ca);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rD]);
	}
	// update XER flags
	PPC_ALU_ERR("addzeox unimplemented\n");
}

/*
 *	andx		AND
 *	.431
 */
void ppc_opc_andx(PPC_CPU_State &aCPU)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	aCPU.gpr[rA] = aCPU.gpr[rS] & aCPU.gpr[rB];
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_andx(JITC &jitc)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.clobberCarry();
	} else {
		jitc.clobberCarryAndFlags();
	}
	if (rA == rS) {
		NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
		NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
		jitc.asmALU32(X86_AND, a, b);
	} else if (rA == rB) {
		NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
		NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
		jitc.asmALU32(X86_AND, a, s);
	} else {
		NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
		NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
		NativeReg a = jitc.mapClientRegisterDirty(PPC_GPR(rA));
		jitc.asmALU32(X86_MOV, a, s);
		jitc.asmALU32(X86_AND, a, b);
	}
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.mapFlagsDirty();
	}
	return flowContinue;
}
/*
 *	andcx		AND with Complement
 *	.432
 */
void ppc_opc_andcx(PPC_CPU_State &aCPU)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	aCPU.gpr[rA] = aCPU.gpr[rS] & ~aCPU.gpr[rB];
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_andcx(JITC &jitc)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.clobberCarry();
	} else {
		jitc.clobberCarryAndFlags();
	}
	if (rA == rS) {
		NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
		NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
		NativeReg tmp = jitc.allocRegister();
		jitc.asmALU32(X86_MOV, tmp, b);
		jitc.asmALU32(X86_NOT, tmp);
		jitc.asmALU32(X86_AND, a, tmp);
	} else if (rA == rB) {
		NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
		NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
		jitc.asmALU32(X86_NOT, a);
		jitc.asmALU32(X86_AND, a, s);
	} else {
		NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
		NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
		NativeReg a = jitc.mapClientRegisterDirty(PPC_GPR(rA));
		jitc.asmALU32(X86_MOV, a, b);
		jitc.asmALU32(X86_NOT, a);
		jitc.asmALU32(X86_AND, a, s);
	}
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.mapFlagsDirty();
	}
	return flowContinue;
}
/*
 *	andi.		AND Immediate
 *	.433
 */
void ppc_opc_andi_(PPC_CPU_State &aCPU)
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_UImm(aCPU.current_opc, rS, rA, imm);
	aCPU.gpr[rA] = aCPU.gpr[rS] & imm;
	ppc_update_cr0(aCPU, aCPU.gpr[rA]);
}
JITCFlow ppc_opc_gen_andi_(JITC &jitc)
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_UImm(jitc.current_opc, rS, rA, imm);
	jitc.clobberCarry();
	if (rS == rA) {
		NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
		jitc.asmALU32(X86_AND, a, imm);
	} else {
		NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
		NativeReg a = jitc.mapClientRegisterDirty(PPC_GPR(rA));
		jitc.asmALU32(X86_MOV, a, s);
		jitc.asmALU32(X86_AND, a, imm);
	}
	jitc.mapFlagsDirty();
	return flowContinue;
}
/*
 *	andis.		AND Immediate Shifted
 *	.434
 */
void ppc_opc_andis_(PPC_CPU_State &aCPU)
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_Shift16(aCPU.current_opc, rS, rA, imm);
	aCPU.gpr[rA] = aCPU.gpr[rS] & imm;
	ppc_update_cr0(aCPU, aCPU.gpr[rA]);
}
JITCFlow ppc_opc_gen_andis_(JITC &jitc)
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_Shift16(jitc.current_opc, rS, rA, imm);
	jitc.clobberCarry();
	if (rS == rA) {
		NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
		jitc.asmALU32(X86_AND, a, imm);
	} else {
		NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
		NativeReg a = jitc.mapClientRegisterDirty(PPC_GPR(rA));
		jitc.asmALU32(X86_MOV, a, s);
		jitc.asmALU32(X86_AND, a, imm);
	}
	jitc.mapFlagsDirty();
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

void ppc_opc_cmp(PPC_CPU_State &aCPU)
{
	uint32 cr;
	int rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, cr, rA, rB);
	cr >>= 2;
	sint32 a = aCPU.gpr[rA];
	sint32 b = aCPU.gpr[rB];
	uint32 c;
	if (a < b) {
		c = 8;
	} else if (a > b) {
		c = 4;
	} else {
		c = 2;
	}
	if (aCPU.xer & XER_SO) c |= 1;
	cr = 7-cr;
	aCPU.cr &= ppc_cmp_and_mask[cr];
	aCPU.cr |= c<<(cr*4);
}
JITCFlow ppc_opc_gen_cmp(JITC &jitc)
{
	uint32 cr;
	int rA, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, cr, rA, rB);
	cr >>= 2;
	jitc.clobberCarryAndFlags();
	NativeReg a = jitc.getClientRegister(PPC_GPR(rA));
	NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
	jitc.asmALU32(X86_CMP, a, b);
#if 1
	if (cr == 0) {
		jitc.asmCALL((NativeAddress)ppc_flush_flags_signed_0_asm);
	} else {
		jitc.clobberRegister(RAX | NATIVE_REG);
		jitc.asmMOV32_NoFlags(RAX, (7-cr)/2);
		jitc.asmCALL((cr & 1) ? (NativeAddress)ppc_flush_flags_signed_odd_asm : (NativeAddress)ppc_flush_flags_signed_even_asm);
	}
#else
	if (cr & 1) {
		jitc.flushFlagsAfterCMP_L((7-cr)/2);
	} else {
		jitc.flushFlagsAfterCMP_U((7-cr)/2);
	}
#endif
	return flowContinue;
}
/*
 *	cmpi		Compare Immediate
 *	.443
 */
void ppc_opc_cmpi(PPC_CPU_State &aCPU)
{
	uint32 cr;
	int rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, cr, rA, imm);
	cr >>= 2;
	sint32 a = aCPU.gpr[rA];
	sint32 b = imm;
	uint32 c;
	if (a < b) {
		c = 8;
	} else if (a > b) {
		c = 4;
	} else {
		c = 2;
	}
	if (aCPU.xer & XER_SO) c |= 1;
	cr = 7-cr;
	aCPU.cr &= ppc_cmp_and_mask[cr];
	aCPU.cr |= c<<(cr*4);
}
JITCFlow ppc_opc_gen_cmpi(JITC &jitc)
{
	uint32 cr;
	int rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, cr, rA, imm);
	cr >>= 2;
	jitc.clobberCarryAndFlags();
	NativeReg a = jitc.getClientRegister(PPC_GPR(rA));
	jitc.asmALU32(X86_CMP, a, imm);
#if 1
	if (cr == 0) {
		jitc.asmCALL((NativeAddress)ppc_flush_flags_signed_0_asm);
	} else {
		jitc.clobberRegister(RAX | NATIVE_REG);
		jitc.asmMOV32_NoFlags(RAX, (7-cr)/2);
		jitc.asmCALL((cr & 1) ? (NativeAddress)ppc_flush_flags_signed_odd_asm : (NativeAddress)ppc_flush_flags_signed_even_asm);
	}
#else
	if (cr & 1) {
		jitc.flushFlagsAfterCMP_L((7-cr)/2);
	} else {
		jitc.flushFlagsAfterCMP_U((7-cr)/2);
	}
#endif
	return flowContinue;
}
/*
 *	cmpl		Compare Logical
 *	.444
 */
void ppc_opc_cmpl(PPC_CPU_State &aCPU)
{
	uint32 cr;
	int rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, cr, rA, rB);
	cr >>= 2;
	uint32 a = aCPU.gpr[rA];
	uint32 b = aCPU.gpr[rB];
	uint32 c;
	if (a < b) {
		c = 8;
	} else if (a > b) {
		c = 4;
	} else {
		c = 2;
	}
	if (aCPU.xer & XER_SO) c |= 1;
	cr = 7-cr;
	aCPU.cr &= ppc_cmp_and_mask[cr];
	aCPU.cr |= c<<(cr*4);
}
JITCFlow ppc_opc_gen_cmpl(JITC &jitc)
{
	uint32 cr;
	int rA, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, cr, rA, rB);
	cr >>= 2;
	jitc.clobberCarryAndFlags();
	NativeReg a = jitc.getClientRegister(PPC_GPR(rA));
	NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
	jitc.asmALU32(X86_CMP, a, b);
#if 1
	if (cr == 0) {
		jitc.asmCALL((NativeAddress)ppc_flush_flags_unsigned_0_asm);
	} else {
		jitc.clobberRegister(RAX | NATIVE_REG);
	        jitc.asmMOV32_NoFlags(RAX, (7-cr)/2);
		jitc.asmCALL((cr & 1) ? (NativeAddress)ppc_flush_flags_unsigned_odd_asm : (NativeAddress)ppc_flush_flags_unsigned_even_asm);
	}
#else
	if (cr & 1) {
		jitc.flushFlagsAfterCMPL_L((7-cr)/2);
	} else {
		jitc.flushFlagsAfterCMPL_U((7-cr)/2);
	}
#endif
	return flowContinue;
}
/*
 *	cmpli		Compare Logical Immediate
 *	.445
 */
void ppc_opc_cmpli(PPC_CPU_State &aCPU)
{
	uint32 cr;
	int rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_UImm(aCPU.current_opc, cr, rA, imm);
	cr >>= 2;
	uint32 a = aCPU.gpr[rA];
	uint32 b = imm;
	uint32 c;
	if (a < b) {
		c = 8;
	} else if (a > b) {
		c = 4;
	} else {
		c = 2;
	}
	if (aCPU.xer & XER_SO) c |= 1;
	cr = 7-cr;
	aCPU.cr &= ppc_cmp_and_mask[cr];
	aCPU.cr |= c<<(cr*4);
}
JITCFlow ppc_opc_gen_cmpli(JITC &jitc)
{
	uint32 cr;
	int rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_UImm(jitc.current_opc, cr, rA, imm);
	cr >>= 2;
	jitc.clobberCarryAndFlags();
	NativeReg a = jitc.getClientRegister(PPC_GPR(rA));
	jitc.asmALU32(X86_CMP, a, imm);
#if 1
	if (cr == 0) {
		jitc.asmCALL((NativeAddress)ppc_flush_flags_unsigned_0_asm);
	} else {
		jitc.clobberRegister(RAX | NATIVE_REG);
		jitc.asmMOV32_NoFlags(RAX, (7-cr)/2);
		jitc.asmCALL((cr & 1) ? (NativeAddress)ppc_flush_flags_unsigned_odd_asm : (NativeAddress)ppc_flush_flags_unsigned_even_asm);
	}
#else
	if (cr & 1) {
		jitc.flushFlagsAfterCMPL_L((7-cr)/2);
	} else {
		jitc.flushFlagsAfterCMPL_U((7-cr)/2);
	}
#endif
	return flowContinue;
}

/*
 *	cntlzwx		Count Leading Zeros Word
 *	.447
 */
void ppc_opc_cntlzwx(PPC_CPU_State &aCPU)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	PPC_OPC_ASSERT(rB==0);
	uint32 n=0;
	uint32 x=0x80000000;
	uint32 v=aCPU.gpr[rS];
	while (!(v & x)) {
		n++;
		if (n==32) break;
		x>>=1;
	}
	aCPU.gpr[rA] = n;
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_cntlzwx(JITC &jitc)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
	jitc.clobberCarryAndFlags();
	NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
	NativeReg a = jitc.mapClientRegisterDirty(PPC_GPR(rA));
	NativeReg z = jitc.allocRegister();
	jitc.asmALU32(X86_MOV, z, 0xffffffff);
	jitc.asmBSx32(X86_BSR, a, s);
	jitc.asmCMOV32(X86_Z, a, z);
	jitc.asmALU32(X86_NEG, a);
	jitc.asmALU32(X86_ADD, a, 31);
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.mapFlagsDirty();
	}
	return flowContinue;
}

/*
 *	crand		Condition Register AND
 *	.448
 */
void ppc_opc_crand(PPC_CPU_State &aCPU)
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, crD, crA, crB);
	if ((aCPU.cr & (1<<(31-crA))) && (aCPU.cr & (1<<(31-crB)))) {
		aCPU.cr |= (1<<(31-crD));
	} else {
		aCPU.cr &= ~(1<<(31-crD));
	}
}
JITCFlow ppc_opc_gen_crand(JITC &jitc)
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(jitc.current_opc, crD, crA, crB);
	jitc.clobberCarryAndFlags();
	jitc.asmTEST32(curCPU(cr), 1<<(31-crA));	
	NativeAddress nocrA = jitc.asmJxxFixup(X86_Z);
		jitc.asmTEST32(curCPU(cr), 1<<(31-crB));
		NativeAddress nocrB = jitc.asmJxxFixup(X86_Z);
			jitc.asmOR32(curCPU(cr), 1<<(31-crD));
			NativeAddress end1 = jitc.asmJMPFixup();
	jitc.asmResolveFixup(nocrB, jitc.asmHERE());
	jitc.asmResolveFixup(nocrA, jitc.asmHERE());
		jitc.asmAND32(curCPU(cr), ~(1<<(31-crD)));
	jitc.asmResolveFixup(end1, jitc.asmHERE());
	return flowContinue;
}
/*
 *	crandc		Condition Register AND with Complement
 *	.449
 */
void ppc_opc_crandc(PPC_CPU_State &aCPU)
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, crD, crA, crB);
	if ((aCPU.cr & (1<<(31-crA))) && !(aCPU.cr & (1<<(31-crB)))) {
		aCPU.cr |= (1<<(31-crD));
	} else {
		aCPU.cr &= ~(1<<(31-crD));
	}
}
JITCFlow ppc_opc_gen_crandc(JITC &jitc)
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(jitc.current_opc, crD, crA, crB);
	jitc.clobberCarryAndFlags();
	jitc.asmTEST32(curCPU(cr), 1<<(31-crA));	
	NativeAddress nocrA = jitc.asmJxxFixup(X86_Z);
		jitc.asmTEST32(curCPU(cr), 1<<(31-crB));
		NativeAddress nocrB = jitc.asmJxxFixup(X86_NZ);
			jitc.asmOR32(curCPU(cr), 1<<(31-crD));
			NativeAddress end1 = jitc.asmJMPFixup();
	jitc.asmResolveFixup(nocrB, jitc.asmHERE());
	jitc.asmResolveFixup(nocrA, jitc.asmHERE());
		jitc.asmAND32(curCPU(cr), ~(1<<(31-crD)));
	jitc.asmResolveFixup(end1, jitc.asmHERE());
	return flowContinue;
}
/*
 *	creqv		Condition Register Equivalent
 *	.450
 */
void ppc_opc_creqv(PPC_CPU_State &aCPU)
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, crD, crA, crB);
	if (((aCPU.cr & (1<<(31-crA))) && (aCPU.cr & (1<<(31-crB))))
	  || (!(aCPU.cr & (1<<(31-crA))) && !(aCPU.cr & (1<<(31-crB))))) {
		aCPU.cr |= (1<<(31-crD));
	} else {
		aCPU.cr &= ~(1<<(31-crD));
	}
}
JITCFlow ppc_opc_gen_creqv(JITC &jitc)
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(jitc.current_opc, crD, crA, crB);
	jitc.clobberCarryAndFlags();
	if (crA == crB) {
		jitc.asmOR32(curCPU(cr), 1<<(31-crD));
	} else {
		// crD = crA ? (crB ? 1 : 0) : (crB ? 0 : 1) 
		jitc.asmTEST32(curCPU(cr), 1<<(31-crA));
		NativeAddress nocrA = jitc.asmJxxFixup(X86_Z);
			jitc.asmTEST32(curCPU(cr), 1<<(31-crB));
			NativeAddress nocrB1 = jitc.asmJxxFixup(X86_Z);
				jitc.asmOR32(curCPU(cr), 1<<(31-crD));
				NativeAddress end1 = jitc.asmJMPFixup();
			jitc.asmResolveFixup(nocrB1, jitc.asmHERE());
				jitc.asmAND32(curCPU(cr), ~(1<<(31-crD)));
				NativeAddress end2 = jitc.asmJMPFixup();
		jitc.asmResolveFixup(nocrA, jitc.asmHERE());
			jitc.asmTEST32(curCPU(cr), 1<<(31-crB));
			NativeAddress nocrB2 = jitc.asmJxxFixup(X86_Z);
				jitc.asmAND32(curCPU(cr), ~(1<<(31-crD)));
				NativeAddress end3 = jitc.asmJMPFixup();
			jitc.asmResolveFixup(nocrB2, jitc.asmHERE());
				jitc.asmOR32(curCPU(cr), 1<<(31-crD));
		jitc.asmResolveFixup(end1, jitc.asmHERE());
		jitc.asmResolveFixup(end2, jitc.asmHERE());
		jitc.asmResolveFixup(end3, jitc.asmHERE());
	}
	return flowContinue;
}
/*
 *	crnand		Condition Register NAND
 *	.451
 */
void ppc_opc_crnand(PPC_CPU_State &aCPU)
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, crD, crA, crB);
	if (!((aCPU.cr & (1<<(31-crA))) && (aCPU.cr & (1<<(31-crB))))) {
		aCPU.cr |= (1<<(31-crD));
	} else {
		aCPU.cr &= ~(1<<(31-crD));
	}
}
JITCFlow ppc_opc_gen_crnand(JITC &jitc)
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(jitc.current_opc, crD, crA, crB);
	jitc.clobberCarryAndFlags();
	jitc.asmTEST32(curCPU(cr), 1<<(31-crA));	
	NativeAddress nocrA = jitc.asmJxxFixup(X86_Z);
		jitc.asmTEST32(curCPU(cr), 1<<(31-crB));
		NativeAddress nocrB = jitc.asmJxxFixup(X86_Z);
			jitc.asmAND32(curCPU(cr), ~(1<<(31-crD)));
			NativeAddress end1 = jitc.asmJMPFixup();
	jitc.asmResolveFixup(nocrB, jitc.asmHERE());
	jitc.asmResolveFixup(nocrA, jitc.asmHERE());
		jitc.asmOR32(curCPU(cr), 1<<(31-crD));
	jitc.asmResolveFixup(end1, jitc.asmHERE());
	return flowContinue;
}
/*
 *	crnor		Condition Register NOR
 *	.452
 */
void ppc_opc_crnor(PPC_CPU_State &aCPU)
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, crD, crA, crB);
	uint32 t = (1<<(31-crA)) | (1<<(31-crB));
	if (!(aCPU.cr & t)) {
		aCPU.cr |= (1<<(31-crD));
	} else {
		aCPU.cr &= ~(1<<(31-crD));
	}
}
JITCFlow ppc_opc_gen_crnor(JITC &jitc)
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(jitc.current_opc, crD, crA, crB);
	jitc.clobberCarryAndFlags();
	jitc.asmTEST32(curCPU(cr), (1<<(31-crA)) | (1<<(31-crB)));
	NativeAddress notset = jitc.asmJxxFixup(X86_Z);
		jitc.asmAND32(curCPU(cr), ~(1<<(31-crD)));
		NativeAddress end1 = jitc.asmJMPFixup();
	jitc.asmResolveFixup(notset, jitc.asmHERE());
		jitc.asmOR32(curCPU(cr), 1<<(31-crD));
	jitc.asmResolveFixup(end1, jitc.asmHERE());
	return flowContinue;
}
/*
 *	cror		Condition Register OR
 *	.453
 */
void ppc_opc_cror(PPC_CPU_State &aCPU)
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, crD, crA, crB);
	uint32 t = (1<<(31-crA)) | (1<<(31-crB));
	if (aCPU.cr & t) {
		aCPU.cr |= (1<<(31-crD));
	} else {
		aCPU.cr &= ~(1<<(31-crD));
	}
}
JITCFlow ppc_opc_gen_cror(JITC &jitc)
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(jitc.current_opc, crD, crA, crB);
	jitc.clobberCarryAndFlags();
	jitc.asmTEST32(curCPU(cr), (1<<(31-crA)) | (1<<(31-crB)));
	NativeAddress notset = jitc.asmJxxFixup(X86_Z);
		jitc.asmOR32(curCPU(cr), 1<<(31-crD));
		NativeAddress end1 = jitc.asmJMPFixup();
	jitc.asmResolveFixup(notset, jitc.asmHERE());
		jitc.asmAND32(curCPU(cr), ~(1<<(31-crD)));
	jitc.asmResolveFixup(end1, jitc.asmHERE());
	return flowContinue;
}
/*
 *	crorc		Condition Register OR with Complement
 *	.454
 */
void ppc_opc_crorc(PPC_CPU_State &aCPU)
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, crD, crA, crB);
	if ((aCPU.cr & (1<<(31-crA))) || !(aCPU.cr & (1<<(31-crB)))) {
		aCPU.cr |= (1<<(31-crD));
	} else {
		aCPU.cr &= ~(1<<(31-crD));
	}
}

JITCFlow ppc_opc_gen_crorc(JITC &jitc)
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(jitc.current_opc, crD, crA, crB);
	jitc.clobberCarryAndFlags();
	jitc.asmTEST32(curCPU(cr), 1<<(31-crA));	
	NativeAddress crAset = jitc.asmJxxFixup(X86_NZ);
		jitc.asmTEST32(curCPU(cr), 1<<(31-crB));
		NativeAddress nocrB = jitc.asmJxxFixup(X86_Z);
			jitc.asmAND32(curCPU(cr), ~(1<<(31-crD)));
			NativeAddress end1 = jitc.asmJMPFixup();
	jitc.asmResolveFixup(nocrB, jitc.asmHERE());
	jitc.asmResolveFixup(crAset, jitc.asmHERE());
		jitc.asmOR32(curCPU(cr), 1<<(31-crD));
	jitc.asmResolveFixup(end1, jitc.asmHERE());
	return flowContinue;
}
/*
 *	crxor		Condition Register XOR
 *	.448
 */
void ppc_opc_crxor(PPC_CPU_State &aCPU)
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, crD, crA, crB);
	if ((!(aCPU.cr & (1<<(31-crA))) && (aCPU.cr & (1<<(31-crB))))
	  || ((aCPU.cr & (1<<(31-crA))) && !(aCPU.cr & (1<<(31-crB))))) {
		aCPU.cr |= (1<<(31-crD));
	} else {
		aCPU.cr &= ~(1<<(31-crD));
	}
}
JITCFlow ppc_opc_gen_crxor(JITC &jitc)
{
	int crD, crA, crB;
	PPC_OPC_TEMPL_X(jitc.current_opc, crD, crA, crB);
	jitc.clobberCarryAndFlags();
	if (crA == crB) {
		jitc.asmAND32(curCPU(cr), ~(1<<(31-crD)));
	} else {
		// crD = crA ? (crB ? 0 : 1) : (crB ? 1 : 0) 
		jitc.asmTEST32(curCPU(cr), 1<<(31-crA));
		NativeAddress nocrA = jitc.asmJxxFixup(X86_Z);
			jitc.asmTEST32(curCPU(cr), 1<<(31-crB));
			NativeAddress nocrB1 = jitc.asmJxxFixup(X86_Z);
				jitc.asmAND32(curCPU(cr), ~(1<<(31-crD)));
				NativeAddress end1 = jitc.asmJMPFixup();
			jitc.asmResolveFixup(nocrB1, jitc.asmHERE());
				jitc.asmOR32(curCPU(cr), 1<<(31-crD));
				NativeAddress end2 = jitc.asmJMPFixup();
		jitc.asmResolveFixup(nocrA, jitc.asmHERE());
			jitc.asmTEST32(curCPU(cr), 1<<(31-crB));
			NativeAddress nocrB2 = jitc.asmJxxFixup(X86_Z);
				jitc.asmOR32(curCPU(cr), 1<<(31-crD));
				NativeAddress end3 = jitc.asmJMPFixup();
			jitc.asmResolveFixup(nocrB2, jitc.asmHERE());
				jitc.asmAND32(curCPU(cr), ~(1<<(31-crD)));
		jitc.asmResolveFixup(end1, jitc.asmHERE());
		jitc.asmResolveFixup(end2, jitc.asmHERE());
		jitc.asmResolveFixup(end3, jitc.asmHERE());
	}
	return flowContinue;
}

/*
 *	divwx		Divide Word
 *	.470
 */
void ppc_opc_divwx(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
	if (!aCPU.gpr[rB]) {
		PPC_ALU_WARN("division by zero @%08x\n", aCPU.pc);
		SINGLESTEP("");
	} else {
		sint32 a = aCPU.gpr[rA];
		sint32 b = aCPU.gpr[rB];
		aCPU.gpr[rD] = a / b;
	}
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rD]);
	}
}
JITCFlow ppc_opc_gen_divwx(JITC &jitc)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(jitc.current_opc, rD, rA, rB);
	jitc.clobberCarryAndFlags();
	jitc.allocRegister(NATIVE_REG | RDX);
	jitc.getClientRegister(PPC_GPR(rA), NATIVE_REG | RAX);
	NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
	jitc.clobberRegister(NATIVE_REG | RAX);
	jitc.asmSimple(X86_CDQ);
	jitc.asmALU32(X86_TEST, b, b);
	NativeAddress na = jitc.asmJxxFixup(X86_Z);
	jitc.asmALU32(X86_IDIV, b);
	jitc.asmResolveFixup(na, jitc.asmHERE());
	jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RAX);
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.asmALU32(X86_TEST, RAX, RAX);
		jitc.mapFlagsDirty();
	}
	return flowContinue;
}
/*
 *	divwox		Divide Word with Overflow
 *	.470
 */
void ppc_opc_divwox(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
	if (!aCPU.gpr[rB]) {
		PPC_ALU_WARN("division by zero\n");
	} else {
		sint32 a = aCPU.gpr[rA];
		sint32 b = aCPU.gpr[rB];
		aCPU.gpr[rD] = a / b;
	}
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rD]);
	}
	// update XER flags
	PPC_ALU_ERR("divwox unimplemented\n");
}
/*
 *	divwux		Divide Word Unsigned
 *	.472
 */
void ppc_opc_divwux(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
	if (!aCPU.gpr[rB]) {
		PPC_ALU_WARN("division by zero @%08x\n", aCPU.pc);
		SINGLESTEP("");
	} else {
		aCPU.gpr[rD] = aCPU.gpr[rA] / aCPU.gpr[rB];
	}
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rD]);
	}
}
JITCFlow ppc_opc_gen_divwux(JITC &jitc)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(jitc.current_opc, rD, rA, rB);
	jitc.clobberCarryAndFlags();
	jitc.allocRegister(NATIVE_REG | RDX);
	jitc.getClientRegister(PPC_GPR(rA), NATIVE_REG | RAX);
	NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
	jitc.clobberRegister(NATIVE_REG | RAX);
	jitc.asmALU32(X86_XOR, RDX, RDX);
	jitc.asmALU32(X86_TEST, b, b);
	NativeAddress na = jitc.asmJxxFixup(X86_Z);
	jitc.asmALU32(X86_DIV, b);
	jitc.asmResolveFixup(na, jitc.asmHERE());
	jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RAX);
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.asmALU32(X86_TEST, RAX, RAX);
		jitc.mapFlagsDirty();
	}
	return flowContinue;
}
/*
 *	divwuox		Divide Word Unsigned with Overflow
 *	.472
 */
void ppc_opc_divwuox(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
	if (!aCPU.gpr[rB]) {
		PPC_ALU_WARN("division by zero @%08x\n", aCPU.pc);
	} else {
		aCPU.gpr[rD] = aCPU.gpr[rA] / aCPU.gpr[rB];
	}
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rD]);
	}
	// update XER flags
	PPC_ALU_ERR("divwuox unimplemented\n");
}

/*
 *	eqvx		Equivalent
 *	.480
 */
void ppc_opc_eqvx(PPC_CPU_State &aCPU)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	aCPU.gpr[rA] = ~(aCPU.gpr[rS] ^ aCPU.gpr[rB]);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_eqvx(JITC &jitc)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.clobberCarry();
	} else {
		jitc.clobberCarryAndFlags();
	}
	NativeReg a;
	if (rA == rS) {
		a = jitc.getClientRegisterDirty(PPC_GPR(rA));
		NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
		jitc.asmALU32(X86_XOR, a, b);
	} else if (rA == rB) {
		a = jitc.getClientRegisterDirty(PPC_GPR(rA));
		NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
		jitc.asmALU32(X86_XOR, a, s);
	} else {
		NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
		NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
		a = jitc.mapClientRegisterDirty(PPC_GPR(rA));
		jitc.asmALU32(X86_MOV, a, s);
		jitc.asmALU32(X86_XOR, a, b);
	}
	jitc.asmALU32(X86_NOT, a);
	if (jitc.current_opc & PPC_OPC_Rc) {
		// "NOT" doesn't update the flags
		jitc.asmALU32(X86_TEST, a, a);
		jitc.mapFlagsDirty();
	}
	return flowContinue;
}

/*
 *	extsbx		Extend Sign Byte
 *	.481
 */
void ppc_opc_extsbx(PPC_CPU_State &aCPU)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	PPC_OPC_ASSERT(rB==0);
	aCPU.gpr[rA] = aCPU.gpr[rS];
	if (aCPU.gpr[rA] & 0x80) {
		aCPU.gpr[rA] |= 0xffffff00;
	} else {
		aCPU.gpr[rA] &= ~0xffffff00;
	}
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_extsbx(JITC &jitc)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
	NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
	NativeReg a = jitc.mapClientRegisterDirty(PPC_GPR(rA));
	jitc.asmMOVxx32_8(X86_MOVSX, a, s);
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.clobberCarry();
		jitc.asmALU32(X86_TEST, a, a);
		jitc.mapFlagsDirty();
	}
	return flowContinue;
}
/*
 *	extshx		Extend Sign Half Word
 *	.482
 */
void ppc_opc_extshx(PPC_CPU_State &aCPU)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	PPC_OPC_ASSERT(rB==0);
	aCPU.gpr[rA] = aCPU.gpr[rS];
	if (aCPU.gpr[rA] & 0x8000) {
		aCPU.gpr[rA] |= 0xffff0000;
	} else {
		aCPU.gpr[rA] &= ~0xffff0000;
	}
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_extshx(JITC &jitc)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
	NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
	NativeReg a = jitc.mapClientRegisterDirty(PPC_GPR(rA));
	jitc.asmMOVxx32_16(X86_MOVSX, a, s);
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.clobberCarry();
		jitc.asmALU32(X86_TEST, a, a);
		jitc.mapFlagsDirty();
	}
	return flowContinue;
}

/*
 *	mulhwx		Multiply High Word
 *	.595
 */
void ppc_opc_mulhwx(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
	sint64 a = sint32(aCPU.gpr[rA]);
	sint64 b = sint32(aCPU.gpr[rB]);
	sint64 c = a*b;
	aCPU.gpr[rD] = uint64(c)>>32;
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rD]);
	}
}
JITCFlow ppc_opc_gen_mulhwx(JITC &jitc)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(jitc.current_opc, rD, rA, rB);
	jitc.clobberCarryAndFlags();
	NativeReg a, b;
	if (jitc.getClientRegisterMapping(PPC_GPR(rB)) == RAX) {
		// swapped by incident
		a = RAX;
		b = jitc.getClientRegister(PPC_GPR(rA));
	} else {
		a = jitc.getClientRegister(PPC_GPR(rA), NATIVE_REG | RAX);
		b = jitc.getClientRegister(PPC_GPR(rB));
	}
	jitc.clobberRegister(NATIVE_REG | RAX);
	jitc.clobberRegister(NATIVE_REG | RDX);
	jitc.asmALU32(X86_IMUL, b);
	jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RDX);
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.asmALU32(X86_TEST, RDX, RDX);
		jitc.mapFlagsDirty();
	}
	return flowContinue;
}
/*
 *	mulhwux		Multiply High Word Unsigned
 *	.596
 */
void ppc_opc_mulhwux(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
	uint64 a = aCPU.gpr[rA];
	uint64 b = aCPU.gpr[rB];
	uint64 c = a*b;
	aCPU.gpr[rD] = c>>32;
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rD]);
	}
}
JITCFlow ppc_opc_gen_mulhwux(JITC &jitc)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(jitc.current_opc, rD, rA, rB);
	jitc.clobberCarryAndFlags();
	NativeReg a, b;
	if (jitc.getClientRegisterMapping(PPC_GPR(rB)) == RAX) {
		// swapped by incident
		a = RAX;
		b = jitc.getClientRegister(PPC_GPR(rA));
	} else {
		a = jitc.getClientRegister(PPC_GPR(rA), NATIVE_REG | RAX);
		b = jitc.getClientRegister(PPC_GPR(rB));
	}
	jitc.clobberRegister(NATIVE_REG | RAX);
	jitc.clobberRegister(NATIVE_REG | RDX);
	jitc.asmALU32(X86_MUL, b);
	jitc.mapClientRegisterDirty(PPC_GPR(rD), NATIVE_REG | RDX);
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.asmALU32(X86_TEST, RDX, RDX);
		jitc.mapFlagsDirty();
	}
	return flowContinue;
}
/*
 *	mulli		Multiply Low Immediate
 *	.598
 */
void ppc_opc_mulli(PPC_CPU_State &aCPU)
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
	// FIXME: signed / unsigned correct?
	aCPU.gpr[rD] = aCPU.gpr[rA] * imm;
}
JITCFlow ppc_opc_gen_mulli(JITC &jitc)
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
	jitc.clobberCarryAndFlags();
	NativeReg a = jitc.getClientRegister(PPC_GPR(rA));
	NativeReg d = jitc.mapClientRegisterDirty(PPC_GPR(rD));
	jitc.asmIMUL32(d, a, imm);
	return flowContinue;
}
/*
 *	mullwx		Multiply Low Word
 *	.599
 */
void ppc_opc_mullwx(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
	aCPU.gpr[rD] = aCPU.gpr[rA] * aCPU.gpr[rB];
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rD]);
	}
	if (aCPU.current_opc & PPC_OPC_OE) {
		// update XER flags
		PPC_ALU_ERR("mullwox unimplemented\n");
	}
}
JITCFlow ppc_opc_gen_mullwx(JITC &jitc)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(jitc.current_opc, rD, rA, rB);
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.clobberCarry();
	} else {
		jitc.clobberCarryAndFlags();
	}
	NativeReg a = jitc.getClientRegister(PPC_GPR(rA));
	NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
	NativeReg d;
	if (rA == rD) {
		d = a;
		jitc.dirtyRegister(a);
	} else if (rB == rD) {
		d = b;
		jitc.dirtyRegister(b);
		b = a;
	} else {
		d = jitc.mapClientRegisterDirty(PPC_GPR(rD));
		jitc.asmALU32(X86_MOV, d, a);
	}
	// now: d *= b
	jitc.asmIMUL32(d, b);
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.asmALU32(X86_OR, d, d);
		jitc.mapFlagsDirty();
	}
	return flowContinue;
}

/*
 *	nandx		NAND
 *	.600
 */
void ppc_opc_nandx(PPC_CPU_State &aCPU)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	aCPU.gpr[rA] = ~(aCPU.gpr[rS] & aCPU.gpr[rB]);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_nandx(JITC &jitc)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.clobberCarry();
	} else {
		jitc.clobberCarryAndFlags();
	}
	NativeReg a;
	if (rS == rB) {
	        if (rA == rS) {
			a = jitc.getClientRegisterDirty(PPC_GPR(rA));
		} else {
			NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
			a = jitc.mapClientRegisterDirty(PPC_GPR(rA));
			jitc.asmALU32(X86_MOV, a, s);
		}
	} else if (rA == rS) {
		a = jitc.getClientRegisterDirty(PPC_GPR(rA));
		NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
		jitc.asmALU32(X86_AND, a, b);
	} else if (rA == rB) {
		a = jitc.getClientRegisterDirty(PPC_GPR(rA));
		NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
		jitc.asmALU32(X86_AND, a, s);
	} else {
		NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
		NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
		a = jitc.mapClientRegisterDirty(PPC_GPR(rA));
		jitc.asmALU32(X86_MOV, a, s);
		jitc.asmALU32(X86_AND, a, b);
	}
	jitc.asmALU32(X86_NOT, a);
	if (jitc.current_opc & PPC_OPC_Rc) {
		// "NOT" doesn't update the flags
		jitc.asmALU32(X86_TEST, a, a);
		jitc.mapFlagsDirty();
	}
	return flowContinue;
}

/*
 *	negx		Negate
 *	.601
 */
void ppc_opc_negx(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
	PPC_OPC_ASSERT(rB == 0);
	aCPU.gpr[rD] = -aCPU.gpr[rA];
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rD]);
	}
}
JITCFlow ppc_opc_gen_negx(JITC &jitc)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(jitc.current_opc, rD, rA, rB);
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.clobberCarry();
	} else {
		jitc.clobberCarryAndFlags();
	}
	if (rA == rD) {
		NativeReg d = jitc.getClientRegisterDirty(PPC_GPR(rD));
		jitc.asmALU32(X86_NEG, d);
	} else {
		NativeReg a = jitc.getClientRegister(PPC_GPR(rA));
		NativeReg d = jitc.mapClientRegisterDirty(PPC_GPR(rD));
		jitc.asmALU32(X86_MOV, d, a);
		jitc.asmALU32(X86_NEG, d);
	}
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.mapFlagsDirty();
	}
	return flowContinue;
}
/*
 *	negox		Negate with Overflow
 *	.601
 */
void ppc_opc_negox(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
	PPC_OPC_ASSERT(rB == 0);
	aCPU.gpr[rD] = -aCPU.gpr[rA];
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rD]);
	}
	// update XER flags
	PPC_ALU_ERR("negox unimplemented\n");
}
/*
 *	norx		NOR
 *	.602
 */
void ppc_opc_norx(PPC_CPU_State &aCPU)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	aCPU.gpr[rA] = ~(aCPU.gpr[rS] | aCPU.gpr[rB]);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_norx(JITC &jitc)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
	NativeReg a;
	if (rS == rB) {
		// norx rA, rS, rS == not rA, rS
		// not doen't clobber the flags
		if (jitc.current_opc & PPC_OPC_Rc) {
			jitc.clobberCarry();
		}
	        if (rA == rS) {
			a = jitc.getClientRegisterDirty(PPC_GPR(rA));
		} else {
			NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
			a = jitc.mapClientRegisterDirty(PPC_GPR(rA));
			jitc.asmALU32(X86_MOV, a, s);
		}
	} else {
		if (jitc.current_opc & PPC_OPC_Rc) {
			jitc.clobberCarry();
		} else {
			jitc.clobberCarryAndFlags();
		}
		if (rA == rS) {
			a = jitc.getClientRegisterDirty(PPC_GPR(rA));
			NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
			jitc.asmALU32(X86_OR, a, b);
		} else if (rA == rB) {
			a = jitc.getClientRegisterDirty(PPC_GPR(rA));
			NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
			jitc.asmALU32(X86_OR, a, s);
		} else {
			NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
			NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
			a = jitc.mapClientRegisterDirty(PPC_GPR(rA));
			jitc.asmALU32(X86_MOV, a, s);
			jitc.asmALU32(X86_OR, a, b);
		}
	}
	jitc.asmALU32(X86_NOT, a);
	if (jitc.current_opc & PPC_OPC_Rc) {
		// "NOT" doesn't update the flags
		jitc.asmALU32(X86_TEST, a, a);
		jitc.mapFlagsDirty();
	}
	return flowContinue;
}

/*
 *	orx		OR
 *	.603
 */
void ppc_opc_orx(PPC_CPU_State &aCPU)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	aCPU.gpr[rA] = aCPU.gpr[rS] | aCPU.gpr[rB];
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_or(JITC &jitc)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
	if (rS == rB) {
		if (rS == rA) {
			/* nop */
		} else {
			/* mr rA, rS*/
			NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
			NativeReg a = jitc.mapClientRegisterDirty(PPC_GPR(rA));
			jitc.asmALU32(X86_MOV, a, s);
		}
	} else {
		if (rA == rS) {
			// or a, a, b
			NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
			jitc.clobberCarryAndFlags();
			NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
			jitc.asmALU32(X86_OR, a, b);
		} else if (rA == rB) {
			// or a, s, a
			NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
			jitc.clobberCarryAndFlags();
			NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
			jitc.asmALU32(X86_OR, a, s);
		} else {
			// or a, s, b
			NativeReg a = jitc.mapClientRegisterDirty(PPC_GPR(rA));
			jitc.clobberCarryAndFlags();
			NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
			NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
			jitc.asmALU32(X86_MOV, a, s);
			jitc.asmALU32(X86_OR, a, b);			
		}
	}
	return flowContinue;
}
JITCFlow ppc_opc_gen_orp(JITC &jitc)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
	jitc.clobberCarry();
	if (rS == rB) {
		if (rS == rA) {
			/* mr. rA, rA */
			NativeReg a = jitc.getClientRegister(PPC_GPR(rA));
			jitc.asmALU32(X86_TEST, a, a);
		} else {
			/* mr. rA, rS*/
			NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
			NativeReg a = jitc.mapClientRegisterDirty(PPC_GPR(rA));
			jitc.asmALU32(X86_MOV, a, s);
			jitc.asmALU32(X86_TEST, a, a);
		}
	} else {
		if (rA == rS) {
			// or a, a, b
			NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
			NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
			jitc.asmALU32(X86_OR, a, b);
		} else if (rA == rB) {
			// or a, s, a
			NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
			NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
			jitc.asmALU32(X86_OR, a, s);
		} else {
			// or a, s, b
			NativeReg a = jitc.mapClientRegisterDirty(PPC_GPR(rA));
			NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
			NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
			jitc.asmALU32(X86_MOV, a, s);
			jitc.asmALU32(X86_OR, a, b);			
		}
	}
	jitc.mapFlagsDirty();
	return flowContinue;
}
JITCFlow ppc_opc_gen_orx(JITC &jitc)
{
	if (jitc.current_opc & PPC_OPC_Rc) {
		return ppc_opc_gen_orp(jitc);
	} else {
		return ppc_opc_gen_or(jitc);
	}
}
/*
 *	orcx		OR with Complement
 *	.604
 */
void ppc_opc_orcx(PPC_CPU_State &aCPU)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	aCPU.gpr[rA] = aCPU.gpr[rS] | ~aCPU.gpr[rB];
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_orcx(JITC &jitc)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.clobberCarry();
	} else {
		jitc.clobberCarryAndFlags();
	}
	if (rA == rS) {
		NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
		NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
		NativeReg tmp = jitc.allocRegister();
		jitc.asmALU32(X86_MOV, tmp, b);
		jitc.asmALU32(X86_NOT, tmp);
		jitc.asmALU32(X86_OR, a, tmp);
	} else if (rA == rB) {
		NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
		NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
		jitc.asmALU32(X86_NOT, a);
		jitc.asmALU32(X86_OR, a, s);
	} else {
		NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
		NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
		NativeReg a = jitc.mapClientRegisterDirty(PPC_GPR(rA));
		jitc.asmALU32(X86_MOV, a, b);
		jitc.asmALU32(X86_NOT, a);
		jitc.asmALU32(X86_OR, a, s);
	}
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.mapFlagsDirty();
	}
	return flowContinue;
}
/*
 *	ori		OR Immediate
 *	.605
 */
void ppc_opc_ori(PPC_CPU_State &aCPU)
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_UImm(aCPU.current_opc, rS, rA, imm);
	aCPU.gpr[rA] = aCPU.gpr[rS] | imm;
}
JITCFlow ppc_opc_gen_ori(JITC &jitc)
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_UImm(jitc.current_opc, rS, rA, imm);
	return ppc_opc_gen_ori_oris_xori_xoris(jitc, X86_OR, imm, rS, rA);
}
/*
 *	oris		OR Immediate Shifted
 *	.606
 */
void ppc_opc_oris(PPC_CPU_State &aCPU)
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_Shift16(aCPU.current_opc, rS, rA, imm);
	aCPU.gpr[rA] = aCPU.gpr[rS] | imm;
}
JITCFlow ppc_opc_gen_oris(JITC &jitc)
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_Shift16(jitc.current_opc, rS, rA, imm);
	return ppc_opc_gen_ori_oris_xori_xoris(jitc, X86_OR, imm, rS, rA);
}
/*
 *	rlwimix		Rotate Left Word Immediate then Mask Insert
 *	.617
 */
void ppc_opc_rlwimix(PPC_CPU_State &aCPU)
{
	int rS, rA, SH, MB, ME;
	PPC_OPC_TEMPL_M(aCPU.current_opc, rS, rA, SH, MB, ME);
	uint32 v = ppc_word_rotl(aCPU.gpr[rS], SH);
	uint32 mask = ppc_mask(MB, ME);
	aCPU.gpr[rA] = (v & mask) | (aCPU.gpr[rA] & ~mask);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rA]);
	}
}
static void ppc_opc_gen_rotl_and(JITC &jitc, NativeReg r, int SH, uint32 mask)
{
	SH &= 0x1f;
	if (SH) {
		if (mask & ((1<<SH)-1)) {
			if (mask & ~((1<<SH)-1)) {
				if (SH == 31) {
					jitc.asmShift32(X86_ROR, r, 1);
				} else {
					jitc.asmShift32(X86_ROL, r, SH);
				}
			} else {
				jitc.asmShift32(X86_SHR, r, 32-SH);
			}
		} else {
			jitc.asmShift32(X86_SHL, r, SH);
		}
	}	
}
JITCFlow ppc_opc_gen_rlwimix(JITC &jitc)
{
	int rS, rA, SH, MB, ME;
	PPC_OPC_TEMPL_M(jitc.current_opc, rS, rA, SH, MB, ME);
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.clobberCarry();
	} else {
		jitc.clobberCarryAndFlags();
	}
	uint32 mask = ppc_mask(MB, ME);
	NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
	NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
	NativeReg tmp = jitc.allocRegister();
	jitc.asmALU32(X86_MOV, tmp, s);
	ppc_opc_gen_rotl_and(jitc, tmp, SH, mask);
	jitc.asmALU32(X86_AND, a, ~mask);
	jitc.asmALU32(X86_AND, tmp, mask);
	jitc.asmALU32(X86_OR, a, tmp);
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.mapFlagsDirty();
	}
	return flowContinue;
}
/*
 *	rlwinmx		Rotate Left Word Immediate then AND with Mask
 *	.618
 */
void ppc_opc_rlwinmx(PPC_CPU_State &aCPU)
{
	int rS, rA, SH;
	uint32 MB, ME;
	PPC_OPC_TEMPL_M(aCPU.current_opc, rS, rA, SH, MB, ME);
	uint32 v = ppc_word_rotl(aCPU.gpr[rS], SH);
	uint32 mask = ppc_mask(MB, ME);
	aCPU.gpr[rA] = v & mask;
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_rlwinmx(JITC &jitc)
{
	int rS, rA, SH;
	uint32 MB, ME;
	PPC_OPC_TEMPL_M(jitc.current_opc, rS, rA, SH, MB, ME);
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.clobberCarry();
	} else {
		jitc.clobberCarryAndFlags();
	}
	NativeReg a;
	if (rS == rA) {
		a = jitc.getClientRegisterDirty(PPC_GPR(rA));
	} else {
		NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
		a = jitc.mapClientRegisterDirty(PPC_GPR(rA));
		jitc.asmALU32(X86_MOV, a, s);
	}
	uint32 mask = ppc_mask(MB, ME);
	ppc_opc_gen_rotl_and(jitc, a, SH, mask);
	jitc.asmALU32(X86_AND, a, mask);
	if (jitc.current_opc & PPC_OPC_Rc) {
		/*
		 *	Important side-node:
		 *	ROL doesn't update the flags, so beware if you want to
		 *	get rid of the above AND
		 */
		jitc.mapFlagsDirty();
	}
	return flowContinue;
}
/*
 *	rlwnmx		Rotate Left Word then AND with Mask
 *	.620
 */
void ppc_opc_rlwnmx(PPC_CPU_State &aCPU)
{
	int rS, rA, rB, MB, ME;
	PPC_OPC_TEMPL_M(aCPU.current_opc, rS, rA, rB, MB, ME);
	uint32 v = ppc_word_rotl(aCPU.gpr[rS], aCPU.gpr[rB]);
	uint32 mask = ppc_mask(MB, ME);
	aCPU.gpr[rA] = v & mask;
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_rlwnmx(JITC &jitc)
{
	int rS, rA, rB, MB, ME;
	PPC_OPC_TEMPL_M(jitc.current_opc, rS, rA, rB, MB, ME);
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.clobberCarry();
	} else {
		jitc.clobberCarryAndFlags();
	}
	NativeReg a;
	jitc.getClientRegister(PPC_GPR(rB), NATIVE_REG | RCX);
	if (rS == rA) {
		a = jitc.getClientRegisterDirty(PPC_GPR(rA));
	} else {
		NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
		if (rA == rB) {
			a = jitc.allocRegister();
		} else {
			a = jitc.mapClientRegisterDirty(PPC_GPR(rA));
		}
		jitc.asmALU32(X86_MOV, a, s);
	}
	jitc.asmShift32CL(X86_ROL, a);
	if (rA != rS && rA == rB) {
		jitc.mapClientRegisterDirty(PPC_GPR(rA), NATIVE_REG | a);
	}
	uint32 mask = ppc_mask(MB, ME);
	jitc.asmALU32(X86_AND, a, mask);
	if (jitc.current_opc & PPC_OPC_Rc) {
		/*
		 *	Important side-node:
		 *	ROL doesn't update the flags, so beware if you want to
		 *	get rid of the above AND
		 */
		jitc.mapFlagsDirty();
	}
	return flowContinue;
}

/*
 *	slwx		Shift Left Word
 *	.625
 */
void ppc_opc_slwx(PPC_CPU_State &aCPU)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	uint32 s = aCPU.gpr[rB] & 0x3f;
	if (s > 31) {
		aCPU.gpr[rA] = 0;
	} else {
		aCPU.gpr[rA] = aCPU.gpr[rS] << s;
	}
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_slwx(JITC &jitc)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.clobberCarry();
	} else {
		jitc.clobberCarryAndFlags();
	}
	NativeReg a;
	NativeReg b = jitc.getClientRegister(PPC_GPR(rB), NATIVE_REG | RCX);
	jitc.asmALU32(X86_TEST, b, 0x20);
	if (rA == rS) {
		a = jitc.getClientRegisterDirty(PPC_GPR(rA));
	} else {
		NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
		if (rA == rB) {
			a = jitc.allocRegister();
		} else {
			a = jitc.mapClientRegisterDirty(PPC_GPR(rA));
		}
		jitc.asmALU32(X86_MOV, a, s);
	}
	NativeAddress fixup = jitc.asmJxxFixup(X86_Z);
	jitc.asmALU32(X86_MOV, a, 0);
	jitc.asmResolveFixup(fixup, jitc.asmHERE());
	jitc.asmShift32CL(X86_SHL, a);
	if (rA != rS && rA == rB) {
		jitc.mapClientRegisterDirty(PPC_GPR(rA), NATIVE_REG | a);
	}
	if (jitc.current_opc & PPC_OPC_Rc) {
		/*
		 *	Welcome to the wonderful world of braindead 
		 *	processor design.
		 *	(shl x, cl doesn't update the flags in case of cl==0)
		 */
		jitc.asmALU32(X86_TEST, a, a);
		jitc.mapFlagsDirty();
	}
	return flowContinue;
}
/*
 *	srawx		Shift Right Algebraic Word
 *	.628
 */
void ppc_opc_srawx(PPC_CPU_State &aCPU)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	uint32 SH = aCPU.gpr[rB] & 0x3f;
	aCPU.gpr[rA] = aCPU.gpr[rS];
	aCPU.xer_ca = 0;
	if (aCPU.gpr[rA] & 0x80000000) {
		uint32 ca = 0;
		for (uint i=0; i < SH; i++) {
			if (aCPU.gpr[rA] & 1) ca = 1;
			aCPU.gpr[rA] >>= 1;
			aCPU.gpr[rA] |= 0x80000000;
		}
		if (ca) aCPU.xer_ca = 1;
	} else {
		if (SH > 31) {
			aCPU.gpr[rA] = 0;
		} else {
			aCPU.gpr[rA] >>= SH;
		}
	}     
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_srawx(JITC &jitc)
{
#if 0
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
	if (!(jitc.current_opc & PPC_OPC_Rc)) {
		jitc.clobberFlags();
	}
	NativeReg a = REG_NO;
	jitc.getClientRegister(PPC_GPR(rB), NATIVE_REG | ECX);
	jitc.asmALU32(X86_TEST, ECX, 0x20);
	NativeAddress ecx_gt_1f = asmJxxFixup(X86_NZ);

		// 0 <= SH <= 31
		NativeReg t;
		if (rS != rA) {
			NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
			a = jitc.mapClientRegisterDirty(PPC_GPR(rA));
			jitc.asmALU32(X86_MOV, a, s);
			t = jitc.allocRegister();
			jitc.asmALU32(X86_MOV, t, s);
		} else {
			a = jitc.getClientRegisterDirty(PPC_GPR(rA));
			t = jitc.allocRegister();
			jitc.asmALU32(X86_MOV, t, a);
		}
		asmShiftRegImm(X86_SAR, t, 31);
		jitc.asmALU32(X86_AND, t, a);
		asmShiftRegCL(X86_SAR, a);
		static int test_values[] = {
			0x00000000, 0x00000001, 0x00000003, 0x00000007,
			0x0000000f, 0x0000001f, 0x0000003f, 0x0000007f,
			0x000000ff, 0x000001ff, 0x000003ff, 0x000007ff,
			0x00000fff, 0x00001fff, 0x00003fff, 0x00007fff,
			0x0000ffff, 0x0001ffff, 0x0003ffff, 0x0007ffff,
			0x000fffff, 0x001fffff, 0x003fffff, 0x007fffff,
			0x00ffffff, 0x01ffffff, 0x03ffffff, 0x07ffffff,
			0x0fffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff,
		};
		asmALURegMem(X86_TEST, t, modrm, x86_mem_sib(modrm, REG_NO, 4, ECX, (uint32)&test_values));
    		asmSETMem(X86_NZ, modrm, x86_mem(modrm, REG_NO, (uint32)&aCPU.xer_ca));
		if (jitc.current_opc & PPC_OPC_Rc) {
			jitc.asmALU32(X86_TEST, a, a);
		}
	NativeAddress end = jitc.asmJMPFixup();


	jitc.asmResolveFixup(ecx_gt_1f, jitc.asmHERE());	
		// SH > 31
		if (rS != rA) {
			NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
			a = jitc.mapClientRegisterDirty(PPC_GPR(rA));
			jitc.asmALU32(X86_MOV, a, s);
		} else {
			a = jitc.getClientRegister(PPC_GPR(rA));
		}
		asmShiftRegImm(X86_SAR, a, 31);
		asmShiftRegImm(X86_SAR, a, 1);
		jitc.mapCarryDirty();
	jitc.asmResolveFixup(end, jitc.asmHERE());

	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.mapFlagsDirty();
	}

	return flowContinue;
#endif
	ppc_opc_gen_interpret(jitc, ppc_opc_srawx);
	return flowEndBlock;
}
/*
 *	srawix		Shift Right Algebraic Word Immediate
 *	.629
 */
void ppc_opc_srawix(PPC_CPU_State &aCPU)
{
	int rS, rA;
	uint32 SH;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, SH);
	aCPU.gpr[rA] = aCPU.gpr[rS];
	aCPU.xer_ca = 0;
	if (aCPU.gpr[rA] & 0x80000000) {
		uint32 ca = 0;
		for (uint i=0; i < SH; i++) {
			if (aCPU.gpr[rA] & 1) ca = 1;
			aCPU.gpr[rA] >>= 1;
			aCPU.gpr[rA] |= 0x80000000;
		}
		if (ca) aCPU.xer_ca = 1;
	} else {
		aCPU.gpr[rA] >>= SH;
	}     
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_srawix(JITC &jitc)
{
	int rS, rA;
	uint32 SH;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, SH);
	if (!(jitc.current_opc & PPC_OPC_Rc)) {
		jitc.clobberFlags();
	}
	NativeReg a = REG_NO;
	if (SH) {
		NativeReg t;
		if (rS != rA) {
			NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
			a = jitc.mapClientRegisterDirty(PPC_GPR(rA));
			jitc.asmALU32(X86_MOV, a, s);
			t = jitc.allocRegister();
			jitc.asmALU32(X86_MOV, t, s);
		} else {
			a = jitc.getClientRegisterDirty(PPC_GPR(rA));
			t = jitc.allocRegister();
			jitc.asmALU32(X86_MOV, t, a);
		}
		jitc.asmShift32(X86_SAR, t, 31);
		jitc.asmALU32(X86_AND, t, a);
		jitc.asmShift32(X86_SAR, a, SH);
		jitc.asmALU32(X86_TEST, t, (1<<SH)-1);
    		jitc.asmSET8(X86_NZ, curCPU(xer_ca));
	} else {
		if (rS != rA) {
			NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
			a = jitc.mapClientRegisterDirty(PPC_GPR(rA));
			jitc.asmALU32(X86_MOV, a, s);
		} else if (jitc.current_opc & PPC_OPC_Rc) {
			a = jitc.getClientRegister(PPC_GPR(rA));
		}
		jitc.asmALU8(X86_MOV, curCPU(xer_ca), 0);
	}
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.asmALU32(X86_TEST, a, a);
		jitc.mapFlagsDirty();
	}
	return flowContinue;
}
/*
 *	srwx		Shift Right Word
 *	.631
 */
void ppc_opc_srwx(PPC_CPU_State &aCPU)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	uint32 v = aCPU.gpr[rB] & 0x3f;
	if (v > 31) {
		aCPU.gpr[rA] = 0;
	} else {
		aCPU.gpr[rA] = aCPU.gpr[rS] >> v;
	}
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_srwx(JITC &jitc)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.clobberCarry();
	} else {
		jitc.clobberCarryAndFlags();
	}
	NativeReg a;
	NativeReg b = jitc.getClientRegister(PPC_GPR(rB), NATIVE_REG | RCX);
	jitc.asmALU32(X86_TEST, b, 0x20);
	if (rA == rS) {
		a = jitc.getClientRegisterDirty(PPC_GPR(rA));
	} else {
		NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
		if (rA == rB) {
			a = jitc.allocRegister();
		} else {
			a = jitc.mapClientRegisterDirty(PPC_GPR(rA));
		}
		jitc.asmALU32(X86_MOV, a, s);
	}
	NativeAddress fixup = jitc.asmJxxFixup(X86_Z);
	jitc.asmALU32(X86_MOV, a, 0);
	jitc.asmResolveFixup(fixup, jitc.asmHERE());
	jitc.asmShift32CL(X86_SHR, a);
	if (rA != rS && rA == rB) {
		jitc.mapClientRegisterDirty(PPC_GPR(rA), NATIVE_REG | a);
	}
	if (jitc.current_opc & PPC_OPC_Rc) {
		/*
		 *	Welcome to the wonderful world of braindead 
		 *	processor design.
		 *	(shr x, cl doesn't update the flags in case of cl==0)
		 */
		jitc.asmALU32(X86_TEST, a, a);
		jitc.mapFlagsDirty();
	}
	return flowContinue;
/*	ppc_opc_gen_interpret(jitc, ppc_opc_srwx);
	return flowEndBlock;*/
}

/*
 *	subfx		Subtract From
 *	.666
 */
void ppc_opc_subfx(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
	aCPU.gpr[rD] = ~aCPU.gpr[rA] + aCPU.gpr[rB] + 1;
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rD]);
	}
}
JITCFlow ppc_opc_gen_subfx(JITC &jitc)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(jitc.current_opc, rD, rA, rB);
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.clobberCarry();
	} else {
		jitc.clobberCarryAndFlags();
	}
	NativeReg a = jitc.getClientRegister(PPC_GPR(rA));
	NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
	if (rD == rA) {
		if (rD == rB) {
			jitc.asmALU32(X86_MOV, a, 0);
		} else {
			// subf rA, rA, rB (a = b - a)
			jitc.asmALU32(X86_NEG, a);
			jitc.asmALU32(X86_ADD, a, b);
		}
		jitc.dirtyRegister(a);
	} else if (rD == rB) {
		// subf rB, rA, rB (b = b - a)
		jitc.asmALU32(X86_SUB, b, a);
		jitc.dirtyRegister(b);
	} else {
		// subf rD, rA, rB (d = b - a)
		NativeReg d = jitc.mapClientRegisterDirty(PPC_GPR(rD));
		jitc.asmALU32(X86_MOV, d, b);
		jitc.asmALU32(X86_SUB, d, a);
	}
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.mapFlagsDirty();
	}
	return flowContinue;
}
/*
 *	subfox		Subtract From with Overflow
 *	.666
 */
void ppc_opc_subfox(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
	aCPU.gpr[rD] = ~aCPU.gpr[rA] + aCPU.gpr[rB] + 1;
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rD]);
	}
	// update XER flags
	PPC_ALU_ERR("subfox unimplemented\n");
}
/*
 *	subfcx		Subtract From Carrying
 *	.667
 */
void ppc_opc_subfcx(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
	uint32 a = aCPU.gpr[rA];
	uint32 b = aCPU.gpr[rB];
	aCPU.gpr[rD] = ~a + b + 1;
	aCPU.xer_ca = ppc_carry_3(~a, b, 1);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rD]);
	}
}
JITCFlow ppc_opc_gen_subfcx(JITC &jitc)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(jitc.current_opc, rD, rA, rB);
	if (!(jitc.current_opc & PPC_OPC_Rc)) {
		jitc.clobberFlags();
	}	
	NativeReg a = jitc.getClientRegister(PPC_GPR(rA));
	NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
	if (rA != rD) {
		if (rD == rB) {
			// b = b - a
			jitc.asmALU32(X86_SUB, b, a);
			jitc.dirtyRegister(b);
		} else {
			// d = b - a
			NativeReg d = jitc.mapClientRegisterDirty(PPC_GPR(rD));
			jitc.asmALU32(X86_MOV, d, b);
			jitc.asmALU32(X86_SUB, d, a);
		}
	} else {
		// a = b - a
		NativeReg tmp = jitc.allocRegister();
		jitc.asmALU32(X86_MOV, tmp, b);
		jitc.asmALU32(X86_SUB, tmp, a);
		jitc.mapClientRegisterDirty(PPC_GPR(rA), NATIVE_REG | tmp);
	}
	jitc.asmSimple(X86_CMC);
	jitc.mapCarryDirty();
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.mapFlagsDirty();
	}
	return flowContinue;
}
/*
 *	subfcox		Subtract From Carrying with Overflow
 *	.667
 */
void ppc_opc_subfcox(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
	uint32 a = aCPU.gpr[rA];
	uint32 b = aCPU.gpr[rB];
	aCPU.gpr[rD] = ~a + b + 1;
	aCPU.xer_ca = ppc_carry_3(~a, b, 1);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rD]);
	}
	// update XER flags
	PPC_ALU_ERR("subfcox unimplemented\n");
}
/*
 *	subfex		Subtract From Extended
 *	.668
 */
void ppc_opc_subfex(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
	uint32 a = aCPU.gpr[rA];
	uint32 b = aCPU.gpr[rB];
	uint32 ca = (aCPU.xer_ca);
	aCPU.gpr[rD] = ~a + b + ca;
	aCPU.xer_ca = ppc_carry_3(~a, b, ca);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rD]);
	}
}
JITCFlow ppc_opc_gen_subfex(JITC &jitc)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(jitc.current_opc, rD, rA, rB);
	if (!(jitc.current_opc & PPC_OPC_Rc)) {
		jitc.clobberFlags();
	}
	NativeReg a = jitc.getClientRegister(PPC_GPR(rA));
	NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
	jitc.getClientCarry();
	jitc.asmSimple(X86_CMC);
	if (rA != rD) {
		if (rD == rB) {
			// b = b - a
			jitc.asmALU32(X86_SBB, b, a);
			jitc.dirtyRegister(b);
		} else {
			// d = b - a
			NativeReg d = jitc.mapClientRegisterDirty(PPC_GPR(rD));
			jitc.asmALU32(X86_MOV, d, b);
			jitc.asmALU32(X86_SBB, d, a);
		}
	} else {
		// a = b - a
		NativeReg tmp = jitc.allocRegister();
		jitc.asmALU32(X86_MOV, tmp, b);
		jitc.asmALU32(X86_SBB, tmp, a);
		jitc.mapClientRegisterDirty(PPC_GPR(rA), NATIVE_REG | tmp);
	}
	jitc.asmSimple(X86_CMC);
	jitc.mapCarryDirty();
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.mapFlagsDirty();
	}
	return flowContinue;
}
/*
 *	subfeox		Subtract From Extended with Overflow
 *	.668
 */
void ppc_opc_subfeox(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
	uint32 a = aCPU.gpr[rA];
	uint32 b = aCPU.gpr[rB];
	uint32 ca = aCPU.xer_ca;
	aCPU.gpr[rD] = ~a + b + ca;
	aCPU.xer_ca = (ppc_carry_3(~a, b, ca));
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rD]);
	}
	// update XER flags
	PPC_ALU_ERR("subfeox unimplemented\n");
}
/*
 *	subfic		Subtract From Immediate Carrying
 *	.669
 */
void ppc_opc_subfic(PPC_CPU_State &aCPU)
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(aCPU.current_opc, rD, rA, imm);
	uint32 a = aCPU.gpr[rA];
	aCPU.gpr[rD] = ~a + imm + 1;
	aCPU.xer_ca = (ppc_carry_3(~a, imm, 1));
}
JITCFlow ppc_opc_gen_subfic(JITC &jitc)
{
	int rD, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_SImm(jitc.current_opc, rD, rA, imm);
	jitc.clobberFlags();
	NativeReg d;
	if (rA == rD) {
		d = jitc.getClientRegisterDirty(PPC_GPR(rD));
	} else {
		NativeReg a = jitc.getClientRegister(PPC_GPR(rA));
		d = jitc.mapClientRegisterDirty(PPC_GPR(rD));
		jitc.asmALU32(X86_MOV, d, a);
	}
	jitc.asmALU32(X86_NOT, d);
	if (imm == 0xffffffff) {
		jitc.asmSimple(X86_STC);
	} else {
		jitc.asmALU32(X86_ADD, d, imm+1);
	}
	jitc.mapCarryDirty();
	return flowContinue;
}
/*
 *	subfmex		Subtract From Minus One Extended
 *	.670
 */
void ppc_opc_subfmex(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
	PPC_OPC_ASSERT(rB == 0);
	uint32 a = aCPU.gpr[rA];
	uint32 ca = aCPU.xer_ca;
	aCPU.gpr[rD] = ~a + ca + 0xffffffff;
	aCPU.xer_ca = ((a!=0xffffffff) || ca);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rD]);
	}
}
JITCFlow ppc_opc_gen_subfmex(JITC &jitc)
{
	ppc_opc_gen_interpret(jitc, ppc_opc_subfmex);
	return flowEndBlock;
}
/*
 *	subfmeox	Subtract From Minus One Extended with Overflow
 *	.670
 */
void ppc_opc_subfmeox(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
	PPC_OPC_ASSERT(rB == 0);
	uint32 a = aCPU.gpr[rA];
	uint32 ca = aCPU.xer_ca;
	aCPU.gpr[rD] = ~a + ca + 0xffffffff;
	aCPU.xer_ca = ((a!=0xffffffff) || ca);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rD]);
	}
	// update XER flags
	PPC_ALU_ERR("subfmeox unimplemented\n");
}
/*
 *	subfzex		Subtract From Zero Extended
 *	.671
 */
void ppc_opc_subfzex(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
	PPC_OPC_ASSERT(rB == 0);
	uint32 a = aCPU.gpr[rA];
	uint32 ca = aCPU.xer_ca;
	aCPU.gpr[rD] = ~a + ca;
	aCPU.xer_ca = (!a && ca);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rD]);
	}
}
JITCFlow ppc_opc_gen_subfzex(JITC &jitc)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(jitc.current_opc, rD, rA, rB);
	if (!(jitc.current_opc & PPC_OPC_Rc)) {
		jitc.clobberFlags();
	}
	NativeReg a = jitc.getClientRegister(PPC_GPR(rA));
	if (rD != rA) {
		NativeReg d = jitc.mapClientRegisterDirty(PPC_GPR(rD));
		jitc.asmALU32(X86_MOV, d, a);
		a = d;
	}
	jitc.getClientCarry();
	jitc.asmALU32(X86_NOT, a);
	jitc.asmALU32(X86_ADC, a, 0);
	jitc.dirtyRegister(a);
	jitc.mapCarryDirty();
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.mapFlagsDirty();
	}
	return flowContinue;
}
/*
 *	subfzeox	Subtract From Zero Extended with Overflow
 *	.671
 */
void ppc_opc_subfzeox(PPC_CPU_State &aCPU)
{
	int rD, rA, rB;
	PPC_OPC_TEMPL_XO(aCPU.current_opc, rD, rA, rB);
	PPC_OPC_ASSERT(rB == 0);
	uint32 a = aCPU.gpr[rA];
	uint32 ca = aCPU.xer_ca;
	aCPU.gpr[rD] = ~a + ca;
	aCPU.xer_ca = (!a && ca);
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rD]);
	}
	// update XER flags
	PPC_ALU_ERR("subfzeox unimplemented\n");
}

/*
 *	xorx		XOR
 *	.680
 */
void ppc_opc_xorx(PPC_CPU_State &aCPU)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(aCPU.current_opc, rS, rA, rB);
	aCPU.gpr[rA] = aCPU.gpr[rS] ^ aCPU.gpr[rB];
	if (aCPU.current_opc & PPC_OPC_Rc) {
		ppc_update_cr0(aCPU, aCPU.gpr[rA]);
	}
}
JITCFlow ppc_opc_gen_xorx(JITC &jitc)
{
	int rS, rA, rB;
	PPC_OPC_TEMPL_X(jitc.current_opc, rS, rA, rB);
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.clobberCarry();
	} else {
		jitc.clobberCarryAndFlags();
	}
	if (rA == rS) {
		NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
		NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
		jitc.asmALU32(X86_XOR, a, b);
	} else if (rA == rB) {
		NativeReg a = jitc.getClientRegisterDirty(PPC_GPR(rA));
		NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
		jitc.asmALU32(X86_XOR, a, s);
	} else {
		NativeReg s = jitc.getClientRegister(PPC_GPR(rS));
		NativeReg b = jitc.getClientRegister(PPC_GPR(rB));
		NativeReg a = jitc.mapClientRegisterDirty(PPC_GPR(rA));
		jitc.asmALU32(X86_MOV, a, s);
		jitc.asmALU32(X86_XOR, a, b);
	}
	if (jitc.current_opc & PPC_OPC_Rc) {
		jitc.mapFlagsDirty();
	}
	return flowContinue;
}
/*
 *	xori		XOR Immediate
 *	.681
 */
void ppc_opc_xori(PPC_CPU_State &aCPU)
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_UImm(aCPU.current_opc, rS, rA, imm);
	aCPU.gpr[rA] = aCPU.gpr[rS] ^ imm;
}
JITCFlow ppc_opc_gen_xori(JITC &jitc)
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_UImm(jitc.current_opc, rS, rA, imm);
	return ppc_opc_gen_ori_oris_xori_xoris(jitc, X86_XOR, imm, rS, rA);
}
/*
 *	xoris		XOR Immediate Shifted
 *	.682
 */
void ppc_opc_xoris(PPC_CPU_State &aCPU)
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_Shift16(aCPU.current_opc, rS, rA, imm);
	aCPU.gpr[rA] = aCPU.gpr[rS] ^ imm;
}
JITCFlow ppc_opc_gen_xoris(JITC &jitc)
{
	int rS, rA;
	uint32 imm;
	PPC_OPC_TEMPL_D_Shift16(jitc.current_opc, rS, rA, imm);
	return ppc_opc_gen_ori_oris_xori_xoris(jitc, X86_XOR, imm, rS, rA);
}
