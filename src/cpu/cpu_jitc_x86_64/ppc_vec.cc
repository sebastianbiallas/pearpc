#if 0
/*
 *	PearPC
 *	ppc_vec.cc
 *
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

/*	Pages marked: v.???
 *	From: IBM PowerPC MicroProcessor Family: Altivec(tm) Technology...
 *		Programming Environments Manual
 */
 
#include <string.h>
#include <math.h>

/*
 *	FIXME: put somewhere appropriate
 */
#ifndef HAS_LOG2
#define log2(x) log(x)/log(2)
#endif /* HAS_LOG2 */ 

#ifndef HAS_EXP2
#define exp2(x)	pow(2, x)
#endif /* HAS_EXP2 */

#define ASSERT_FLUSHED(vrA)
//#define ASSERT_FLUSHED(vrA)	jitcAssertFlushedVectorRegister(vrA)

#include "debug/tracers.h"
#include "ppc_cpu.h"
#include "ppc_dec.h"
#include "ppc_fpu.h"
#include "ppc_vec.h"

#define	SIGN32	0x80000000

#define RL2RH(RL)	(NativeReg8)(RL+4)

#define vrT	JITC_VECTOR_TEMP
#define vrNEG1	JITC_VECTOR_NEG1

static inline void commutative_operation(X86ALUPSopc opc, int vrD, int vrA, int vrB);
static inline void noncommutative_operation(X86ALUPSopc opc, int vrD, int vrA, int vrB);

static inline void commutative_operation(X86PALUopc opc, int vrD, int vrA, int vrB);
static inline void noncommutative_operation(X86PALUopc opc, int vrD, int vrA, int vrB);

/*	PACK_PIXEL	Packs a uint32 pixel to uint16 pixel
 *	v.219
 */
static inline uint16 PACK_PIXEL(uint32 clr)
{
	return	(((clr & 0x000000f8) >> 3) | \
		 ((clr & 0x0000f800) >> 6) | \
		 ((clr & 0x01f80000) >> 9));
}

/*	UNPACK_PIXEL	Unpacks a uint16 pixel to uint32 pixel
 *	v.276 & v.279
 */
static inline uint32 UNPACK_PIXEL(uint16 clr)
{
	return	(((uint32)(clr & 0x001f)) | \
		 ((uint32)(clr & 0x03E0) << 3) | \
		 ((uint32)(clr & 0x7c00) << 6) | \
		 (((clr) & 0x8000) ? 0xff000000 : 0));
}

static inline uint8 SATURATE_UB(uint16 val)
{
	if (val & 0xff00) {
		gCPU.vscr |= VSCR_SAT;
		return 0xff;
	}
	return val;
}
static inline uint8 SATURATE_0B(uint16 val)
{
	if (val & 0xff00) {
		gCPU.vscr |= VSCR_SAT;
		return 0;
	}
	return val;
}

static inline uint16 SATURATE_UH(uint32 val)
{
	if (val & 0xffff0000) {
		gCPU.vscr |= VSCR_SAT;
		return 0xffff;
	}
	return val;
}

static inline uint16 SATURATE_0H(uint32 val)
{
	if (val & 0xffff0000) {
		gCPU.vscr |= VSCR_SAT;
		return 0;
	}
	return val;
}

static inline sint8 SATURATE_SB(sint16 val)
{
	if (val > 127) {			// 0x7F
		gCPU.vscr |= VSCR_SAT;
		return 127;
	} else if (val < -128) {		// 0x80
		gCPU.vscr |= VSCR_SAT;
		return -128;
	}
	return val;
}

static inline uint8 SATURATE_USB(sint16 val)
{
	if (val > 0xff) {
		gCPU.vscr |= VSCR_SAT;
		return 0xff;
	} else if (val < 0) {
		gCPU.vscr |= VSCR_SAT;
		return 0;
	}
	return (uint8)val;
}

static inline sint16 SATURATE_SH(sint32 val)
{
	if (val > 32767) {			// 0x7fff
		gCPU.vscr |= VSCR_SAT;
		return 32767;
	} else if (val < -32768) {		// 0x8000
		gCPU.vscr |= VSCR_SAT;
		return -32768;
	}
	return val;
}

static inline uint16 SATURATE_USH(sint32 val)
{
	if (val > 0xffff) {
		gCPU.vscr |= VSCR_SAT;
		return 0xffff;
	} else if (val < 0) {
		gCPU.vscr |= VSCR_SAT;
		return 0;
	}
	return (uint16)val;
}

static inline sint32 SATURATE_UW(sint64 val)
{
	if (val > 0xffffffffLL) {
		gCPU.vscr |= VSCR_SAT;
		return 0xffffffffLL;
	}
	return val;
}

static inline sint32 SATURATE_SW(sint64 val)
{
	if (val > 2147483647LL) {			// 0x7fffffff
		gCPU.vscr |= VSCR_SAT;
		return 2147483647LL;
	} else if (val < -2147483648LL) {		// 0x80000000
		gCPU.vscr |= VSCR_SAT;
		return -2147483648LL;
	}
	return val;
}

static inline void vec_raise_saturate(void)
{
	asmOR(&gCPU.vscr, VSCR_SAT);
}

static inline void vec_saturateSB(NativeReg reg, NativeReg reg2)
{
	jitcClobberCarryAndFlags();

	asmMOV_NoFlags(reg2, 0x7f);
	asmALU(X86_CMP, reg, reg2);
	NativeAddress skip1 = asmJxxFixup(X86_G);

	asmMOV_NoFlags(reg2, 0xffffff80);
	asmALU(X86_CMP, reg, reg2);
	NativeAddress skip2 = asmJxxFixup(X86_L);

	NativeAddress skip3 = asmJMPFixup();

	asmResolveFixup(skip1);
	asmResolveFixup(skip2);

	asmALU(X86_MOV, reg, reg2);
	vec_raise_saturate();

	asmResolveFixup(skip3);
}

static inline void vec_saturateSUB(NativeReg reg, NativeReg reg2)
{
	jitcClobberCarryAndFlags();

	asmMOV_NoFlags(reg2, 0xff);
	asmALU(X86_CMP, reg, reg2);
	NativeAddress skip1 = asmJxxFixup(X86_G);

	asmALU(X86_XOR, reg2, reg2);
	asmALU(X86_CMP, reg, reg2);
	NativeAddress skip2 = asmJxxFixup(X86_L);

	NativeAddress skip3 = asmJMPFixup();

	asmResolveFixup(skip1);
	asmResolveFixup(skip2);

	asmALU(X86_MOV, reg, reg2);
	vec_raise_saturate();

	asmResolveFixup(skip3);
}

static inline void vec_saturateUB(NativeReg8 reg)
{
	jitcClobberCarryAndFlags();

	asmALU(X86_CMP, (NativeReg)reg, 0xff);
	NativeAddress skip1 = asmJxxFixup(X86_BE);

	asmSET(X86_S, reg);
	asmALU(X86_SUB, reg, 1);

	vec_raise_saturate();

	asmResolveFixup(skip1);
}


static inline void vec_saturateSH(NativeReg reg, NativeReg reg2)
{
	jitcClobberCarryAndFlags();

	asmMOV_NoFlags(reg2, 0x7fff);
	asmALU(X86_CMP, reg, reg2);
	NativeAddress skip1 = asmJxxFixup(X86_G);

	asmMOV_NoFlags(reg2, 0xffff8000);
	asmALU(X86_CMP, reg, reg2);
	NativeAddress skip2 = asmJxxFixup(X86_L);

	NativeAddress skip3 = asmJMPFixup();

	asmResolveFixup(skip1);
	asmResolveFixup(skip2);

	asmALU(X86_MOV, reg, reg2);
	vec_raise_saturate();

	asmResolveFixup(skip3);
}

static inline void vec_saturateSUH(NativeReg reg, NativeReg reg2)
{
	jitcClobberCarryAndFlags();

	asmMOV_NoFlags(reg2, 0xffff);
	asmALU(X86_CMP, reg, reg2);
	NativeAddress skip1 = asmJxxFixup(X86_G);

	asmALU(X86_XOR, reg2, reg2);
	asmALU(X86_CMP, reg, reg2);
	NativeAddress skip2 = asmJxxFixup(X86_L);

	NativeAddress skip3 = asmJMPFixup();

	asmResolveFixup(skip1);
	asmResolveFixup(skip2);

	asmALU(X86_MOV, reg, reg2);
	vec_raise_saturate();

	asmResolveFixup(skip3);
}

static inline void vec_saturateUH(NativeReg8 reg)
{
	jitcClobberCarryAndFlags();

	asmALU(X86_CMP, (NativeReg)reg, 0xffff);
	NativeAddress skip1 = asmJxxFixup(X86_BE);

	asmSET(X86_S, reg);
	asmMOVxx(X86_MOVZX, (NativeReg)reg, reg);
	asmALU(X86_SUB, (NativeReg)reg, 1);

	vec_raise_saturate();

	asmResolveFixup(skip1);
}

static inline void vec_getByte(NativeReg8 dest, int src, NativeReg r, int i)
{
	modrm_o modrm;

	ASSERT_FLUSHED(src);

	asmALU(X86_MOV, dest, x86_mem2(modrm, r, &(gCPU.vr[src].b[i])));
}

static inline void vec_setByte(int dest, NativeReg r, int i, NativeReg8 src)
{
	modrm_o modrm;

	ASSERT_FLUSHED(dest);

	asmALU(X86_MOV, x86_mem2(modrm, r, &(gCPU.vr[dest].b[i])), src);
}

static inline void vec_getByteZ(NativeReg dest, int src, int i)
{
	modrm_o modrm;

	ASSERT_FLUSHED(src);

	asmMOVxx_B(X86_MOVZX, dest, x86_mem2(modrm, &(gCPU.vr[src].b[i])));
}

static inline void vec_getByteS(NativeReg dest, int src, int i)
{
	modrm_o modrm;

	ASSERT_FLUSHED(src);

	asmMOVxx_B(X86_MOVSX, dest, x86_mem2(modrm, &(gCPU.vr[src].b[i])));
}

static inline void vec_getHalf(NativeReg16 dest, int src, int i)
{
	ASSERT_FLUSHED(src);

	asmMOV(dest, &(gCPU.vr[src].b[i]));
}

static inline void vec_setHalf(int dest, int i, NativeReg16 src)
{
	ASSERT_FLUSHED(dest);

	asmMOV(&(gCPU.vr[dest].b[i]), src);
}

static inline void vec_getHalfZ(NativeReg dest, int src, int i)
{
	modrm_o modrm;

	ASSERT_FLUSHED(src);

	asmMOVxx_W(X86_MOVZX, dest, x86_mem2(modrm, &(gCPU.vr[src].b[i])));
}

static inline void vec_getHalfS(NativeReg dest, int src, int i)
{
	modrm_o modrm;

	ASSERT_FLUSHED(src);

	asmMOVxx_W(X86_MOVSX, dest, x86_mem2(modrm, &(gCPU.vr[src].b[i])));
}

static inline void vec_getWord(NativeReg dest, int src, int i)
{
	ASSERT_FLUSHED(src);

	asmMOV(dest, &(gCPU.vr[src].b[i]));
}

static inline void vec_setWord(int dest, int i, NativeReg src)
{
	ASSERT_FLUSHED(dest);

	asmMOV(&(gCPU.vr[dest].b[i]), src);
}

#define SSE_AVAIL	gJITC.hostCPUCaps.sse
//#define SSE_AVAIL	0

#define SSE2_AVAIL	gJITC.hostCPUCaps.sse2
//#define SSE2_AVAIL	0

#define SSE_NO		0
#define SSE2_NO		0

const static sint32 sint32_max = 2147483647;
const static sint32 sint32_min = -2147483648;
const static double uint32_max = 4294967295;
const static uint32 uint32_min = 0;

static inline void vec_Copy(int dest, int src)
{
	if (dest == src);

	else if (SSE_AVAIL) {
		NativeVectorReg reg = jitcMapClientVectorRegisterDirty(dest);
		NativeVectorReg reg2 = jitcGetClientVectorRegisterMapping(src);

		if (reg2 == VECTREG_NO)
			asmMOVAPS(reg, &gCPU.vr[src]);
		else
			asmALUPS(X86_MOVAPS, reg, reg2);
	} else {
		jitcFlushClientVectorRegister(src);
		jitcDropClientVectorRegister(dest);

		NativeReg reg = jitcAllocRegister();

		for (int i=0; i<16; i+=4) {
			vec_getWord(reg, src, i);
			vec_setWord(dest, i, reg);
		}
	}
}

static inline void vec_Zero(int dest)
{
	if (SSE_AVAIL) {
		NativeVectorReg reg = jitcMapClientVectorRegisterDirty(dest);

		asmALUPS(X86_XORPS, reg, reg);
	} else {
		jitcDropClientVectorRegister(dest);

		NativeReg reg = jitcAllocRegister();

		asmMOV_NoFlags(reg, 0);	// avoid flag clobber

		vec_setWord(dest, 0, reg);
		vec_setWord(dest, 4, reg);
		vec_setWord(dest, 8, reg);
		vec_setWord(dest,12, reg);
	}
}

static inline void vec_Neg1(int dest)
{
	if (SSE_AVAIL) {
		NativeVectorReg reg = jitcMapClientVectorRegisterDirty(dest);
		NativeVectorReg n1 = jitcGetClientVectorRegister(vrNEG1);

		asmALUPS(X86_MOVAPS, reg, n1);
	} else {
		jitcDropClientVectorRegister(dest);

		NativeReg reg = jitcAllocRegister();

		asmMOV_NoFlags(reg, 0xffffffff);	// avoid flag clobber

		vec_setWord(dest, 0, reg);
		vec_setWord(dest, 4, reg);
		vec_setWord(dest, 8, reg);
		vec_setWord(dest,12, reg);
	}
}

static inline void vec_Not(int dest, int src)
{
	if (SSE_AVAIL) {
		NativeVectorReg reg1;
		NativeVectorReg reg2;

		if (src == dest) {
			reg1 = jitcGetClientVectorRegisterDirty(dest);
		} else  {
			reg1 = jitcMapClientVectorRegisterDirty(dest);
			reg2 = jitcGetClientVectorRegisterMapping(src);

			if (reg2 == VECTREG_NO)	
				asmMOVAPS(reg1, &gCPU.vr[src]);
			else
				asmALUPS(X86_MOVAPS, reg1, reg2);
		}

		reg2 = jitcGetClientVectorRegister(vrNEG1);

		asmALUPS(X86_XORPS, reg1, reg2);
	} else {
		jitcFlushClientVectorRegister(src);
		jitcDropClientVectorRegister(dest);

		// according to documentation NOT does not effect flags!
		NativeReg reg = jitcAllocRegister();

		for (int i=0; i<16; i+=4) {
			vec_getWord(reg, src, i);
			asmALU(X86_NOT, reg);
			vec_setWord(dest, i, reg);
		}
	}
}

static inline void vec_SelectiveLoadByte(X86FlagTest flags, NativeReg8 reg1, NativeReg reg2, int vrA, int vrB)
{
	modrm_o modrm;

	NativeAddress skip = asmJxxFixup(flags);
	asmALU(X86_MOV, (NativeReg)reg1,
		x86_mem2(modrm, reg2, &(gCPU.vr[vrA].b[16])));

	NativeAddress end = asmJMPFixup();

	asmResolveFixup(skip);
	asmALU(X86_MOV, (NativeReg)reg1,
		x86_mem2(modrm, reg2, &(gCPU.vr[vrB].b[16])));

	asmResolveFixup(end);
}

static inline void vec_PermuteQuad(NativeReg8 reg1, NativeReg reg2, NativeReg8 regT, NativeReg regA, NativeReg regB, int vrA, int vrB, int vrD, int i)
{
	modrm_o modrm;

	asmMOVxx(X86_MOVSX, regA, reg1);
	asmMOVxx(X86_MOVSX, regB, RL2RH(reg1));

	if (gJITC.hostCPUCaps.cmov) {
		asmALU(X86_TEST, reg2, 0x10);
		asmCMOV(X86_Z, (NativeReg)regT,
			x86_mem2(modrm, regA, &(gCPU.vr[vrA].b[16])));
		asmCMOV(X86_NZ, (NativeReg)regT,
			x86_mem2(modrm, regA, &(gCPU.vr[vrB].b[16])));

		vec_setByte(vrD, REG_NO, i, regT);

		asmALU(X86_TEST, reg2, 0x1000);
		asmCMOV(X86_Z, (NativeReg)regT,
			x86_mem2(modrm, regB, &(gCPU.vr[vrA].b[16])));
		asmCMOV(X86_NZ, (NativeReg)regT,
			x86_mem2(modrm, regB, &(gCPU.vr[vrB].b[16])));

		vec_setByte(vrD, REG_NO, i+1, regT);

		asmShift(X86_SHR, (NativeReg)reg1, 16);

		asmMOVxx(X86_MOVSX, regA, reg1);
		asmMOVxx(X86_MOVSX, regB, RL2RH(reg1));

		asmALU(X86_TEST, reg2, 0x100000);
		asmCMOV(X86_Z, (NativeReg)regT,
			x86_mem2(modrm, regA, &(gCPU.vr[vrA].b[16])));
		asmCMOV(X86_NZ, (NativeReg)regT,
			x86_mem2(modrm, regA, &(gCPU.vr[vrB].b[16])));

		vec_setByte(vrD, REG_NO, i+2, regT);

		asmALU(X86_TEST, reg2, 0x10000000);
		asmCMOV(X86_Z, (NativeReg)regT,
			x86_mem2(modrm, regB, &(gCPU.vr[vrA].b[16])));
		asmCMOV(X86_NZ, (NativeReg)regT,
			x86_mem2(modrm, regB, &(gCPU.vr[vrB].b[16])));

		vec_setByte(vrD, REG_NO, i+3, regT);
	} else {
		asmALU(X86_TEST, reg2, 0x10);
		vec_SelectiveLoadByte(X86_NZ, regT, regA, vrA, vrB);
		vec_setByte(vrD, REG_NO, i, regT);

		asmALU(X86_TEST, reg2, 0x1000);
		vec_SelectiveLoadByte(X86_NZ, regT, regB, vrA, vrB);
		vec_setByte(vrD, REG_NO, i+1, regT);

		asmShift(X86_SHR, (NativeReg)reg1, 16);

		asmMOVxx(X86_MOVSX, regA, reg1);
		asmMOVxx(X86_MOVSX, regB, RL2RH(reg1));

		asmALU(X86_TEST, reg2, 0x100000);
		vec_SelectiveLoadByte(X86_NZ, regT, regA, vrA, vrB);
		vec_setByte(vrD, REG_NO, i+2, regT);

		asmALU(X86_TEST, reg2, 0x10000000);
		vec_SelectiveLoadByte(X86_NZ, regT, regB, vrA, vrB);
		vec_setByte(vrD, REG_NO, i+3, regT);
	}
}

/*	vperm		Vector Permutation
 *	v.218
 */
void ppc_opc_vperm()
{
	VECTOR_DEBUG_COMMON;
	int vrD, vrA, vrB, vrC;
	int sel;
	Vector_t r;
	PPC_OPC_TEMPL_A(gCPU.current_opc, vrD, vrA, vrB, vrC);
	for (int i=0; i<16; i++) {
		sel = gCPU.vr[vrC].b[i];
		if (sel & 0x10)
			r.b[i] = VECT_B(gCPU.vr[vrB], sel & 0xf);
		else
			r.b[i] = VECT_B(gCPU.vr[vrA], sel & 0xf);
	}

	gCPU.vr[vrD] = r;
}
JITCFlow ppc_opc_gen_vperm()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vperm);
	return flowEndBlock;
#else
	int vrD, vrA, vrB, vrC, vrTMP;
	PPC_OPC_TEMPL_A(gJITC.current_opc, vrD, vrA, vrB, vrC);

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcFlushClientVectorRegister(vrC);

	jitcClobberCarryAndFlags();
	NativeReg8 reg1 = (NativeReg8)jitcAllocRegister(NATIVE_REG_8);
	NativeReg8 regT = (NativeReg8)jitcAllocRegister(NATIVE_REG_8);
	NativeReg reg2 = jitcAllocRegister();
	NativeReg regA = jitcAllocRegister();
	NativeReg regB = jitcAllocRegister();

	vrTMP = ((vrD == vrA) || (vrD == vrB)) ? vrT : vrD;
	jitcDropClientVectorRegister(vrTMP);

	for (int i=0; i<16; i+=4) {
		vec_getWord((NativeReg)reg1, vrC, i);

		asmALU(X86_MOV, reg2, (NativeReg)reg1);

		asmALU(X86_AND, (NativeReg)reg1, 0x0f0f0f0f);
		asmALU(X86_NOT, (NativeReg)reg1);

		// reg1 = original map (selector mask)
		// reg2 = negative offsets - 1
		// regT = spare use register (must be 8-bit)
		// regA = spare use register
		// regB = spare use register
		vec_PermuteQuad(reg1, reg2, regT, regA, regB, vrA, vrB, vrTMP, i);
	}

	if (vrTMP == vrT) {
		if (SSE_AVAIL) {
			vec_Copy(vrD, vrT);
		} else {
			/* Since we already have a number of registers
			 *   allocated, this is much faster than the non-SSE
			 *   vec_Copy()
			 */
			jitcDropClientVectorRegister(vrD);

			vec_getWord((NativeReg)reg1, vrT, 0);
			vec_getWord(reg2, vrT, 4);
			vec_getWord(regA, vrT, 8);
			vec_getWord(regB, vrT,12);

			vec_setWord(vrD, 0, (NativeReg)reg1);
			vec_setWord(vrD, 4, reg2);
			vec_setWord(vrD, 8, regA);
			vec_setWord(vrD,12, regB);
		}
	}

	return flowContinue;
#endif
}

/*	vsel		Vector Select
 *	v.238
 */
void ppc_opc_vsel()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB, vrC;
	uint64 mask, val;
	PPC_OPC_TEMPL_A(gCPU.current_opc, vrD, vrA, vrB, vrC);

	mask = gCPU.vr[vrC].d[0];
	val = gCPU.vr[vrB].d[0] & mask;
	val |= gCPU.vr[vrA].d[0] & ~mask;
	gCPU.vr[vrD].d[0] = val;

	mask = gCPU.vr[vrC].d[1];
	val = gCPU.vr[vrB].d[1] & mask;
	val |= gCPU.vr[vrA].d[1] & ~mask;
	gCPU.vr[vrD].d[1] = val;
}
JITCFlow ppc_opc_gen_vsel()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vsel);
	return flowEndBlock;
#else
	int vrD, vrA, vrB, vrC;
	PPC_OPC_TEMPL_A(gJITC.current_opc, vrD, vrA, vrB, vrC);

	if (vrA == vrB) {
		vec_Copy(vrD, vrB);
		return flowContinue;
	}

	if (SSE_AVAIL) {
		modrm_o modrm;

		/* We cannot use jitcMapClientVectorRegisterDirty(vrD) here,
		 *	because if vrD == vrC
		 */
		NativeVectorReg d = jitcAllocVectorRegister();
		NativeVectorReg c;

		if (vrA == vrC) {
			c = jitcGetClientVectorRegisterMapping(vrC);

			if (c == VECTREG_NO) {
				asmMOVAPS(d, &gCPU.vr[vrC]);
			} else {
				asmALUPS(X86_MOVAPS, d, c);
			}
		} else {
			c = jitcGetClientVectorRegister(vrC);
			jitcTrashVectorRegister(NATIVE_REG | c);

			asmALUPS(X86_MOVAPS, d, c);
		}

		if (vrB != vrC) {
			NativeVectorReg b = jitcGetClientVectorRegisterMapping(vrB);

			if (b == VECTREG_NO) {
				asmALUPS(X86_ANDPS, d,
					x86_mem2(modrm, &gCPU.vr[vrB]));
			} else
				asmALUPS(X86_ANDPS, d, b);
		}

		if (vrA != vrC) {
			NativeVectorReg a = jitcGetClientVectorRegisterMapping(vrA);

			if (a == VECTREG_NO) {
				asmALUPS(X86_ANDNPS, c,
					x86_mem2(modrm, &gCPU.vr[vrA]));
			} else
				asmALUPS(X86_ANDNPS, c, a);

			asmALUPS(X86_ORPS, d, c);
		}

		jitcRenameVectorRegisterDirty(d, vrD);

		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcFlushClientVectorRegister(vrC);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg1 = jitcAllocRegister();
	NativeReg reg2 = jitcAllocRegister();
	modrm_o modrm;

	for (int i=0; i<16; i+=4) {
		vec_getWord(reg1, vrC, i);
		asmALU(X86_MOV, reg2, reg1);
		asmALU(X86_NOT, reg2);

		asmALU(X86_AND, reg1, x86_mem2(modrm, &(gCPU.vr[vrB].b[i])));

		asmALU(X86_AND, reg2, x86_mem2(modrm, &(gCPU.vr[vrA].b[i])));

		asmALU(X86_OR, reg1, reg2);
		vec_setWord(vrD, i, reg1);
	}

	return flowContinue;
#endif
}

/*	vsrb		Vector Shift Right Byte
 *	v.256
 */
void ppc_opc_vsrb()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);
	for (int i=0; i<16; i++) {
		gCPU.vr[vrD].b[i] = gCPU.vr[vrA].b[i] >> (gCPU.vr[vrB].b[i] & 0x7);
	}
}
JITCFlow ppc_opc_gen_vsrb()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vsrb);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	jitcAllocRegister(NATIVE_REG | ECX);
	NativeReg8 reg = (NativeReg8)jitcAllocRegister(NATIVE_REG_8);

	for (int i=0; i<16; i++) {
		vec_getByte(reg, vrA, REG_NO, i);
		vec_getByte(CL, vrB, REG_NO, i);

		asmALU(X86_AND, CL, 0x7);
		asmShift_CL(X86_SHR, reg);

		vec_setByte(vrD, REG_NO, i, reg);
	}

	return flowContinue;
#endif
}

/*	vsrh		Vector Shift Right Half Word
 *	v.257
 */
void ppc_opc_vsrh()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);
	for (int i=0; i<8; i++) {
		gCPU.vr[vrD].h[i] = gCPU.vr[vrA].h[i] >> (gCPU.vr[vrB].h[i] & 0xf);
	}
}
JITCFlow ppc_opc_gen_vsrh()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vsrh);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	jitcAllocRegister(NATIVE_REG | ECX);
	NativeReg16 reg = (NativeReg16)jitcAllocRegister();

	for (int i=0; i<16; i+=2) {
		vec_getHalf(reg, vrA, i);
		vec_getHalf(CX, vrB, i);

		asmALU(X86_AND, CL, 0x0f);
		asmShift_CL(X86_SHR, reg);

		vec_setHalf(vrD, i, reg);
	}

	return flowContinue;
#endif
}

/*	vsrw		Vector Shift Right Word
 *	v.259
 */
void ppc_opc_vsrw()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);
	for (int i=0; i<4; i++) {
		gCPU.vr[vrD].w[i] = gCPU.vr[vrA].w[i] >> (gCPU.vr[vrB].w[i] & 0x1f);
	}
}
JITCFlow ppc_opc_gen_vsrw()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vsrw);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	jitcAllocRegister(NATIVE_REG | ECX);
	NativeReg reg = jitcAllocRegister();

	for (int i=0; i<16; i+=4) {
		vec_getWord(reg, vrA, i);
		vec_getWord(ECX, vrB, i);

		// this instruction auto-masks to 0x1f
		asmShift_CL(X86_SHR, reg);

		vec_setWord(vrD, i, reg);
	}

	return flowContinue;
#endif
}

/*	vsrab		Vector Shift Right Arithmetic Byte
 *	v.253
 */
void ppc_opc_vsrab()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);
	for (int i=0; i<16; i++) {
		gCPU.vr[vrD].sb[i] = gCPU.vr[vrA].sb[i] >> (gCPU.vr[vrB].b[i] & 0x7);
	}
}
JITCFlow ppc_opc_gen_vsrab()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vsrab);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	jitcAllocRegister(NATIVE_REG | ECX);
	NativeReg8 reg = (NativeReg8)jitcAllocRegister(NATIVE_REG_8);

	for (int i=0; i<16; i++) {
		vec_getByte(reg, vrA, REG_NO, i);
		vec_getByte(CL, vrB, REG_NO, i);

		asmALU(X86_AND, CL, 0x7);
		asmShift_CL(X86_SAR, reg);

		vec_setByte(vrD, REG_NO, i, reg);
	}

	return flowContinue;
#endif
}

/*	vsrah		Vector Shift Right Arithmetic Half Word
 *	v.254
 */
void ppc_opc_vsrah()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);
	for (int i=0; i<8; i++) {
		gCPU.vr[vrD].sh[i] = gCPU.vr[vrA].sh[i] >> (gCPU.vr[vrB].h[i] & 0xf);
	}
}
JITCFlow ppc_opc_gen_vsrah()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vsrah);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	jitcAllocRegister(NATIVE_REG | ECX);
	NativeReg16 reg = (NativeReg16)jitcAllocRegister();

	for (int i=0; i<16; i+=2) {
		vec_getHalf(reg, vrA, i);
		vec_getHalf(CX, vrB, i);

		asmALU(X86_AND, CL, 0x0f);
		asmShift_CL(X86_SAR, reg);

		vec_setHalf(vrD, i, reg);
	}

	return flowContinue;
#endif
}

/*	vsraw		Vector Shift Right Arithmetic Word
 *	v.255
 */
void ppc_opc_vsraw()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);
	for (int i=0; i<4; i++) {
		gCPU.vr[vrD].sw[i] = gCPU.vr[vrA].sw[i] >> (gCPU.vr[vrB].w[i] & 0x1f);
	}
}
JITCFlow ppc_opc_gen_vsraw()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vsraw);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	jitcAllocRegister(NATIVE_REG | ECX);
	NativeReg reg = jitcAllocRegister();

	for (int i=0; i<16; i+=4) {
		vec_getWord(reg, vrA, i);
		vec_getWord(ECX, vrB, i);

		// this instruction auto-masks to 0x1f
		asmShift_CL(X86_SAR, reg);

		vec_setWord(vrD, i, reg);
	}

	return flowContinue;
#endif
}

/*	vslb		Vector Shift Left Byte
 *	v.240
 */
void ppc_opc_vslb()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);
	for (int i=0; i<16; i++) {
		gCPU.vr[vrD].b[i] = gCPU.vr[vrA].b[i] << (gCPU.vr[vrB].b[i] & 0x7);
	}
}
JITCFlow ppc_opc_gen_vslb()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vslb);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	jitcAllocRegister(NATIVE_REG | ECX);
	NativeReg8 reg = (NativeReg8)jitcAllocRegister(NATIVE_REG_8);

	for (int i=0; i<16; i++) {
		vec_getByte(reg, vrA, REG_NO, i);
		vec_getByte(CL, vrB, REG_NO, i);

		asmALU(X86_AND, CL, 0x7);
		asmShift_CL(X86_SHL, reg);

		vec_setByte(vrD, REG_NO, i, reg);
	}

	return flowContinue;
#endif
}

/*	vslh		Vector Shift Left Half Word
 *	v.242
 */
void ppc_opc_vslh()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);
	for (int i=0; i<8; i++) {
		gCPU.vr[vrD].h[i] = gCPU.vr[vrA].h[i] << (gCPU.vr[vrB].h[i] & 0xf);
	}
}
JITCFlow ppc_opc_gen_vslh()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vslh);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	jitcAllocRegister(NATIVE_REG | ECX);
	NativeReg16 reg = (NativeReg16)jitcAllocRegister();

	for (int i=0; i<16; i+=2) {
		vec_getHalf(reg, vrA, i);
		vec_getHalf(CX, vrB, i);

		asmALU(X86_AND, CL, 0x0f);
		asmShift_CL(X86_SHL, reg);

		vec_setHalf(vrD, i, reg);
	}

	return flowContinue;
#endif
}

/*	vslw		Vector Shift Left Word
 *	v.244
 */
void ppc_opc_vslw()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);
	for (int i=0; i<4; i++) {
		gCPU.vr[vrD].w[i] = gCPU.vr[vrA].w[i] << (gCPU.vr[vrB].w[i] & 0x1f);
	}
}
JITCFlow ppc_opc_gen_vslw()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vslw);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	jitcAllocRegister(NATIVE_REG | ECX);
	NativeReg reg = jitcAllocRegister();

	for (int i=0; i<16; i+=4) {
		vec_getWord(reg, vrA, i);
		vec_getWord(ECX, vrB, i);

		// this instruction auto-masks to 0x1f
		asmShift_CL(X86_SHL, reg);

		vec_setWord(vrD, i, reg);
	}

	return flowContinue;
#endif
}

/*	vsr		Vector Shift Right
 *	v.251
 */
void ppc_opc_vsr()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	int shift;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	/* Specs say that the low-order 3 bits of all byte elements in vB
	 *   must be the same, or the result is undefined.  So we can just
	 *   use the same low-order 3 bits for all of our shifts.
	 */
	shift = gCPU.vr[vrB].w[0] & 0x7;

	r.d[0] = gCPU.vr[vrA].d[0] >> shift;
	r.d[1] = gCPU.vr[vrA].d[1] >> shift;

	VECT_D(r, 1) |= VECT_D(gCPU.vr[vrA], 0) << (64 - shift);

	gCPU.vr[vrD] = r;
}
JITCFlow ppc_opc_gen_vsr()
{
	ppc_opc_gen_interpret(ppc_opc_vsr);
	return flowEndBlock;
}

/*	vsro		Vector Shift Right Octet
 *	v.258
 */
void ppc_opc_vsro()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	int shift, i;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	shift = (gCPU.vr[vrB].w[0] >> 3) & 0xf;
#if HOST_ENDIANESS == HOST_ENDIANESS_LE
	for (i=0; i<(16-shift); i++) {
		r.b[i] = gCPU.vr[vrA].b[i+shift];
	}

	for (; i<16; i++) {
		r.b[i] = 0;
	}
#elif HOST_ENDIANESS == HOST_ENDIANESS_BE
	for (i=0; i<shift; i++) {
		r.b[i] = 0;
	}

	for (; i<16; i++) {
		r.b[i] = gCPU.vr[vrA].b[i-shift];
	}
#else
#error Endianess not supported!
#endif

	gCPU.vr[vrD] = r;
}
JITCFlow ppc_opc_gen_vsro()
{
	ppc_opc_gen_interpret(ppc_opc_vsro);
	return flowEndBlock;
}

/*	vsl		Vector Shift Left
 *	v.239
 */
void ppc_opc_vsl()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	int shift;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	/* Specs say that the low-order 3 bits of all byte elements in vB
	 *   must be the same, or the result is undefined.  So we can just
	 *   use the same low-order 3 bits for all of our shifts.
	 */
	shift = gCPU.vr[vrB].w[0] & 0x7;

	r.d[0] = gCPU.vr[vrA].d[0] << shift;
	r.d[1] = gCPU.vr[vrA].d[1] << shift;

	VECT_D(r, 0) |= VECT_D(gCPU.vr[vrA], 1) >> (64 - shift);

	gCPU.vr[vrD] = r;
}
JITCFlow ppc_opc_gen_vsl()
{
	ppc_opc_gen_interpret(ppc_opc_vsl);
	return flowEndBlock;
}

/*	vslo		Vector Shift Left Octet
 *	v.243
 */
void ppc_opc_vslo()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	int shift, i;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	shift = (gCPU.vr[vrB].w[0] >> 3) & 0xf;
#if HOST_ENDIANESS == HOST_ENDIANESS_LE
	for (i=0; i<shift; i++) {
		r.b[i] = 0;
	}

	for (; i<16; i++) {
		r.b[i] = gCPU.vr[vrA].b[i-shift];
	}
#elif HOST_ENDIANESS == HOST_ENDIANESS_BE
	for (i=0; i<(16-shift); i++) {
		r.b[i] = gCPU.vr[vrA].b[i+shift];
	}

	for (; i<16; i++) {
		r.b[i] = 0;
	}
#else
#error Endianess not supported!
#endif

	gCPU.vr[vrD] = r;
}
JITCFlow ppc_opc_gen_vslo()
{
	ppc_opc_gen_interpret(ppc_opc_vslo);
	return flowEndBlock;
}

static inline void ppc_opc_gen_helper_vsldoi_left(int vrA, int vrD, int shift, int pshift, NativeReg reg)
{
	for (int i=12; i>=shift; i-=4) {
		vec_getWord(reg, vrA, i-shift);

		vec_setWord(vrD, i, reg);
	}

	if (pshift) {
		for (int i=shift+4-pshift; i>=shift; i--) {
			vec_getByte((NativeReg8)reg, vrA, REG_NO, i-shift);

			vec_setByte(vrD, REG_NO, i, (NativeReg8)reg);
		}
	}
}

static inline void ppc_opc_gen_helper_vsldoi_right(int vrB, int vrD, int shift, int ashift, int bshift, NativeReg reg)
{
	for (int i=0; i<bshift; i+=4) {
		vec_getWord(reg, vrB, i+ashift);

		vec_setWord(vrD, i, reg);
	}

	for (int i=bshift; i<shift; i++) {
		vec_getByte((NativeReg8)reg, vrB, REG_NO, i+ashift);

		vec_setByte(vrD, REG_NO, i, (NativeReg8)reg);
	}
}

/*	vsldoi		Vector Shift Left Double by Octet Immediate
 *	v.241
 */
void ppc_opc_vsldoi()
{
	VECTOR_DEBUG_COMMON;
	int vrD, vrA, vrB, shift, ashift;
	int i;
	Vector_t r;
	PPC_OPC_TEMPL_A(gCPU.current_opc, vrD, vrA, vrB, shift);

	shift &= 0xf;
	ashift = 16 - shift;

#if HOST_ENDIANESS == HOST_ENDIANESS_LE
	for (i=0; i<shift; i++) {
		r.b[i] = gCPU.vr[vrB].b[i+ashift];
	}

	for (; i<16; i++) {
		r.b[i] = gCPU.vr[vrA].b[i-shift];
	}
#elif HOST_ENDIANESS == HOST_ENDIANESS_BE
	for (i=0; i<ashift; i++) {
		r.b[i] = gCPU.vr[vrA].b[i+shift];
	}

	for (; i<16; i++) {
		r.b[i] = gCPU.vr[vrB].b[i-ashift];
	}
#else
#error Endianess not supported!
#endif

	gCPU.vr[vrD] = r;
}
JITCFlow ppc_opc_gen_vsldoi()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vsldoi);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	int shift, ashift, pshift, bshift;
	PPC_OPC_TEMPL_A(gJITC.current_opc, vrD, vrA, vrB, shift);

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	shift &= 0xf;
	ashift = 16 - shift;
	bshift = (shift & ~0x03);
	pshift = shift & 0x03;

	NativeReg reg = jitcAllocRegister(NATIVE_REG_8);

	if ((vrD == vrA) && (vrD != vrB)) {
		ppc_opc_gen_helper_vsldoi_left(vrA, vrD, shift, pshift, reg);

		ppc_opc_gen_helper_vsldoi_right(vrB, vrD, shift, ashift, bshift, reg);

		return flowContinue;
	}

	if (vrD == vrA) {
		jitcDropClientVectorRegister(vrT);
		ppc_opc_gen_helper_vsldoi_right(vrB, vrT, shift, ashift, bshift, reg);
	} else {
		ppc_opc_gen_helper_vsldoi_right(vrB, vrD, shift, ashift, bshift, reg);
	}

	ppc_opc_gen_helper_vsldoi_left(vrA, vrD, shift, pshift, reg);

	if (vrD == vrA) {
		for (int i=0; i<bshift; i+=4) {
			vec_getWord(reg, vrT, i);
			vec_setWord(vrD, i, reg);
		}

		for (int i=bshift; i<shift; i++) {
			vec_getByte((NativeReg8)reg, vrT, REG_NO, i);
			vec_setByte(vrD, REG_NO, i, (NativeReg8)reg);
		}
	}

	return flowContinue;
#endif
}

/*	vrlb		Vector Rotate Left Byte
 *	v.234
 */
void ppc_opc_vrlb()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB, shift;
	Vector_t r;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		shift = (gCPU.vr[vrB].b[i] & 0x7);

		r.b[i] = gCPU.vr[vrA].b[i] << shift;
		r.b[i] |= gCPU.vr[vrA].b[i] >> (8 - shift);
	}

	gCPU.vr[vrD] = r;
}
JITCFlow ppc_opc_gen_vrlb()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vrlb);
	return flowEndBlock;
#else

	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	jitcAllocRegister(NATIVE_REG | ECX);
	NativeReg8 reg = (NativeReg8)jitcAllocRegister(NATIVE_REG_8);

	for (int i=0; i<16; i++) {
		vec_getByte(reg, vrA, REG_NO, i);
		vec_getByte(CL, vrB, REG_NO, i);

		asmShift_CL(X86_ROL, reg);

		vec_setByte(vrD, REG_NO, i, reg);
	}

	return flowContinue;
#endif
}

/*	vrlh		Vector Rotate Left Half Word
 *	v.235
 */
void ppc_opc_vrlh()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB, shift;
	Vector_t r;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		shift = (gCPU.vr[vrB].h[i] & 0xf);

		r.h[i] = gCPU.vr[vrA].h[i] << shift;
		r.h[i] |= gCPU.vr[vrA].h[i] >> (16 - shift);
	}

	gCPU.vr[vrD] = r;
}
JITCFlow ppc_opc_gen_vrlh()
{
	ppc_opc_gen_interpret(ppc_opc_vrlh);
	return flowEndBlock;
}

/*	vrlw		Vector Rotate Left Word
 *	v.236
 */
void ppc_opc_vrlw()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB, shift;
	Vector_t r;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		shift = (gCPU.vr[vrB].w[i] & 0x1F);

		r.w[i] = gCPU.vr[vrA].w[i] << shift;
		r.w[i] |= gCPU.vr[vrA].w[i] >> (32 - shift);
	}

	gCPU.vr[vrD] = r;
}
JITCFlow ppc_opc_gen_vrlw()
{
	ppc_opc_gen_interpret(ppc_opc_vrlw);
	return flowEndBlock;
}

/* With the merges, I just don't see any point in risking that a compiler
 *   might generate actual alu code to calculate anything when it's
 *   compile-time known.  Plus, it's easier to validate it like this.
 */

/*	vmrghb		Vector Merge High Byte
 *	v.195
 */
void ppc_opc_vmrghb()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	VECT_B(r, 0) = VECT_B(gCPU.vr[vrA], 0);
	VECT_B(r, 1) = VECT_B(gCPU.vr[vrB], 0);
	VECT_B(r, 2) = VECT_B(gCPU.vr[vrA], 1);
	VECT_B(r, 3) = VECT_B(gCPU.vr[vrB], 1);
	VECT_B(r, 4) = VECT_B(gCPU.vr[vrA], 2);
	VECT_B(r, 5) = VECT_B(gCPU.vr[vrB], 2);
	VECT_B(r, 6) = VECT_B(gCPU.vr[vrA], 3);
	VECT_B(r, 7) = VECT_B(gCPU.vr[vrB], 3);
	VECT_B(r, 8) = VECT_B(gCPU.vr[vrA], 4);
	VECT_B(r, 9) = VECT_B(gCPU.vr[vrB], 4);
	VECT_B(r,10) = VECT_B(gCPU.vr[vrA], 5);
	VECT_B(r,11) = VECT_B(gCPU.vr[vrB], 5);
	VECT_B(r,12) = VECT_B(gCPU.vr[vrA], 6);
	VECT_B(r,13) = VECT_B(gCPU.vr[vrB], 6);
	VECT_B(r,14) = VECT_B(gCPU.vr[vrA], 7);
	VECT_B(r,15) = VECT_B(gCPU.vr[vrB], 7);

	gCPU.vr[vrD] = r;
}
JITCFlow ppc_opc_gen_vmrghb()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vmrghb);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (SSE2_AVAIL) {
		noncommutative_operation(PALUB(X86_PUNPCKH), vrD, vrB, vrA);
		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	NativeReg8 reg1 = (NativeReg8)jitcAllocRegister(NATIVE_REG_8);
	NativeReg8 reg2 = (NativeReg8)jitcAllocRegister(NATIVE_REG_8);

	vec_getHalf((NativeReg16)reg1, vrB, 8);
	vec_getHalf((NativeReg16)reg2, vrA, 8);
	asmALU(X86_XCHG, RL2RH(reg1), reg2);
	vec_setHalf(vrD, 0, (NativeReg16)reg1);
	vec_setHalf(vrD, 2, (NativeReg16)reg2);

	vec_getHalf((NativeReg16)reg1, vrB, 10);
	vec_getHalf((NativeReg16)reg2, vrA, 10);
	asmALU(X86_XCHG, RL2RH(reg1), reg2);
	vec_setHalf(vrD, 4, (NativeReg16)reg1);
	vec_setHalf(vrD, 6, (NativeReg16)reg2);

	vec_getHalf((NativeReg16)reg1, vrB, 12);
	vec_getHalf((NativeReg16)reg2, vrA, 12);
	asmALU(X86_XCHG, RL2RH(reg1), reg2);
	vec_setHalf(vrD, 8, (NativeReg16)reg1);
	vec_setHalf(vrD, 10, (NativeReg16)reg2);

	vec_getHalf((NativeReg16)reg1, vrB, 14);
	vec_getHalf((NativeReg16)reg2, vrA, 14);
	asmALU(X86_XCHG, RL2RH(reg1), reg2);
	vec_setHalf(vrD, 12, (NativeReg16)reg1);
	vec_setHalf(vrD, 14, (NativeReg16)reg2);

	return flowContinue;
#endif
}

/*	vmrghh		Vector Merge High Half Word
 *	v.196
 */
void ppc_opc_vmrghh()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	VECT_H(r, 0) = VECT_H(gCPU.vr[vrA], 0);
	VECT_H(r, 1) = VECT_H(gCPU.vr[vrB], 0);
	VECT_H(r, 2) = VECT_H(gCPU.vr[vrA], 1);
	VECT_H(r, 3) = VECT_H(gCPU.vr[vrB], 1);
	VECT_H(r, 4) = VECT_H(gCPU.vr[vrA], 2);
	VECT_H(r, 5) = VECT_H(gCPU.vr[vrB], 2);
	VECT_H(r, 6) = VECT_H(gCPU.vr[vrA], 3);
	VECT_H(r, 7) = VECT_H(gCPU.vr[vrB], 3);

	gCPU.vr[vrD] = r;
}
JITCFlow ppc_opc_gen_vmrghh()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vmrghh);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (SSE2_AVAIL) {
		noncommutative_operation(PALUW(X86_PUNPCKH), vrD, vrB, vrA);
		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	NativeReg16 reg1 = (NativeReg16)jitcAllocRegister();

	vec_getHalf(reg1, vrB, 8);
	vec_setHalf(vrD, 0, reg1);
	vec_getHalf(reg1, vrB, 10);
	vec_setHalf(vrD, 4, reg1);

	vec_getHalf(reg1, vrA, 8);
	vec_setHalf(vrD, 2, reg1);
	vec_getHalf(reg1, vrA, 10);
	vec_setHalf(vrD, 6, reg1);

	vec_getHalf(reg1, vrB, 12);
	vec_setHalf(vrD, 8, reg1);

	vec_getHalf(reg1, vrA, 12);
	vec_setHalf(vrD, 10, reg1);

	vec_getHalf(reg1, vrB, 14);
	vec_setHalf(vrD, 12, reg1);

	if (vrD != vrA) {
		vec_getHalf(reg1, vrA, 14);
		vec_setHalf(vrD, 14, reg1);
	}

	return flowContinue;
#endif
}

/*	vmrghw		Vector Merge High Word
 *	v.197
 */
void ppc_opc_vmrghw()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	VECT_W(r, 0) = VECT_W(gCPU.vr[vrA], 0);
	VECT_W(r, 1) = VECT_W(gCPU.vr[vrB], 0);
	VECT_W(r, 2) = VECT_W(gCPU.vr[vrA], 1);
	VECT_W(r, 3) = VECT_W(gCPU.vr[vrB], 1);

	gCPU.vr[vrD] = r;
}
JITCFlow ppc_opc_gen_vmrghw()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vmrghw);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (SSE2_AVAIL) {
		noncommutative_operation(PALUD(X86_PUNPCKH), vrD, vrB, vrA);
		return flowContinue;
	}

	if (SSE_AVAIL) {
		NativeVectorReg dest, src;
		int shuf_map = 0x72;
		int vrS = vrB;

		if (vrA == vrD) {
			dest = jitcGetClientVectorRegisterDirty(vrA);
			src = jitcGetClientVectorRegisterMapping(vrB);
		} else if (vrB == vrD) {
			dest = jitcGetClientVectorRegisterDirty(vrB);
			src = jitcGetClientVectorRegisterMapping(vrA);
			shuf_map = 0xd8;
			vrS = vrA;
		} else {
			dest = jitcMapClientVectorRegisterDirty(vrD);
			src = jitcGetClientVectorRegisterMapping(vrA);

			if (src == VECTREG_NO)
				asmMOVAPS(dest, &gCPU.vr[vrA]);
			else
				asmALUPS(X86_MOVAPS, dest, src);

			src = jitcGetClientVectorRegisterMapping(vrB);
		}

		if (src == VECTREG_NO) {
			asmSHUFPS(dest, x86_mem2(modrm, &gCPU.vr[vrS]), 0xee);
		} else {
			asmSHUFPS(dest, src, 0xee);
		}

		asmSHUFPS(dest, dest, shuf_map);

		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	NativeReg reg = jitcAllocRegister();
	JitcVectorReg vrTMP = vrD;

	if (vrA == vrD || vrB == vrD) {
		vrTMP = vrT;
	}

	vec_getWord(reg, vrA, 12);
	vec_setWord(vrTMP, 12, reg);

	vec_getWord(reg, vrB, 12);
	vec_setWord(vrTMP, 8, reg);

	vec_getWord(reg, vrA, 8);
	vec_setWord(vrTMP, 4, reg);

	vec_getWord(reg, vrB, 8);
	vec_setWord(vrTMP, 0, reg);

	vec_Copy(vrD, vrTMP);
	
	return flowContinue;
#endif
}

/*	vmrglb		Vector Merge Low Byte
 *	v.198
 */
void ppc_opc_vmrglb()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	VECT_B(r, 0) = VECT_B(gCPU.vr[vrA], 8);
	VECT_B(r, 1) = VECT_B(gCPU.vr[vrB], 8);
	VECT_B(r, 2) = VECT_B(gCPU.vr[vrA], 9);
	VECT_B(r, 3) = VECT_B(gCPU.vr[vrB], 9);
	VECT_B(r, 4) = VECT_B(gCPU.vr[vrA],10);
	VECT_B(r, 5) = VECT_B(gCPU.vr[vrB],10);
	VECT_B(r, 6) = VECT_B(gCPU.vr[vrA],11);
	VECT_B(r, 7) = VECT_B(gCPU.vr[vrB],11);
	VECT_B(r, 8) = VECT_B(gCPU.vr[vrA],12);
	VECT_B(r, 9) = VECT_B(gCPU.vr[vrB],12);
	VECT_B(r,10) = VECT_B(gCPU.vr[vrA],13);
	VECT_B(r,11) = VECT_B(gCPU.vr[vrB],13);
	VECT_B(r,12) = VECT_B(gCPU.vr[vrA],14);
	VECT_B(r,13) = VECT_B(gCPU.vr[vrB],14);
	VECT_B(r,14) = VECT_B(gCPU.vr[vrA],15);
	VECT_B(r,15) = VECT_B(gCPU.vr[vrB],15);

	gCPU.vr[vrD] = r;
}
JITCFlow ppc_opc_gen_vmrglb()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vmrglb);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (SSE2_AVAIL) {
		noncommutative_operation(PALUB(X86_PUNPCKL), vrD, vrB, vrA);
		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	NativeReg8 reg1 = (NativeReg8)jitcAllocRegister(NATIVE_REG_8);
	NativeReg8 reg2 = (NativeReg8)jitcAllocRegister(NATIVE_REG_8);

	vec_getHalf((NativeReg16)reg1, vrB, 6);
	vec_getHalf((NativeReg16)reg2, vrA, 6);
	asmALU(X86_XCHG, RL2RH(reg1), reg2);
	vec_setHalf(vrD, 12, (NativeReg16)reg1);
	vec_setHalf(vrD, 14, (NativeReg16)reg2);

	vec_getHalf((NativeReg16)reg1, vrB, 4);
	vec_getHalf((NativeReg16)reg2, vrA, 4);
	asmALU(X86_XCHG, RL2RH(reg1), reg2);
	vec_setHalf(vrD, 8, (NativeReg16)reg1);
	vec_setHalf(vrD, 10, (NativeReg16)reg2);

	vec_getHalf((NativeReg16)reg1, vrB, 2);
	vec_getHalf((NativeReg16)reg2, vrA, 2);
	asmALU(X86_XCHG, RL2RH(reg1), reg2);
	vec_setHalf(vrD, 4, (NativeReg16)reg1);
	vec_setHalf(vrD, 6, (NativeReg16)reg2);

	vec_getHalf((NativeReg16)reg1, vrB, 0);
	vec_getHalf((NativeReg16)reg2, vrA, 0);
	asmALU(X86_XCHG, RL2RH(reg1), reg2);
	vec_setHalf(vrD, 0, (NativeReg16)reg1);
	vec_setHalf(vrD, 2, (NativeReg16)reg2);

	return flowContinue;
#endif
}

/*	vmrglh		Vector Merge Low Half Word
 *	v.199
 */
void ppc_opc_vmrglh()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	VECT_H(r, 0) = VECT_H(gCPU.vr[vrA], 4);
	VECT_H(r, 1) = VECT_H(gCPU.vr[vrB], 4);
	VECT_H(r, 2) = VECT_H(gCPU.vr[vrA], 5);
	VECT_H(r, 3) = VECT_H(gCPU.vr[vrB], 5);
	VECT_H(r, 4) = VECT_H(gCPU.vr[vrA], 6);
	VECT_H(r, 5) = VECT_H(gCPU.vr[vrB], 6);
	VECT_H(r, 6) = VECT_H(gCPU.vr[vrA], 7);
	VECT_H(r, 7) = VECT_H(gCPU.vr[vrB], 7);

	gCPU.vr[vrD] = r;
}
JITCFlow ppc_opc_gen_vmrglh()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vmrglh);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (SSE2_AVAIL) {
		noncommutative_operation(PALUW(X86_PUNPCKL), vrD, vrB, vrA);
		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	NativeReg16 reg1 = (NativeReg16)jitcAllocRegister();

	vec_getHalf(reg1, vrA, 6);
	vec_setHalf(vrD, 14, reg1);
	vec_getHalf(reg1, vrA, 4);
	vec_setHalf(vrD, 10, reg1);

	vec_getHalf(reg1, vrB, 6);
	vec_setHalf(vrD, 12, reg1);
	vec_getHalf(reg1, vrB, 4);
	vec_setHalf(vrD, 8, reg1);

	vec_getHalf(reg1, vrA, 2);
	vec_setHalf(vrD, 6, reg1);

	vec_getHalf(reg1, vrB, 2);
	vec_setHalf(vrD, 4, reg1);

	vec_getHalf(reg1, vrA, 0);
	vec_setHalf(vrD, 2, reg1);

	if (vrD != vrB) {
		vec_getHalf(reg1, vrB, 0);
		vec_setHalf(vrD, 0, reg1);
	}

	return flowContinue;
#endif
}

/*	vmrglw		Vector Merge Low Word
 *	v.200
 */
void ppc_opc_vmrglw()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	VECT_W(r, 0) = VECT_W(gCPU.vr[vrA], 2);
	VECT_W(r, 1) = VECT_W(gCPU.vr[vrB], 2);
	VECT_W(r, 2) = VECT_W(gCPU.vr[vrA], 3);
	VECT_W(r, 3) = VECT_W(gCPU.vr[vrB], 3);

	gCPU.vr[vrD] = r;
}
JITCFlow ppc_opc_gen_vmrglw()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vmrglw);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (SSE2_AVAIL) {
		noncommutative_operation(PALUD(X86_PUNPCKL), vrD, vrB, vrA);
		return flowContinue;
	}

	if (SSE_AVAIL) {
		NativeVectorReg dest, src;
		int shuf_map = 0x72;
		int vrS = vrB;

		if (vrA == vrD) {
			dest = jitcGetClientVectorRegisterDirty(vrA);
			src = jitcGetClientVectorRegisterMapping(vrB);
		} else if (vrB == vrD) {
			dest = jitcGetClientVectorRegisterDirty(vrB);
			src = jitcGetClientVectorRegisterMapping(vrA);
			shuf_map = 0xd8;
			vrS = vrA;
		} else {
			dest = jitcGetClientVectorRegister(vrA);
			jitcRenameVectorRegisterDirty(dest, vrD);

			src = jitcGetClientVectorRegisterMapping(vrB);
		}

		if (src == VECTREG_NO) {
			asmSHUFPS(dest, x86_mem2(modrm, &gCPU.vr[vrS]), 0x44);
		} else {
			asmSHUFPS(dest, src, 0x44);
		}

		asmSHUFPS(dest, dest, shuf_map);

		return flowContinue;
	}

	NativeReg reg = jitcAllocRegister();

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	vec_getWord(reg, vrA, 4);
	vec_setWord(vrD, 12, reg);

	vec_getWord(reg, vrB, 4);
	vec_setWord(vrD,  8, reg);

	vec_getWord(reg, vrA, 0);
	vec_setWord(vrD, 4, reg);

	if (vrB != vrD) {
		vec_getWord(reg, vrB, 0);
		vec_setWord(vrD, 0, reg);
	}

	return flowContinue;
#endif
}

/*	vspltb		Vector Splat Byte
 *	v.245
 */
void ppc_opc_vspltb()
{
	VECTOR_DEBUG;
	int vrD, vrB;
	uint32 uimm;
	uint64 val;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, uimm, vrB);

	/* The documentation doesn't stipulate what a value higher than 0xf
	 *   will do.  Thus, this is by default an undefined value.  We
	 *   are thus doing this the fastest way that won't crash us.
	 */
	val = VECT_B(gCPU.vr[vrB], uimm & 0xf);
	val |= (val << 8);
	val |= (val << 16);
	val |= (val << 32);

	gCPU.vr[vrD].d[0] = val;
	gCPU.vr[vrD].d[1] = val;
}
JITCFlow ppc_opc_gen_vspltb()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vspltb);
	return flowEndBlock;
#else
	int vrD, vrB;
	uint32 uimm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, uimm, vrB);

	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg1 = jitcAllocRegister(NATIVE_REG_8);
	NativeReg reg2 = jitcAllocRegister();

	/* The documentation doesn't stipulate what a value higher than 0xf
	 *   will do.  Thus, this is by default an undefined value.  We
	 *   are thus doing this the fastest way that won't crash us.
	 */
	vec_getByteZ(reg1, vrB, (15 - (uimm & 0xf)));
	asmALU(X86_MOV, RL2RH(reg1), (NativeReg8)reg1);

	asmALU(X86_MOV, reg2, reg1);
	asmShift(X86_SHL, reg2, 16);
	asmALU(X86_OR, reg1, reg2);

	vec_setWord(vrD,  0, reg1);
	vec_setWord(vrD,  4, reg1);
	vec_setWord(vrD,  8, reg1);
	vec_setWord(vrD, 12, reg1);

	return flowContinue;
#endif
}

/*	vsplth		Vector Splat Half Word
 *	v.246
 */
void ppc_opc_vsplth()
{
	VECTOR_DEBUG;
	int vrD, vrB;
	uint32 uimm;
	uint64 val;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, uimm, vrB);

	/* The documentation doesn't stipulate what a value higher than 0x7
	 *   will do.  Thus, this is by default an undefined value.  We
	 *   are thus doing this the fastest way that won't crash us.
	 */
	val = VECT_H(gCPU.vr[vrB], uimm & 0x7);
	val |= (val << 16);
	val |= (val << 32);

	gCPU.vr[vrD].d[0] = val;
	gCPU.vr[vrD].d[1] = val;
}
JITCFlow ppc_opc_gen_vsplth()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vsplth);
	return flowEndBlock;
#else
	int vrD, vrB;
	uint32 uimm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, uimm, vrB);

	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg1 = jitcAllocRegister();
	NativeReg reg2 = jitcAllocRegister();

	/* The documentation doesn't stipulate what a value higher than 0x7
	 *   will do.  Thus, this is by default an undefined value.  We
	 *   are thus doing this the fastest way that won't crash us.
	 */
	vec_getHalfZ(reg1, vrB, (7 - (uimm & 0x7)) << 1);

	asmALU(X86_MOV, reg2, reg1);
	asmShift(X86_SHL, reg2, 16);
	asmALU(X86_OR, reg1, reg2);

	vec_setWord(vrD,  0, reg1);
	vec_setWord(vrD,  4, reg1);
	vec_setWord(vrD,  8, reg1);
	vec_setWord(vrD, 12, reg1);

	return flowContinue;
#endif
}

/*	vspltw		Vector Splat Word
 *	v.250
 */
void ppc_opc_vspltw()
{
	VECTOR_DEBUG;
	int vrD, vrB;
	uint32 uimm;
	uint64 val;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, uimm, vrB);

	/* The documentation doesn't stipulate what a value higher than 0x3
	 *   will do.  Thus, this is by default an undefined value.  We
	 *   are thus doing this the fastest way that won't crash us.
	 */
	val = VECT_W(gCPU.vr[vrB], uimm & 0x3);
	val |= (val << 32);

	gCPU.vr[vrD].d[0] = val;
	gCPU.vr[vrD].d[1] = val;
}
JITCFlow ppc_opc_gen_vspltw()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vspltw);
	return flowEndBlock;
#else
	int vrD, vrB;
	uint32 uimm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, uimm, vrB);

	if (SSE_AVAIL) {
		NativeVectorReg reg;

		if (vrD == vrB) {
			reg = jitcGetClientVectorRegisterDirty(vrD);
		} else {
			reg = jitcMapClientVectorRegisterDirty(vrD);
			NativeVectorReg reg2 = jitcGetClientVectorRegisterMapping(vrB);

			if (reg2 == VECTREG_NO)
				asmMOVAPS(reg, &gCPU.vr[vrB]);
			else
				asmALUPS(X86_MOVAPS, reg, reg2);
		}

		asmSHUFPS(reg, reg, 0xff - ((uimm & 0x3) * 0x55));

		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg1 = jitcAllocRegister();

	/* The documentation doesn't stipulate what a value higher than 0xf
	 *   will do.  Thus, this is by default an undefined value.  We
	 *   are thus doing this the fastest way that won't crash us.
	 */
	vec_getWord(reg1, vrB, (3 - (uimm & 0x3)) << 2);

	vec_setWord(vrD,  0, reg1);
	vec_setWord(vrD,  4, reg1);
	vec_setWord(vrD,  8, reg1);
	vec_setWord(vrD, 12, reg1);

	return flowContinue;
#endif
}

/*	vspltisb	Vector Splat Immediate Signed Byte
 *	v.247
 */
void ppc_opc_vspltisb()
{
	VECTOR_DEBUG_COMMON;
	int vrD, vrB;
	uint32 simm;
	uint64 val;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, simm, vrB);
	PPC_OPC_ASSERT(vrB==0);

	val = (simm & 0x10) ? (simm | 0xE0) : simm;
	val |= (val << 8);
	val |= (val << 16);
	val |= (val << 32);

	gCPU.vr[vrD].d[0] = val;
	gCPU.vr[vrD].d[1] = val;
}
JITCFlow ppc_opc_gen_vspltisb()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vspltisb);
	return flowEndBlock;
#else
	int vrD, vrB;
	uint32 simm, val;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, simm, vrB);

	if (simm == 0) {
		vec_Zero(vrD);
		return flowContinue;
	} else if (simm == 0x1f) {
		vec_Neg1(vrD);
		return flowContinue;
	}

	val = (simm & 0x10) ? (simm | 0xE0) : simm;
	val |= (val << 8);
	val |= (val << 16);

	if (SSE_AVAIL) {
		modrm_o modrm;
		NativeVectorReg reg1 = jitcMapClientVectorRegisterDirty(vrD);

		asmALU_D(X86_MOV, x86_mem2(modrm, &gCPU.vtemp), val);
		asmMOVSS(reg1, &gCPU.vtemp);
		asmSHUFPS(reg1, reg1, 0x00);

		return flowContinue;
	}

	jitcDropClientVectorRegister(vrD);

	NativeReg r1 = jitcAllocRegister();
	asmALU(X86_MOV, r1, val);

	vec_setWord(vrD, 0, r1);
	vec_setWord(vrD, 4, r1);
	vec_setWord(vrD, 8, r1);
	vec_setWord(vrD,12, r1);

	return flowContinue;
#endif
}

/*	vspltish	Vector Splat Immediate Signed Half Word
 *	v.248
 */
void ppc_opc_vspltish()
{
	VECTOR_DEBUG_COMMON;
	int vrD, vrB;
	uint32 simm;
	uint64 val;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, simm, vrB);
	PPC_OPC_ASSERT(vrB==0);

	val = (simm & 0x10) ? (simm | 0xFFE0) : simm;
	val |= (val << 16);
	val |= (val << 32);

	gCPU.vr[vrD].d[0] = val;
	gCPU.vr[vrD].d[1] = val;
}
JITCFlow ppc_opc_gen_vspltish()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vspltish);
	return flowEndBlock;
#else
	int vrD, vrB;
	uint32 simm, val;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, simm, vrB);

	if (simm == 0) {
		vec_Zero(vrD);
		return flowContinue;
	} else if (simm == 0x1f) {
		vec_Neg1(vrD);
		return flowContinue;
	}

	val = (simm & 0x10) ? (simm | 0xFFE0) : simm;
	val |= (val << 16);

	if (SSE_AVAIL) {
		modrm_o modrm;
		NativeVectorReg reg1 = jitcMapClientVectorRegisterDirty(vrD);

		asmALU_D(X86_MOV, x86_mem2(modrm, &gCPU.vtemp), val);
		asmMOVSS(reg1, &gCPU.vtemp);
		asmSHUFPS(reg1, reg1, 0x00);

		return flowContinue;
	}

	jitcDropClientVectorRegister(vrD);

	NativeReg r1 = jitcAllocRegister();
	asmALU(X86_MOV, r1, val);

	vec_setWord(vrD, 0, r1);
	vec_setWord(vrD, 4, r1);
	vec_setWord(vrD, 8, r1);
	vec_setWord(vrD,12, r1);

	return flowContinue;
#endif
}

/*	vspltisw	Vector Splat Immediate Signed Word
 *	v.249
 */
void ppc_opc_vspltisw()
{
	VECTOR_DEBUG_COMMON;
	int vrD, vrB;
	uint32 simm;
	uint64 val;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, simm, vrB);
	PPC_OPC_ASSERT(vrB==0);

	val = (simm & 0x10) ? (simm | 0xFFFFFFE0) : simm;
	val |= (val << 32);

	gCPU.vr[vrD].d[0] = val;
	gCPU.vr[vrD].d[1] = val;
}
JITCFlow ppc_opc_gen_vspltisw()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vspltisw);
	return flowEndBlock;
#else
	int vrD, vrB;
	uint32 simm, val;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, simm, vrB);

	if (simm == 0) {
		vec_Zero(vrD);
		return flowContinue;
	} else if (simm == 0x1f) {
		vec_Neg1(vrD);
		return flowContinue;
	}

	val = (simm & 0x10) ? (simm | 0xFFFFFFE0) : simm;

	if (SSE_AVAIL) {
		modrm_o modrm;
		NativeVectorReg reg1 = jitcMapClientVectorRegisterDirty(vrD);

		asmALU_D(X86_MOV, x86_mem2(modrm, &gCPU.vtemp), val);
		asmMOVSS(reg1, &gCPU.vtemp);
		asmSHUFPS(reg1, reg1, 0x00);

		return flowContinue;
	}

	jitcDropClientVectorRegister(vrD);

	NativeReg r1 = jitcAllocRegister();
	asmALU(X86_MOV, r1, val);

	vec_setWord(vrD, 0, r1);
	vec_setWord(vrD, 4, r1);
	vec_setWord(vrD, 8, r1);
	vec_setWord(vrD,12, r1);

	return flowContinue;
#endif
}

/*	mfvscr		Move from Vector Status and Control Register
 *	v.129
 */
void ppc_opc_mfvscr()
{
	VECTOR_DEBUG_COMMON;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);
	PPC_OPC_ASSERT(vrB==0);

	VECT_W(gCPU.vr[vrD], 3) = gCPU.vscr;
	VECT_W(gCPU.vr[vrD], 2) = 0;
	VECT_D(gCPU.vr[vrD], 0) = 0;
}
JITCFlow ppc_opc_gen_mfvscr()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_mfvscr);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (SSE_AVAIL) {
		NativeVectorReg reg1 = jitcMapClientVectorRegisterDirty(vrD);
		NativeReg reg2 = jitcGetClientRegisterMapping(PPC_VSCR);

		if (reg1 != VECTREG_NO)
			jitcFlushRegister(NATIVE_REG | reg2);

		asmMOVSS(reg1, &gCPU.vscr);
	} else {
		jitcDropClientVectorRegister(vrD);

		NativeReg r1 = jitcGetClientRegister(PPC_VSCR);
		vec_setWord(vrD, 0, r1);

		jitcClobberRegister(NATIVE_REG | r1);

		asmMOV_NoFlags(r1, 0);	// avoid flag clobber
		vec_setWord(vrD, 4, r1);
		vec_setWord(vrD, 8, r1);
		vec_setWord(vrD,12, r1);
	}

	return flowContinue;
#endif
}

/*	mtvscr		Move to Vector Status and Control Register
 *	v.130
 */
void ppc_opc_mtvscr()
{
	VECTOR_DEBUG_COMMON;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);
	PPC_OPC_ASSERT(vrD==0);

	gCPU.vscr = VECT_W(gCPU.vr[vrB], 3);
}
JITCFlow ppc_opc_gen_mtvscr()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_mtvscr);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (SSE_AVAIL) {
		NativeVectorReg reg1 = jitcGetClientVectorRegisterMapping(vrB);

		if (reg1 != VECTREG_NO) {
			NativeReg reg2 = jitcGetClientRegisterMapping(PPC_VSCR);

			if (reg2 != REG_NO)
				jitcClobberRegister(NATIVE_REG | reg2);

			asmMOVSS(&gCPU.vscr, reg1);
			return flowContinue;
		}
	}

	jitcFlushClientVectorRegister(vrB);

	NativeReg r1 = jitcMapClientRegisterDirty(PPC_VSCR);

	vec_getWord(r1, vrB, 0);
	return flowContinue;
#endif
}

/*	vpkuhum		Vector Pack Unsigned Half Word Unsigned Modulo
 *	v.224
 */
void ppc_opc_vpkuhum()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	VECT_B(r, 0) = VECT_B(gCPU.vr[vrA], 1);
	VECT_B(r, 1) = VECT_B(gCPU.vr[vrA], 3);
	VECT_B(r, 2) = VECT_B(gCPU.vr[vrA], 5);
	VECT_B(r, 3) = VECT_B(gCPU.vr[vrA], 7);
	VECT_B(r, 4) = VECT_B(gCPU.vr[vrA], 9);
	VECT_B(r, 5) = VECT_B(gCPU.vr[vrA],11);
	VECT_B(r, 6) = VECT_B(gCPU.vr[vrA],13);
	VECT_B(r, 7) = VECT_B(gCPU.vr[vrA],15);

	VECT_B(r, 8) = VECT_B(gCPU.vr[vrB], 1);
	VECT_B(r, 9) = VECT_B(gCPU.vr[vrB], 3);
	VECT_B(r,10) = VECT_B(gCPU.vr[vrB], 5);
	VECT_B(r,11) = VECT_B(gCPU.vr[vrB], 7);
	VECT_B(r,12) = VECT_B(gCPU.vr[vrB], 9);
	VECT_B(r,13) = VECT_B(gCPU.vr[vrB],11);
	VECT_B(r,14) = VECT_B(gCPU.vr[vrB],13);
	VECT_B(r,15) = VECT_B(gCPU.vr[vrB],15);

	gCPU.vr[vrD] = r;
}
JITCFlow ppc_opc_gen_vpkuhum()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vpkuhum);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg8 reg1 = (NativeReg8)jitcAllocRegister(NATIVE_REG_8);

	if (vrB == vrD && vrA == vrD) {
		for (int i=0; i<16; i += 2) {
			vec_getHalf((NativeReg16)reg1, vrA, i);

			vec_setByte(vrT, REG_NO, (i >> 1), reg1);
			vec_setByte(vrT, REG_NO, 8+(i >> 1), reg1);
		}

		vec_Copy(vrD, vrT);
	} else if (vrA == vrD) {
		for (int i=0; i<16; i += 2) {
			vec_getHalf((NativeReg16)reg1, vrA, 14 - i);
			vec_setByte(vrD, REG_NO, 15 - (i >> 1), reg1);
		}

		for (int i=0; i<16; i += 2) {
			vec_getHalf((NativeReg16)reg1, vrB, i);
			vec_setByte(vrD, REG_NO, (i >> 1), reg1);
		}
	} else {
		for (int i=0; i<16; i += 2) {
			vec_getHalf((NativeReg16)reg1, vrB, i);
			vec_setByte(vrD, REG_NO, (i >> 1), reg1);
		}

		for (int i=0; i<16; i += 2) {
			vec_getHalf((NativeReg16)reg1, vrA, i);
			vec_setByte(vrD, REG_NO, 8 + (i >> 1), reg1);
		}
	}

	return flowContinue;
#endif
}

/*	vpkuwum		Vector Pack Unsigned Word Unsigned Modulo
 *	v.226
 */
void ppc_opc_vpkuwum()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	VECT_H(r, 0) = VECT_H(gCPU.vr[vrA], 1);
	VECT_H(r, 1) = VECT_H(gCPU.vr[vrA], 3);
	VECT_H(r, 2) = VECT_H(gCPU.vr[vrA], 5);
	VECT_H(r, 3) = VECT_H(gCPU.vr[vrA], 7);

	VECT_H(r, 4) = VECT_H(gCPU.vr[vrB], 1);
	VECT_H(r, 5) = VECT_H(gCPU.vr[vrB], 3);
	VECT_H(r, 6) = VECT_H(gCPU.vr[vrB], 5);
	VECT_H(r, 7) = VECT_H(gCPU.vr[vrB], 7);

	gCPU.vr[vrD] = r;
}
JITCFlow ppc_opc_gen_vpkuwum()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vpkuwum);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg16 reg1 = (NativeReg16)jitcAllocRegister();

	if (vrB == vrD && vrA == vrD) {
		for (int i=0; i<16; i += 4) {
			vec_getWord((NativeReg)reg1, vrA, i);

			vec_setHalf(vrT, (i >> 1), reg1);
			vec_setHalf(vrT, 8+(i >> 1), reg1);
		}

		vec_Copy(vrD, vrT);
	} else if (vrA == vrD) {
		for (int i=0; i<16; i += 4) {
			vec_getWord((NativeReg)reg1, vrA, 12 - i);
			vec_setHalf(vrD, 14 - (i >> 1), reg1);
		}

		for (int i=0; i<16; i += 4) {
			vec_getWord((NativeReg)reg1, vrB, i);
			vec_setHalf(vrD, (i >> 1), reg1);
		}
	} else {
		for (int i=0; i<16; i += 4) {
			vec_getWord((NativeReg)reg1, vrB, i);
			vec_setHalf(vrD, (i >> 1), reg1);
		}

		for (int i=0; i<16; i += 4) {
			vec_getWord((NativeReg)reg1, vrA, i);
			vec_setHalf(vrD, 8 + (i >> 1), reg1);
		}
	}

	return flowContinue;
#endif
}

static inline void ppc_opc_gen_helper_vpkpx(NativeReg reg1, NativeReg reg2, NativeReg reg3)
{
	asmALU(X86_MOV, reg2, reg1);
	asmALU(X86_MOV, reg3, reg1);

	asmALU(X86_AND, reg1, 0x000000f8);
	asmALU(X86_AND, reg2, 0x0000f800);
	asmALU(X86_AND, reg3, 0x01f80000);

	asmShift(X86_SHR, reg1, 3);
	asmShift(X86_SHR, reg2, 6);
	asmShift(X86_SHR, reg3, 9);

	asmALU(X86_OR, reg1, reg2);
	asmALU(X86_OR, reg1, reg3);
}

/*	vpkpx		Vector Pack Pixel32
 *	v.219
 */
void ppc_opc_vpkpx()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	VECT_H(r, 0) = PACK_PIXEL(VECT_W(gCPU.vr[vrA], 0));
	VECT_H(r, 1) = PACK_PIXEL(VECT_W(gCPU.vr[vrA], 1));
	VECT_H(r, 2) = PACK_PIXEL(VECT_W(gCPU.vr[vrA], 2));
	VECT_H(r, 3) = PACK_PIXEL(VECT_W(gCPU.vr[vrA], 3));

	VECT_H(r, 4) = PACK_PIXEL(VECT_W(gCPU.vr[vrB], 0));
	VECT_H(r, 5) = PACK_PIXEL(VECT_W(gCPU.vr[vrB], 1));
	VECT_H(r, 6) = PACK_PIXEL(VECT_W(gCPU.vr[vrB], 2));
	VECT_H(r, 7) = PACK_PIXEL(VECT_W(gCPU.vr[vrB], 3));

	gCPU.vr[vrD] = r;
}
JITCFlow ppc_opc_gen_vpkpx()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vpkpx);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg1 = jitcAllocRegister();
	NativeReg reg2 = jitcAllocRegister();
	NativeReg reg3 = jitcAllocRegister();

	if((vrA == vrB)) {
		for (int i=0; i<8; i += 2) {
			vec_getWord(reg1, vrB, i<<1);
			ppc_opc_gen_helper_vpkpx(reg1, reg2, reg3);
			vec_setHalf(vrD, i, (NativeReg16)reg1);

			if (vrD != vrA)
				vec_setHalf(vrD, i+8, (NativeReg16)reg1);
		}

		if (vrD == vrB) {
			vec_getWord(reg1, vrD, 0);
			vec_getWord(reg2, vrD, 4);

			vec_setWord(vrD, 8, reg1);
			vec_setWord(vrD,12, reg2);
		}
	} else if (vrB == vrD) {
		for (int i=0; i<8; i += 2) {
			vec_getWord(reg1, vrB, i<<1);
			ppc_opc_gen_helper_vpkpx(reg1, reg2, reg3);
			vec_setHalf(vrD, i, (NativeReg16)reg1);
		}

		for (int i=0; i<8; i += 2) {
			vec_getWord(reg1, vrA, i<<1);
			ppc_opc_gen_helper_vpkpx(reg1, reg2, reg3);
			vec_setHalf(vrD, i+8, (NativeReg16)reg1);
		}
	} else {
		for (int i=0; i<8; i += 2) {
			vec_getWord(reg1, vrA, 12-(i<<1));
			ppc_opc_gen_helper_vpkpx(reg1, reg2, reg3);
			vec_setHalf(vrD, 14-i, (NativeReg16)reg1);
		}

		for (int i=0; i<8; i += 2) {
			vec_getWord(reg1, vrB, i<<1);
			ppc_opc_gen_helper_vpkpx(reg1, reg2, reg3);
			vec_setHalf(vrD, i, (NativeReg16)reg1);
		}
	}

	return flowContinue;
#endif
}


/*	vpkuhus		Vector Pack Unsigned Half Word Unsigned Saturate
 *	v.225
 */
void ppc_opc_vpkuhus()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	VECT_B(r, 0) = SATURATE_UB(VECT_H(gCPU.vr[vrA], 0));
	VECT_B(r, 1) = SATURATE_UB(VECT_H(gCPU.vr[vrA], 1));
	VECT_B(r, 2) = SATURATE_UB(VECT_H(gCPU.vr[vrA], 2));
	VECT_B(r, 3) = SATURATE_UB(VECT_H(gCPU.vr[vrA], 3));
	VECT_B(r, 4) = SATURATE_UB(VECT_H(gCPU.vr[vrA], 4));
	VECT_B(r, 5) = SATURATE_UB(VECT_H(gCPU.vr[vrA], 5));
	VECT_B(r, 6) = SATURATE_UB(VECT_H(gCPU.vr[vrA], 6));
	VECT_B(r, 7) = SATURATE_UB(VECT_H(gCPU.vr[vrA], 7));

	VECT_B(r, 8) = SATURATE_UB(VECT_H(gCPU.vr[vrB], 0));
	VECT_B(r, 9) = SATURATE_UB(VECT_H(gCPU.vr[vrB], 1));
	VECT_B(r,10) = SATURATE_UB(VECT_H(gCPU.vr[vrB], 2));
	VECT_B(r,11) = SATURATE_UB(VECT_H(gCPU.vr[vrB], 3));
	VECT_B(r,12) = SATURATE_UB(VECT_H(gCPU.vr[vrB], 4));
	VECT_B(r,13) = SATURATE_UB(VECT_H(gCPU.vr[vrB], 5));
	VECT_B(r,14) = SATURATE_UB(VECT_H(gCPU.vr[vrB], 6));
	VECT_B(r,15) = SATURATE_UB(VECT_H(gCPU.vr[vrB], 7));

	gCPU.vr[vrD] = r;
}
JITCFlow ppc_opc_gen_vpkuhus()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vpkuhus);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg8 reg1 = (NativeReg8)jitcAllocRegister(NATIVE_REG_8);

	if (vrB == vrD && vrA == vrD) {
		for (int i=0; i<16; i += 2) {
			vec_getHalfZ((NativeReg)reg1, vrA, i);

			vec_saturateUB(reg1);

			vec_setByte(vrT, REG_NO, (i >> 1), reg1);
			vec_setByte(vrT, REG_NO, 8+(i >> 1), reg1);
		}

		vec_Copy(vrD, vrT);
	} else if (vrA == vrD) {
		for (int i=0; i<16; i += 2) {
			vec_getHalfZ((NativeReg)reg1, vrA, 14 - i);

			vec_saturateUB(reg1);

			vec_setByte(vrD, REG_NO, 15 - (i >> 1), reg1);
		}

		for (int i=0; i<16; i += 2) {
			vec_getHalfZ((NativeReg)reg1, vrB, i);

			vec_saturateUB(reg1);

			vec_setByte(vrD, REG_NO, (i >> 1), reg1);
		}
	} else {
		for (int i=0; i<16; i += 2) {
			vec_getHalfZ((NativeReg)reg1, vrB, i);

			vec_saturateUB(reg1);

			vec_setByte(vrD, REG_NO, (i >> 1), reg1);
		}

		for (int i=0; i<16; i += 2) {
			vec_getHalfZ((NativeReg)reg1, vrA, i);

			vec_saturateUB(reg1);

			vec_setByte(vrD, REG_NO, 8 + (i >> 1), reg1);
		}
	}

	return flowContinue;
#endif
}

/*	vpkshss		Vector Pack Signed Half Word Signed Saturate
 *	v.220
 */
void ppc_opc_vpkshss()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	VECT_B(r, 0) = SATURATE_SB(VECT_H(gCPU.vr[vrA], 0));
	VECT_B(r, 1) = SATURATE_SB(VECT_H(gCPU.vr[vrA], 1));
	VECT_B(r, 2) = SATURATE_SB(VECT_H(gCPU.vr[vrA], 2));
	VECT_B(r, 3) = SATURATE_SB(VECT_H(gCPU.vr[vrA], 3));
	VECT_B(r, 4) = SATURATE_SB(VECT_H(gCPU.vr[vrA], 4));
	VECT_B(r, 5) = SATURATE_SB(VECT_H(gCPU.vr[vrA], 5));
	VECT_B(r, 6) = SATURATE_SB(VECT_H(gCPU.vr[vrA], 6));
	VECT_B(r, 7) = SATURATE_SB(VECT_H(gCPU.vr[vrA], 7));

	VECT_B(r, 8) = SATURATE_SB(VECT_H(gCPU.vr[vrB], 0));
	VECT_B(r, 9) = SATURATE_SB(VECT_H(gCPU.vr[vrB], 1));
	VECT_B(r,10) = SATURATE_SB(VECT_H(gCPU.vr[vrB], 2));
	VECT_B(r,11) = SATURATE_SB(VECT_H(gCPU.vr[vrB], 3));
	VECT_B(r,12) = SATURATE_SB(VECT_H(gCPU.vr[vrB], 4));
	VECT_B(r,13) = SATURATE_SB(VECT_H(gCPU.vr[vrB], 5));
	VECT_B(r,14) = SATURATE_SB(VECT_H(gCPU.vr[vrB], 6));
	VECT_B(r,15) = SATURATE_SB(VECT_H(gCPU.vr[vrB], 7));

	gCPU.vr[vrD] = r;
}
JITCFlow ppc_opc_gen_vpkshss()
{
	ppc_opc_gen_interpret(ppc_opc_vpkshss);
	return flowEndBlock;
}

/*	vpkuwus		Vector Pack Unsigned Word Unsigned Saturate
 *	v.227
 */
void ppc_opc_vpkuwus()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	VECT_H(r, 0) = SATURATE_UH(VECT_W(gCPU.vr[vrA], 0));
	VECT_H(r, 1) = SATURATE_UH(VECT_W(gCPU.vr[vrA], 1));
	VECT_H(r, 2) = SATURATE_UH(VECT_W(gCPU.vr[vrA], 2));
	VECT_H(r, 3) = SATURATE_UH(VECT_W(gCPU.vr[vrA], 3));

	VECT_H(r, 4) = SATURATE_UH(VECT_W(gCPU.vr[vrB], 0));
	VECT_H(r, 5) = SATURATE_UH(VECT_W(gCPU.vr[vrB], 1));
	VECT_H(r, 6) = SATURATE_UH(VECT_W(gCPU.vr[vrB], 2));
	VECT_H(r, 7) = SATURATE_UH(VECT_W(gCPU.vr[vrB], 3));

	gCPU.vr[vrD] = r;
}
JITCFlow ppc_opc_gen_vpkuwus()
{
	ppc_opc_gen_interpret(ppc_opc_vpkuwus);
	return flowEndBlock;
}

/*	vpkswss		Vector Pack Signed Word Signed Saturate
 *	v.222
 */
void ppc_opc_vpkswss()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	VECT_H(r, 0) = SATURATE_SH(VECT_W(gCPU.vr[vrA], 0));
	VECT_H(r, 1) = SATURATE_SH(VECT_W(gCPU.vr[vrA], 1));
	VECT_H(r, 2) = SATURATE_SH(VECT_W(gCPU.vr[vrA], 2));
	VECT_H(r, 3) = SATURATE_SH(VECT_W(gCPU.vr[vrA], 3));

	VECT_H(r, 4) = SATURATE_SH(VECT_W(gCPU.vr[vrB], 0));
	VECT_H(r, 5) = SATURATE_SH(VECT_W(gCPU.vr[vrB], 1));
	VECT_H(r, 6) = SATURATE_SH(VECT_W(gCPU.vr[vrB], 2));
	VECT_H(r, 7) = SATURATE_SH(VECT_W(gCPU.vr[vrB], 3));

	gCPU.vr[vrD] = r;
}
JITCFlow ppc_opc_gen_vpkswss()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vpkswss);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (SSE2_AVAIL) {
		noncommutative_operation(X86_PACKSSDW, vrD, vrB, vrA);
		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg1 = jitcAllocRegister();
	NativeReg reg2 = jitcAllocRegister();

	if (vrB == vrD && vrA == vrD) {
		for (int i=0; i<16; i += 4) {
			vec_getWord(reg1, vrA, i);

			vec_saturateSH(reg1, reg2);

			vec_setHalf(vrT, (i >> 1), (NativeReg16)reg1);
			vec_setHalf(vrT, 8+(i >> 1), (NativeReg16)reg1);
		}

		vec_Copy(vrD, vrT);
	} else if (vrA == vrD) {
		for (int i=0; i<16; i += 4) {
			vec_getWord(reg1, vrA, 12 - i);

			vec_saturateSH(reg1, reg2);

			vec_setHalf(vrD, 14 - (i >> 1), (NativeReg16)reg1);
		}

		for (int i=0; i<16; i += 4) {
			vec_getWord(reg1, vrB, i);

			vec_saturateSH(reg1, reg2);

			vec_setHalf(vrD, (i >> 1), (NativeReg16)reg1);
		}
	} else {
		for (int i=0; i<16; i += 4) {
			vec_getWord(reg1, vrB, i);

			vec_saturateSH(reg1, reg2);

			vec_setHalf(vrD, (i >> 1), (NativeReg16)reg1);
		}

		for (int i=0; i<16; i += 4) {
			vec_getWord(reg1, vrA, i);

			vec_saturateSH(reg1, reg2);

			vec_setHalf(vrD, 8 + (i >> 1), (NativeReg16)reg1);
		}
	}

	return flowContinue;
#endif
}

/*	vpkshus		Vector Pack Signed Half Word Unsigned Saturate
 *	v.221
 */
void ppc_opc_vpkshus()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	VECT_B(r, 0) = SATURATE_USB(VECT_H(gCPU.vr[vrA], 0));
	VECT_B(r, 1) = SATURATE_USB(VECT_H(gCPU.vr[vrA], 1));
	VECT_B(r, 2) = SATURATE_USB(VECT_H(gCPU.vr[vrA], 2));
	VECT_B(r, 3) = SATURATE_USB(VECT_H(gCPU.vr[vrA], 3));
	VECT_B(r, 4) = SATURATE_USB(VECT_H(gCPU.vr[vrA], 4));
	VECT_B(r, 5) = SATURATE_USB(VECT_H(gCPU.vr[vrA], 5));
	VECT_B(r, 6) = SATURATE_USB(VECT_H(gCPU.vr[vrA], 6));
	VECT_B(r, 7) = SATURATE_USB(VECT_H(gCPU.vr[vrA], 7));

	VECT_B(r, 8) = SATURATE_USB(VECT_H(gCPU.vr[vrB], 0));
	VECT_B(r, 9) = SATURATE_USB(VECT_H(gCPU.vr[vrB], 1));
	VECT_B(r,10) = SATURATE_USB(VECT_H(gCPU.vr[vrB], 2));
	VECT_B(r,11) = SATURATE_USB(VECT_H(gCPU.vr[vrB], 3));
	VECT_B(r,12) = SATURATE_USB(VECT_H(gCPU.vr[vrB], 4));
	VECT_B(r,13) = SATURATE_USB(VECT_H(gCPU.vr[vrB], 5));
	VECT_B(r,14) = SATURATE_USB(VECT_H(gCPU.vr[vrB], 6));
	VECT_B(r,15) = SATURATE_USB(VECT_H(gCPU.vr[vrB], 7));

	gCPU.vr[vrD] = r;
}
JITCFlow ppc_opc_gen_vpkshus()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vpkshus);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (SSE2_AVAIL) {
		noncommutative_operation(X86_PACKUSWB, vrD, vrB, vrA);
		return flowContinue;
	}

	NativeReg reg1 = jitcAllocRegister(NATIVE_REG_8);
	NativeReg reg2 = jitcAllocRegister();

	if (vrB == vrD && vrA == vrD) {
		for (int i=0; i<16; i += 2) {
			vec_getHalfS(reg1, vrA, i);

			vec_saturateSUB(reg1, reg2);

			vec_setByte(vrT, REG_NO, (i >> 1), (NativeReg8)reg1);
			vec_setByte(vrT, REG_NO, 8+(i >> 1), (NativeReg8)reg1);
		}

		vec_Copy(vrD, vrT);
	} else if (vrA == vrD) {
		for (int i=0; i<16; i += 2) {
			vec_getHalfS(reg1, vrA, 14 - i);

			vec_saturateSUB(reg1, reg2);

			vec_setByte(vrD, REG_NO, 15 - (i >> 1), (NativeReg8)reg1);
		}

		for (int i=0; i<16; i += 2) {
			vec_getHalfS(reg1, vrB, i);

			vec_saturateSUB(reg1, reg2);

			vec_setByte(vrD, REG_NO, (i >> 1), (NativeReg8)reg1);
		}
	} else {
		for (int i=0; i<16; i += 2) {
			vec_getHalfS(reg1, vrB, i);

			vec_saturateSUB(reg1, reg2);

			vec_setByte(vrD, REG_NO, (i >> 1), (NativeReg8)reg1);
		}

		for (int i=0; i<16; i += 2) {
			vec_getHalfS(reg1, vrA, i);

			vec_saturateSUB(reg1, reg2);

			vec_setByte(vrD, REG_NO, 8 + (i >> 1), (NativeReg8)reg1);
		}
	}

	return flowContinue;
#endif
}

/*	vpkswus		Vector Pack Signed Word Unsigned Saturate
 *	v.223
 */
void ppc_opc_vpkswus()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	VECT_H(r, 0) = SATURATE_USH(VECT_W(gCPU.vr[vrA], 0));
	VECT_H(r, 1) = SATURATE_USH(VECT_W(gCPU.vr[vrA], 1));
	VECT_H(r, 2) = SATURATE_USH(VECT_W(gCPU.vr[vrA], 2));
	VECT_H(r, 3) = SATURATE_USH(VECT_W(gCPU.vr[vrA], 3));

	VECT_H(r, 4) = SATURATE_USH(VECT_W(gCPU.vr[vrB], 0));
	VECT_H(r, 5) = SATURATE_USH(VECT_W(gCPU.vr[vrB], 1));
	VECT_H(r, 6) = SATURATE_USH(VECT_W(gCPU.vr[vrB], 2));
	VECT_H(r, 7) = SATURATE_USH(VECT_W(gCPU.vr[vrB], 3));

	gCPU.vr[vrD] = r;
}
JITCFlow ppc_opc_gen_vpkswus()
{
	ppc_opc_gen_interpret(ppc_opc_vpkswus);
	return flowEndBlock;
}

/*	vupkhsb		Vector Unpack High Signed Byte
 *	v.277
 */
void ppc_opc_vupkhsb()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);

	VECT_SH(r, 0) = VECT_SB(gCPU.vr[vrB], 0);
	VECT_SH(r, 1) = VECT_SB(gCPU.vr[vrB], 1);
	VECT_SH(r, 2) = VECT_SB(gCPU.vr[vrB], 2);
	VECT_SH(r, 3) = VECT_SB(gCPU.vr[vrB], 3);
	VECT_SH(r, 4) = VECT_SB(gCPU.vr[vrB], 4);
	VECT_SH(r, 5) = VECT_SB(gCPU.vr[vrB], 5);
	VECT_SH(r, 6) = VECT_SB(gCPU.vr[vrB], 6);
	VECT_SH(r, 7) = VECT_SB(gCPU.vr[vrB], 7);

	gCPU.vr[vrD] = r;
}
JITCFlow ppc_opc_gen_vupkhsb()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vupkhsb);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	NativeReg reg = jitcAllocRegister();

	for (int i=0; i<16; i += 2) {
		vec_getByteS(reg, vrB, 8 + (i >> 1));

		vec_setHalf(vrD, i, (NativeReg16)reg);
	}

	return flowContinue;
#endif
}

/*	vupkhpx		Vector Unpack High Pixel32
 *	v.279
 */
void ppc_opc_vupkhpx()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);

	VECT_W(r, 0) = UNPACK_PIXEL(VECT_H(gCPU.vr[vrB], 0));
	VECT_W(r, 1) = UNPACK_PIXEL(VECT_H(gCPU.vr[vrB], 1));
	VECT_W(r, 2) = UNPACK_PIXEL(VECT_H(gCPU.vr[vrB], 2));
	VECT_W(r, 3) = UNPACK_PIXEL(VECT_H(gCPU.vr[vrB], 3));
	
	gCPU.vr[vrD] = r;
}
JITCFlow ppc_opc_gen_vupkhpx()
{
	ppc_opc_gen_interpret(ppc_opc_vupkhpx);
	return flowEndBlock;
}

/*	vupkhsh		Vector Unpack High Signed Half Word
 *	v.278
 */
void ppc_opc_vupkhsh()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);

	VECT_SW(r, 0) = VECT_SH(gCPU.vr[vrB], 0);
	VECT_SW(r, 1) = VECT_SH(gCPU.vr[vrB], 1);
	VECT_SW(r, 2) = VECT_SH(gCPU.vr[vrB], 2);
	VECT_SW(r, 3) = VECT_SH(gCPU.vr[vrB], 3);

	gCPU.vr[vrD] = r;
}
JITCFlow ppc_opc_gen_vupkhsh()
{
	ppc_opc_gen_interpret(ppc_opc_vupkhsh);
	return flowEndBlock;
}

/*	vupklsb		Vector Unpack Low Signed Byte
 *	v.280
 */
void ppc_opc_vupklsb()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);

	VECT_SH(r, 0) = VECT_SB(gCPU.vr[vrB], 8);
	VECT_SH(r, 1) = VECT_SB(gCPU.vr[vrB], 9);
	VECT_SH(r, 2) = VECT_SB(gCPU.vr[vrB],10);
	VECT_SH(r, 3) = VECT_SB(gCPU.vr[vrB],11);
	VECT_SH(r, 4) = VECT_SB(gCPU.vr[vrB],12);
	VECT_SH(r, 5) = VECT_SB(gCPU.vr[vrB],13);
	VECT_SH(r, 6) = VECT_SB(gCPU.vr[vrB],14);
	VECT_SH(r, 7) = VECT_SB(gCPU.vr[vrB],15);

	gCPU.vr[vrD] = r;
}
JITCFlow ppc_opc_gen_vupklsb()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vupklsb);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	NativeReg reg = jitcAllocRegister();

	for (int i=0; i<16; i += 2) {
		vec_getByteS(reg, vrB, 7 - (i >> 1));

		vec_setHalf(vrD, 14 - i, (NativeReg16)reg);
	}

	return flowContinue;
#endif
}

/*	vupklpx		Vector Unpack Low Pixel32
 *	v.279
 */
void ppc_opc_vupklpx()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);

	VECT_W(r, 0) = UNPACK_PIXEL(VECT_H(gCPU.vr[vrB], 4));
	VECT_W(r, 1) = UNPACK_PIXEL(VECT_H(gCPU.vr[vrB], 5));
	VECT_W(r, 2) = UNPACK_PIXEL(VECT_H(gCPU.vr[vrB], 6));
	VECT_W(r, 3) = UNPACK_PIXEL(VECT_H(gCPU.vr[vrB], 7));

	gCPU.vr[vrD] = r;
}
JITCFlow ppc_opc_gen_vupklpx()
{
	ppc_opc_gen_interpret(ppc_opc_vupklpx);
	return flowEndBlock;
}

/*	vupklsh		Vector Unpack Low Signed Half Word
 *	v.281
 */
void ppc_opc_vupklsh()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	Vector_t r;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);

	VECT_SW(r, 0) = VECT_SH(gCPU.vr[vrB], 4);
	VECT_SW(r, 1) = VECT_SH(gCPU.vr[vrB], 5);
	VECT_SW(r, 2) = VECT_SH(gCPU.vr[vrB], 6);
	VECT_SW(r, 3) = VECT_SH(gCPU.vr[vrB], 7);

	gCPU.vr[vrD] = r;
}
JITCFlow ppc_opc_gen_vupklsh()
{
	ppc_opc_gen_interpret(ppc_opc_vupklsh);
	return flowEndBlock;
}

static inline void helper_gen_vaddubm(NativeReg reg, NativeReg reg2, modrm_p modrma, modrm_p modrmb)
{
	asmALU(X86_MOV, reg, modrma);
	asmALU(X86_MOV, reg2, reg);

	asmALU(X86_AND, reg, modrmb);
	asmALU(X86_XOR, reg2, modrmb);

	asmShift(X86_SHL, reg, 1);
	asmALU(X86_AND, reg2, 0xfefefefe);

	asmALU(X86_ADD, reg2, reg);
	asmALU(X86_AND, reg2, 0x01010100);
}

/*	vaddubm		Vector Add Unsigned Byte Modulo
 *	v.141
 */
void ppc_opc_vaddubm()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint8 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		res = gCPU.vr[vrA].b[i] + gCPU.vr[vrB].b[i];
		gCPU.vr[vrD].b[i] = res;
	}
}
JITCFlow ppc_opc_gen_vaddubm()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vaddubm);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrma, modrmb;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (SSE2_AVAIL) {
		commutative_operation(PALUB(X86_PADD), vrD, vrA, vrB);
		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg = jitcAllocRegister();
	NativeReg reg2 = jitcAllocRegister();

	if ((vrA == vrD) || (vrB == vrD)) {
		int vrS = vrB;

		if (vrD == vrB) {
			vrS = vrA;
		}

		for (int i=0; i<16; i += 4) {
			x86_mem2(modrma, &(gCPU.vr[vrD].b[i]));
			x86_mem2(modrmb, &(gCPU.vr[vrS].b[i]));

			helper_gen_vaddubm(reg, reg2, modrma, modrmb);

			asmALU(X86_SUB, reg2, modrmb);
			asmALU(X86_SUB, modrma, reg2);
		}
	} else {
		for (int i=0; i<16; i += 4) {
			x86_mem2(modrma, &(gCPU.vr[vrA].b[i]));
			x86_mem2(modrmb, &(gCPU.vr[vrB].b[i]));

			helper_gen_vaddubm(reg, reg2, modrma, modrmb);

			asmALU(X86_MOV, reg, modrmb);
			asmALU(X86_SUB, reg, reg2);
			asmALU(X86_ADD, reg, modrma);

			vec_setWord(vrD, i, reg);
		}
	}

	return flowContinue;
#endif
}

static inline void helper_gen_vadduhm(NativeReg reg, NativeReg reg2, modrm_p modrma, modrm_p modrmb)
{
	asmALU(X86_MOV, reg, modrma);
	asmALU(X86_MOV, reg2, reg);

	asmALU(X86_AND, reg, modrmb);
	asmALU(X86_XOR, reg2, modrmb);

	asmShift(X86_SHL, reg, 1);
	asmALU(X86_AND, reg2, 0xfffefffe);

	asmALU(X86_ADD, reg2, reg);
	asmALU(X86_AND, reg2, 0x00010000);
}

/*	vadduhm		Vector Add Unsigned Half Word Modulo
 *	v.143
 */
void ppc_opc_vadduhm()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint16 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = gCPU.vr[vrA].h[i] + gCPU.vr[vrB].h[i];
		gCPU.vr[vrD].h[i] = res;
	}
}
JITCFlow ppc_opc_gen_vadduhm()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vadduhm);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrma, modrmb;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (SSE2_AVAIL) {
		commutative_operation(PALUW(X86_PADD), vrD, vrA, vrB);
		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg = jitcAllocRegister();
	NativeReg reg2 = jitcAllocRegister();

	if ((vrA == vrD) || (vrB == vrD)) {
		int vrS = vrB;

		if (vrD == vrB) {
			vrS = vrA;
		}

		for (int i=0; i<16; i += 4) {
			x86_mem2(modrma, &(gCPU.vr[vrD].b[i]));
			x86_mem2(modrmb, &(gCPU.vr[vrS].b[i]));

			helper_gen_vadduhm(reg, reg2, modrma, modrmb);

			asmALU(X86_SUB, reg2, modrmb);
			asmALU(X86_SUB, modrma, reg2);
		}
	} else {
		for (int i=0; i<16; i += 4) {
			x86_mem2(modrma, &(gCPU.vr[vrA].b[i]));
			x86_mem2(modrmb, &(gCPU.vr[vrB].b[i]));

			helper_gen_vadduhm(reg, reg2, modrma, modrmb);

			asmALU(X86_MOV, reg, modrmb);
			asmALU(X86_SUB, reg, reg2);
			asmALU(X86_ADD, reg, modrma);

			vec_setWord(vrD, i, reg);
		}
	}

	return flowContinue;
#endif
}

/*	vadduwm		Vector Add Unsigned Word Modulo
 *	v.145
 */
void ppc_opc_vadduwm()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint32 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = gCPU.vr[vrA].w[i] + gCPU.vr[vrB].w[i];
		gCPU.vr[vrD].w[i] = res;
	}
}
JITCFlow ppc_opc_gen_vadduwm()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vadduwm);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);
	NativeReg reg = jitcAllocRegister();

	if (SSE2_AVAIL) {
		commutative_operation(PALUD(X86_PADD), vrD, vrA, vrB);
		return flowContinue;
	}

	if ((vrA == vrD) || (vrB == vrD)) {
		int vrS = vrB;

		if (vrD == vrB) {
			vrS = vrA;
		}

		for (int i=0; i<16; i += 4) {
			vec_getWord(reg, vrS, i);

			asmALU(X86_ADD,
				x86_mem2(modrm, &(gCPU.vr[vrD].b[i])), reg);
		}
	} else {
		for (int i=0; i<16; i += 4) {
			vec_getWord(reg, vrA, i);

			asmALU(X86_ADD, reg,
				x86_mem2(modrm, &(gCPU.vr[vrB].b[i])));

			vec_setWord(vrD, i, reg);
		}
	}

	return flowContinue;
#endif
}

/*	vaddfp		Vector Add Float Point
 *	v.137
 */
void ppc_opc_vaddfp()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	float res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		res = gCPU.vr[vrA].f[i] + gCPU.vr[vrB].f[i];
		gCPU.vr[vrD].f[i] = res;
	}
}
JITCFlow ppc_opc_gen_vaddfp()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vaddfp);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (SSE_AVAIL) {
		commutative_operation(X86_ADDPS, vrD, vrA, vrB);
		return flowContinue;
	}

	ppc_opc_gen_interpret(ppc_opc_vaddfp);
	return flowEndBlock;
#endif
}

/*	vaddcuw		Vector Add Carryout Unsigned Word
 *	v.136
 */
void ppc_opc_vaddcuw()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint32 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = gCPU.vr[vrA].w[i] + gCPU.vr[vrB].w[i];
		gCPU.vr[vrD].w[i] = (res < gCPU.vr[vrA].w[i]) ? 1 : 0;
	}
}
JITCFlow ppc_opc_gen_vaddcuw()
{
	ppc_opc_gen_interpret(ppc_opc_vaddcuw);
	return flowEndBlock;
}

static inline void helper_gen_vaddubs(NativeReg reg, NativeReg reg2, modrm_p modrma, modrm_p modrmb)
{
	asmALU(X86_MOV, reg, modrma);
	asmALU(X86_MOV, reg2, reg);

	asmALU(X86_XOR, reg, modrmb);
	asmALU(X86_AND, reg2, modrmb);

	asmShift(X86_SHR, reg, 1);
	asmALU(X86_AND, reg, 0x7f7f7f7f);

	asmALU(X86_ADD, reg2, reg);
	asmALU(X86_AND, reg2, 0x80808080);

	asmShift(X86_SHR, reg2, 7);
	asmALU(X86_ADD, reg2, 0x7f7f7f7f);
	asmALU(X86_XOR, reg2, 0x7f7f7f7f);
	NativeAddress skip = asmJxxFixup(X86_Z);

	vec_raise_saturate();

	asmResolveFixup(skip);
}

/*	vaddubs		Vector Add Unsigned Byte Saturate
 *	v.142
 */
void ppc_opc_vaddubs()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint16 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		res = (uint16)gCPU.vr[vrA].b[i] + (uint16)gCPU.vr[vrB].b[i];
		gCPU.vr[vrD].b[i] = SATURATE_UB(res);
	}
}
JITCFlow ppc_opc_gen_vaddubs()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vaddubs);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrma, modrmb;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (SSE2_AVAIL) {
		commutative_operation(PALUB(X86_PADDUS), vrD, vrA, vrB);
		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg = jitcAllocRegister();
	NativeReg reg2 = jitcAllocRegister();

	if ((vrA == vrD) || (vrB == vrD)) {
		int vrS = vrB;

		if (vrD == vrB) {
			vrS = vrA;
		}

		for (int i=0; i<16; i += 4) {
			x86_mem2(modrma, &(gCPU.vr[vrD].b[i]));
			x86_mem2(modrmb, &(gCPU.vr[vrS].b[i]));

			helper_gen_vaddubs(reg, reg2, modrma, modrmb);

			asmALU(X86_MOV, reg, modrmb);
			asmALU(X86_SUB, reg, reg2);
			asmALU(X86_ADD, modrma, reg);
			asmALU(X86_OR, modrma, reg2);
		}
	} else {
		for (int i=0; i<16; i += 4) {
			x86_mem2(modrma, &(gCPU.vr[vrA].b[i]));
			x86_mem2(modrmb, &(gCPU.vr[vrB].b[i]));

			helper_gen_vaddubs(reg, reg2, modrma, modrmb);

			asmALU(X86_MOV, reg, modrmb);
			asmALU(X86_SUB, reg, reg2);
			asmALU(X86_ADD, reg, modrma);
			asmALU(X86_OR, reg, reg2);

			vec_setWord(vrD, i, reg);
		}
	}

	return flowContinue;
#endif
}

/*	vaddsbs		Vector Add Signed Byte Saturate
 *	v.138
 */
void ppc_opc_vaddsbs()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint16 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		res = (sint16)gCPU.vr[vrA].sb[i] + (sint16)gCPU.vr[vrB].sb[i];
		gCPU.vr[vrD].b[i] = SATURATE_SB(res);
	}
}
JITCFlow ppc_opc_gen_vaddsbs()
{
	ppc_opc_gen_interpret(ppc_opc_vaddsbs);
	return flowEndBlock;
}

static inline void helper_gen_vadduhs(NativeReg reg, NativeReg reg2, modrm_p modrma, modrm_p modrmb)
{
	asmALU(X86_MOV, reg, modrma);
	asmALU(X86_MOV, reg2, reg);

	asmALU(X86_XOR, reg, modrmb);
	asmALU(X86_AND, reg2, modrmb);

	asmShift(X86_SHR, reg, 1);
	asmALU(X86_AND, reg, 0x7fff7fff);

	asmALU(X86_ADD, reg2, reg);
	asmALU(X86_AND, reg2, 0x80008000);

	asmShift(X86_SHR, reg2, 15);
	asmALU(X86_ADD, reg2, 0x7fff7fff);
	asmALU(X86_XOR, reg2, 0x7fff7fff);
	NativeAddress skip = asmJxxFixup(X86_Z);

	vec_raise_saturate();

	asmResolveFixup(skip);
}

/*	vadduhs		Vector Add Unsigned Half Word Saturate
 *	v.144
 */
void ppc_opc_vadduhs()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint32 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = (uint32)gCPU.vr[vrA].h[i] + (uint32)gCPU.vr[vrB].h[i];
		gCPU.vr[vrD].h[i] = SATURATE_UH(res);
	}
}
JITCFlow ppc_opc_gen_vadduhs()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vadduhs);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrma, modrmb;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (SSE2_AVAIL) {
		commutative_operation(PALUW(X86_PADDUS), vrD, vrA, vrB);
		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg = jitcAllocRegister();
	NativeReg reg2 = jitcAllocRegister();

	if ((vrA == vrD) || (vrB == vrD)) {
		int vrS = vrB;

		if (vrD == vrB) {
			vrS = vrA;
		}

		for (int i=0; i<16; i += 4) {
			x86_mem2(modrma, &(gCPU.vr[vrD].b[i]));
			x86_mem2(modrmb, &(gCPU.vr[vrS].b[i]));

			helper_gen_vadduhs(reg, reg2, modrma, modrmb);

			asmALU(X86_MOV, reg, modrmb);
			asmALU(X86_SUB, reg, reg2);
			asmALU(X86_ADD, modrma, reg);
			asmALU(X86_OR, modrma, reg2);
		}
	} else {
		for (int i=0; i<16; i += 4) {
			x86_mem2(modrma, &(gCPU.vr[vrA].b[i]));
			x86_mem2(modrmb, &(gCPU.vr[vrB].b[i]));

			helper_gen_vadduhs(reg, reg2, modrma, modrmb);

			asmALU(X86_MOV, reg, modrmb);
			asmALU(X86_SUB, reg, reg2);
			asmALU(X86_ADD, reg, modrma);
			asmALU(X86_OR, reg, reg2);

			vec_setWord(vrD, i, reg);
		}
	}

	return flowContinue;

#endif
}

/*	vaddshs		Vector Add Signed Half Word Saturate
 *	v.139
 */
void ppc_opc_vaddshs()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint32 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = (sint32)gCPU.vr[vrA].sh[i] + (sint32)gCPU.vr[vrB].sh[i];
		gCPU.vr[vrD].h[i] = SATURATE_SH(res);
	}
}
JITCFlow ppc_opc_gen_vaddshs()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vaddshs);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (SSE2_AVAIL) {
		commutative_operation(PALUW(X86_PADDS), vrD, vrA, vrB);
		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg = jitcAllocRegister();
	NativeReg reg2 = jitcAllocRegister();

	for (int i=0; i<16; i += 2) {
		vec_getHalfS(reg, vrA, i);
		vec_getHalfS(reg2, vrB, i);

		asmALU(X86_ADD, reg, reg2);
		vec_saturateSH(reg, reg2);

		vec_setHalf(vrD, i, (NativeReg16)reg);
	}

	return flowContinue;
#endif
}

/*	vadduws		Vector Add Unsigned Word Saturate
 *	v.146
 */
void ppc_opc_vadduws()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint32 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = gCPU.vr[vrA].w[i] + gCPU.vr[vrB].w[i];

		// We do this to prevent us from having to do 64-bit math
		if (res < gCPU.vr[vrA].w[i]) {
			res = 0xFFFFFFFF;
			gCPU.vscr |= VSCR_SAT;
		}

	/*	64-bit math		|	32-bit hack
	 *	------------------------+-------------------------------------
	 *	add, addc	(a+b)	|	add		(a+b)
	 *	sub, subb 	(r>ub)	|	sub		(r<a)
	 */

		gCPU.vr[vrD].w[i] = res;
	}
}
JITCFlow ppc_opc_gen_vadduws()
{
	ppc_opc_gen_interpret(ppc_opc_vadduws);
	return flowEndBlock;
}

/*	vaddsws		Vector Add Signed Word Saturate
 *	v.140
 */
void ppc_opc_vaddsws()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint32 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = gCPU.vr[vrA].w[i] + gCPU.vr[vrB].w[i];

		// We do this to prevent us from having to do 64-bit math
		if (((gCPU.vr[vrA].w[i] ^ gCPU.vr[vrB].w[i]) & SIGN32) == 0) {
			// the signs of both operands are the same

			if (((res ^ gCPU.vr[vrA].w[i]) & SIGN32) != 0) {
				// sign of result != sign of operands

				// if res is negative, should have been positive
				res = (res & SIGN32) ? (SIGN32 - 1) : SIGN32;
				gCPU.vscr |= VSCR_SAT;
			}
		}

	/*	64-bit math		|	32-bit hack
	 *	------------------------+-------------------------------------
	 *	add, addc	(a+b)	|	add		(a+b)
	 *	sub, subb 	(r>ub)	|	xor, and	(sign == sign)
	 *	sub, subb	(r<lb)	|	xor, and	(sign != sign)
	 *				|	and		(which)
	 */

		gCPU.vr[vrD].w[i] = res;
	}
}
JITCFlow ppc_opc_gen_vaddsws()
{
	ppc_opc_gen_interpret(ppc_opc_vaddsws);
	return flowEndBlock;
}

static inline void helper_gen_vsububm(NativeReg reg, NativeReg reg2, modrm_p modrma, modrm_p modrmb)
{
	asmALU(X86_MOV, reg, modrma);
	asmALU(X86_NOT, reg);
	asmALU(X86_MOV, reg2, reg);

	asmALU(X86_AND, reg, modrmb);
	asmALU(X86_XOR, reg2, modrmb);

	asmShift(X86_SHL, reg, 1);
	asmALU(X86_AND, reg2, 0xfefefefe);

	asmALU(X86_ADD, reg2, reg);
	asmALU(X86_AND, reg2, 0x01010100);
}

/*	vsububm		Vector Subtract Unsigned Byte Modulo
 *	v.265
 */
void ppc_opc_vsububm()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint8 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		res = gCPU.vr[vrA].b[i] - gCPU.vr[vrB].b[i];
		gCPU.vr[vrD].b[i] = res;
	}
}
JITCFlow ppc_opc_gen_vsububm()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vsububm);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrma, modrmb;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (SSE2_AVAIL) {
		noncommutative_operation(PALUB(X86_PSUB), vrD, vrA, vrB);
		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg = jitcAllocRegister();
	NativeReg reg2 = jitcAllocRegister();

	if (vrA == vrD) {
		for (int i=0; i<16; i += 4) {
			x86_mem2(modrma, &(gCPU.vr[vrA].b[i]));
			x86_mem2(modrmb, &(gCPU.vr[vrB].b[i]));

			helper_gen_vsububm(reg, reg2, modrma, modrmb);

			asmALU(X86_SUB, reg2, modrmb);
			asmALU(X86_ADD, modrma, reg2);
		}
	} else {
		for (int i=0; i<16; i += 4) {
			x86_mem2(modrma, &(gCPU.vr[vrA].b[i]));
			x86_mem2(modrmb, &(gCPU.vr[vrB].b[i]));

			helper_gen_vsububm(reg, reg2, modrma, modrmb);

			asmALU(X86_SUB, reg2, modrmb);
			asmALU(X86_ADD, reg2, modrma);

			vec_setWord(vrD, i, reg2);
		}
	}

	return flowContinue;
#endif
}

static inline void helper_gen_vsubuhm(NativeReg reg, NativeReg reg2, modrm_p modrma, modrm_p modrmb)
{
	asmALU(X86_MOV, reg, modrma);
	asmALU(X86_NOT, reg);
	asmALU(X86_MOV, reg2, reg);

	asmALU(X86_AND, reg, modrmb);
	asmALU(X86_XOR, reg2, modrmb);

	asmShift(X86_SHL, reg, 1);
	asmALU(X86_AND, reg2, 0xfffefffe);

	asmALU(X86_ADD, reg2, reg);
	asmALU(X86_AND, reg2, 0x00010000);
}


/*	vsubuhm		Vector Subtract Unsigned Half Word Modulo
 *	v.267
 */
void ppc_opc_vsubuhm()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint16 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = gCPU.vr[vrA].h[i] - gCPU.vr[vrB].h[i];
		gCPU.vr[vrD].h[i] = res;
	}
}
JITCFlow ppc_opc_gen_vsubuhm()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vsubuhm);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrma, modrmb;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (SSE2_AVAIL) {
		noncommutative_operation(PALUW(X86_PSUB), vrD, vrA, vrB);
		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	NativeReg reg = jitcAllocRegister();
	NativeReg reg2 = jitcAllocRegister();

	if (vrA == vrD) {
		for (int i=0; i<16; i += 4) {
			x86_mem2(modrma, &(gCPU.vr[vrA].b[i]));
			x86_mem2(modrmb, &(gCPU.vr[vrB].b[i]));

			helper_gen_vsubuhm(reg, reg2, modrma, modrmb);

			asmALU(X86_SUB, reg2, modrmb);
			asmALU(X86_ADD, modrma, reg2);
		}
	} else {
		for (int i=0; i<16; i += 4) {
			x86_mem2(modrma, &(gCPU.vr[vrA].b[i]));
			x86_mem2(modrmb, &(gCPU.vr[vrB].b[i]));

			helper_gen_vsubuhm(reg, reg2, modrma, modrmb);

			asmALU(X86_SUB, reg2, modrmb);
			asmALU(X86_ADD, reg2, modrma);

			vec_setWord(vrD, i, reg2);
		}
	}

	return flowContinue;
#endif
}

/*	vsubuwm		Vector Subtract Unsigned Word Modulo
 *	v.269
 */
void ppc_opc_vsubuwm()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint32 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = gCPU.vr[vrA].w[i] - gCPU.vr[vrB].w[i];
		gCPU.vr[vrD].w[i] = res;
	}
}
JITCFlow ppc_opc_gen_vsubuwm()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vsubuwm);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (SSE2_AVAIL) {
		noncommutative_operation(PALUD(X86_PSUB), vrD, vrA, vrB);
		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);
	NativeReg reg = jitcAllocRegister();

	if (vrA == vrD) {
		for (int i=0; i<16; i += 4) {
			vec_getWord(reg, vrB, i);

			asmALU(X86_SUB,
				x86_mem2(modrm, &(gCPU.vr[vrD].b[i])), reg);
		}
	} else {
		for (int i=0; i<16; i += 4) {
			vec_getWord(reg, vrA, i);

			asmALU(X86_SUB, reg,
				x86_mem2(modrm, &(gCPU.vr[vrB].b[i])));

			vec_setWord(vrD, i, reg);
		}
	}

	return flowContinue;
#endif
}

/*	vsubfp		Vector Subtract Float Point
 *	v.261
 */
void ppc_opc_vsubfp()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	float res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		res = gCPU.vr[vrA].f[i] - gCPU.vr[vrB].f[i];
		gCPU.vr[vrD].f[i] = res;
	}
}
JITCFlow ppc_opc_gen_vsubfp()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vsubfp);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (SSE_AVAIL) {
		noncommutative_operation(X86_SUBPS, vrD, vrA, vrB);
		return flowContinue;
	}

	ppc_opc_gen_interpret(ppc_opc_vsubfp);
	return flowEndBlock;
#endif
}

/*	vsubcuw		Vector Subtract Carryout Unsigned Word
 *	v.260
 */
void ppc_opc_vsubcuw()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint32 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = gCPU.vr[vrA].w[i] - gCPU.vr[vrB].w[i];
		gCPU.vr[vrD].w[i] = (res <= gCPU.vr[vrA].w[i]) ? 1 : 0;
	}
}
JITCFlow ppc_opc_gen_vsubcuw()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vsubcuw);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (vrA == vrB) {
		// no need to flush vrA, vrB, their values are irrelevant here
		jitcDropClientVectorRegister(vrD);

		NativeReg reg = jitcAllocRegister();
		asmMOV_NoFlags(reg, 1);	// avoid flag clobber

		vec_setWord(vrD, 0, reg);
		vec_setWord(vrD, 4, reg);
		vec_setWord(vrD, 8, reg);
		vec_setWord(vrD,12, reg);
	} else {
		jitcFlushClientVectorRegister(vrA);
		jitcFlushClientVectorRegister(vrB);
		jitcFlushClientVectorRegister(vrD);

		NativeReg reg = jitcAllocRegister(NATIVE_REG_8);

		for (int i=0; i<16; i += 4) {
			vec_getWord(reg, vrA, i);

			asmALU(X86_SUB, reg, 
				x86_mem2(modrm, &(gCPU.vr[vrB].b[i])));

			asmSET(X86_NC, (NativeReg8)reg);

			asmMOVxx(X86_MOVZX, reg, (NativeReg8)reg);

			vec_setWord(vrD, i, reg);
		}
	}

	return flowContinue;
#endif
}

static inline void helper_gen_vsububs(NativeReg reg, NativeReg reg2, modrm_p modrma, modrm_p modrmb)
{
	asmALU(X86_MOV, reg, modrma);
	asmALU(X86_NOT, reg);
	asmALU(X86_MOV, reg2, reg);

	asmALU(X86_XOR, reg, modrmb);
	asmALU(X86_AND, reg2, modrmb);

	asmShift(X86_SHR, reg, 1);
	asmALU(X86_AND, reg, 0x7f7f7f7f);

	asmALU(X86_ADD, reg2, reg);
	asmALU(X86_AND, reg2, 0x80808080);

	asmShift(X86_SHR, reg2, 7);
	asmALU(X86_ADD, reg2, 0x7f7f7f7f);
	asmALU(X86_XOR, reg2, 0x7f7f7f7f);
	NativeAddress skip = asmJxxFixup(X86_Z);

	vec_raise_saturate();

	asmResolveFixup(skip);
}

/*	vsububs		Vector Subtract Unsigned Byte Saturate
 *	v.266
 */
void ppc_opc_vsububs()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint16 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		res = (uint16)gCPU.vr[vrA].b[i] - (uint16)gCPU.vr[vrB].b[i];

		gCPU.vr[vrD].b[i] = SATURATE_0B(res);
	}
}
JITCFlow ppc_opc_gen_vsububs()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vsububs);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrma, modrmb;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (SSE2_AVAIL) {
		noncommutative_operation(PALUB(X86_PSUBUS), vrD, vrA, vrB);
		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg = jitcAllocRegister();
	NativeReg reg2 = jitcAllocRegister();

	if (vrA == vrD) {
		for (int i=0; i<16; i += 4) {
			x86_mem2(modrma, &(gCPU.vr[vrD].b[i]));
			x86_mem2(modrmb, &(gCPU.vr[vrB].b[i]));

			helper_gen_vsububs(reg, reg2, modrma, modrmb);

			asmALU(X86_MOV, reg, modrmb);
			asmALU(X86_OR, reg, reg2);

			asmALU(X86_OR, modrma, reg2);
			asmALU(X86_SUB, modrma, reg);
		}
	} else {
		for (int i=0; i<16; i += 4) {
			x86_mem2(modrma, &(gCPU.vr[vrA].b[i]));
			x86_mem2(modrmb, &(gCPU.vr[vrB].b[i]));

			helper_gen_vsububs(reg, reg2, modrma, modrmb);

			asmALU(X86_MOV, reg, modrmb);
			asmALU(X86_OR, reg, reg2);

			asmALU(X86_OR, reg2, modrma);
			asmALU(X86_SUB, reg2, reg);

			vec_setWord(vrD, i, reg2);
		}
	}

	return flowContinue;
#endif
}

/*	vsubsbs		Vector Subtract Signed Byte Saturate
 *	v.262
 */
void ppc_opc_vsubsbs()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint16 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		res = (sint16)gCPU.vr[vrA].sb[i] - (sint16)gCPU.vr[vrB].sb[i];

		gCPU.vr[vrD].sb[i] = SATURATE_SB(res);
	}
}
JITCFlow ppc_opc_gen_vsubsbs()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vsubsbs);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (SSE2_AVAIL) {
		noncommutative_operation(PALUB(X86_PSUBS), vrD, vrA, vrB);
		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg = jitcAllocRegister(NATIVE_REG_8);
	NativeReg reg2 = jitcAllocRegister();

	for (int i=0; i<16; i++) {
		vec_getByteS(reg, vrA, i);
		vec_getByteS(reg2, vrB, i);

		asmALU(X86_SUB, reg, reg2);
		vec_saturateSB(reg, reg2);

		vec_setByte(vrD, REG_NO, i, (NativeReg8)reg);
	}

	return flowContinue;
#endif
}

static inline void helper_gen_vsubuhs(NativeReg reg, NativeReg reg2, modrm_p modrma, modrm_p modrmb)
{
	asmALU(X86_MOV, reg, modrma);
	asmALU(X86_NOT, reg);
	asmALU(X86_MOV, reg2, reg);

	asmALU(X86_XOR, reg, modrmb);
	asmALU(X86_AND, reg2, modrmb);

	asmShift(X86_SHR, reg, 1);
	asmALU(X86_AND, reg, 0x7fff7fff);

	asmALU(X86_ADD, reg2, reg);
	asmALU(X86_AND, reg2, 0x80008000);

	asmShift(X86_SHR, reg2, 15);
	asmALU(X86_ADD, reg2, 0x7fff7fff);
	asmALU(X86_XOR, reg2, 0x7fff7fff);
	NativeAddress skip = asmJxxFixup(X86_Z);

	vec_raise_saturate();

	asmResolveFixup(skip);
}

/*	vsubuhs		Vector Subtract Unsigned Half Word Saturate
 *	v.268
 */
void ppc_opc_vsubuhs()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint32 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = (uint32)gCPU.vr[vrA].h[i] - (uint32)gCPU.vr[vrB].h[i];

		gCPU.vr[vrD].h[i] = SATURATE_0H(res);
	}
}
JITCFlow ppc_opc_gen_vsubuhs()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vsubuhs);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrma, modrmb;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (SSE2_AVAIL) {
		noncommutative_operation(PALUW(X86_PSUBUS), vrD, vrA, vrB);
		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg = jitcAllocRegister();
	NativeReg reg2 = jitcAllocRegister();

	if (vrA == vrD) {
		for (int i=0; i<16; i += 4) {
			x86_mem2(modrma, &(gCPU.vr[vrD].b[i]));
			x86_mem2(modrmb, &(gCPU.vr[vrB].b[i]));

			helper_gen_vsubuhs(reg, reg2, modrma, modrmb);

			asmALU(X86_MOV, reg, modrmb);
			asmALU(X86_OR, reg, reg2);

			asmALU(X86_OR, modrma, reg2);
			asmALU(X86_SUB, modrma, reg);
		}
	} else {
		for (int i=0; i<16; i += 4) {
			x86_mem2(modrma, &(gCPU.vr[vrA].b[i]));
			x86_mem2(modrmb, &(gCPU.vr[vrB].b[i]));

			helper_gen_vsubuhs(reg, reg2, modrma, modrmb);

			asmALU(X86_MOV, reg, modrmb);
			asmALU(X86_OR, reg, reg2);

			asmALU(X86_OR, reg2, modrma);
			asmALU(X86_SUB, reg2, reg);

			vec_setWord(vrD, i, reg2);
		}
	}

	return flowContinue;
#endif
}

/*	vsubshs		Vector Subtract Signed Half Word Saturate
 *	v.263
 */
void ppc_opc_vsubshs()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint32 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = (sint32)gCPU.vr[vrA].sh[i] - (sint32)gCPU.vr[vrB].sh[i];

		gCPU.vr[vrD].sh[i] = SATURATE_SH(res);
	}
}
JITCFlow ppc_opc_gen_vsubshs()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vsubshs);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (SSE2_AVAIL) {
		noncommutative_operation(PALUW(X86_PSUBS), vrD, vrA, vrB);
		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg = jitcAllocRegister();
	NativeReg reg2 = jitcAllocRegister();

	for (int i=0; i<16; i += 2) {
		vec_getHalfS(reg, vrA, i);
		vec_getHalfS(reg2, vrB, i);

		asmALU(X86_SUB, reg, reg2);
		vec_saturateSH(reg, reg2);

		vec_setHalf(vrD, i, (NativeReg16)reg);
	}

	return flowContinue;
#endif
}

/*	vsubuws		Vector Subtract Unsigned Word Saturate
 *	v.270
 */
void ppc_opc_vsubuws()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint32 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = gCPU.vr[vrA].w[i] - gCPU.vr[vrB].w[i];

		// We do this to prevent us from having to do 64-bit math
		if (res > gCPU.vr[vrA].w[i]) {
			res = 0;
			gCPU.vscr |= VSCR_SAT;
		}

	/*	64-bit math		|	32-bit hack
	 *	------------------------+-------------------------------------
	 *	sub, subb	(a+b)	|	sub		(a+b)
	 *	sub, subb 	(r>ub)	|	sub		(r<a)
	 */

		gCPU.vr[vrD].w[i] = res;
	}
}
JITCFlow ppc_opc_gen_vsubuws()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vsubuws);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg = jitcAllocRegister();

	for (int i=0; i<16; i += 4) {
		vec_getWord(reg, vrA, i);

		asmALU(X86_SUB, reg,
			x86_mem2(modrm, &(gCPU.vr[vrB].b[i])));

		NativeAddress skip = asmJxxFixup(X86_NC);

		asmALU(X86_XOR, reg, reg);

		asmResolveFixup(skip);

		vec_setWord(vrD, i, reg);
	}

	return flowContinue;
#endif
}

/*	vsubsws		Vector Subtract Signed Word Saturate
 *	v.264
 */
void ppc_opc_vsubsws()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint32 res, tmp;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		tmp = -gCPU.vr[vrB].w[i];
		res = gCPU.vr[vrA].w[i] + tmp;

		// We do this to prevent us from having to do 64-bit math
		if (((gCPU.vr[vrA].w[i] ^ tmp) & SIGN32) == 0) {
			// the signs of both operands are the same

			if (((res ^ tmp) & SIGN32) != 0) {
				// sign of result != sign of operands

				// if res is negative, should have been positive
				res = (res & SIGN32) ? (SIGN32 - 1) : SIGN32;
				gCPU.vscr |= VSCR_SAT;
			}
		}

	/*	64-bit math		|	32-bit hack
	 *	------------------------+-------------------------------------
	 *	sub, subc	(a+b)	|	neg, add	(a-b)
	 *	sub, subb 	(r>ub)	|	xor, and	(sign == sign)
	 *	sub, subb	(r<lb)	|	xor, and	(sign != sign)
	 *				|	and		(which)
	 */

		gCPU.vr[vrD].w[i] = res;
	}
}
JITCFlow ppc_opc_gen_vsubsws()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vsubsws);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg = jitcAllocRegister();

	for (int i=0; i<16; i += 4) {
		vec_getWord(reg, vrA, i);

		asmALU(X86_SUB, reg,
			x86_mem2(modrm, &(gCPU.vr[vrB].b[i])));

		NativeAddress skip = asmJxxFixup(X86_NO);

		asmMOV_NoFlags(reg, 0x80000000);

		asmResolveFixup(skip);

		vec_setWord(vrD, i, reg);
	}

	return flowContinue;
#endif
}

/*	vmuleub		Vector Multiply Even Unsigned Byte
 *	v.209
 */
void ppc_opc_vmuleub()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint16 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = (uint16)gCPU.vr[vrA].b[VECT_EVEN(i)] *
			 (uint16)gCPU.vr[vrB].b[VECT_EVEN(i)];

		gCPU.vr[vrD].h[i] = res;
	}
}
JITCFlow ppc_opc_gen_vmuleub()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vmuleub);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg1 = jitcAllocRegister();
	NativeReg reg2 = jitcAllocRegister();

	for (int i=0; i<16; i+=2) {
		vec_getByteZ(reg1, vrA, i+1);
		vec_getByteZ(reg2, vrB, i+1);

		asmIMUL(reg1, reg2);

		vec_setHalf(vrD, i, (NativeReg16)reg1);
	}

	return flowContinue;
#endif
}

/*	vmulesb		Vector Multiply Even Signed Byte
 *	v.207
 */
void ppc_opc_vmulesb()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint16 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = (sint16)gCPU.vr[vrA].sb[VECT_EVEN(i)] *
			 (sint16)gCPU.vr[vrB].sb[VECT_EVEN(i)];

		gCPU.vr[vrD].sh[i] = res;
	}
}
JITCFlow ppc_opc_gen_vmulesb()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vmulesb);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg1 = jitcAllocRegister();
	NativeReg reg2 = jitcAllocRegister();

	for (int i=0; i<16; i+=2) {
		vec_getByteS(reg1, vrA, i+1);
		vec_getByteS(reg2, vrB, i+1);

		asmIMUL(reg1, reg2);

		vec_setHalf(vrD, i, (NativeReg16)reg1);
	}

	return flowContinue;
#endif
}

/*	vmuleuh		Vector Multiply Even Unsigned Half Word
 *	v.210
 */
void ppc_opc_vmuleuh()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint32 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = (uint32)gCPU.vr[vrA].h[VECT_EVEN(i)] *
			 (uint32)gCPU.vr[vrB].h[VECT_EVEN(i)];

		gCPU.vr[vrD].w[i] = res;
	}
}
JITCFlow ppc_opc_gen_vmuleuh()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vmuleuh);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg1 = jitcAllocRegister();
	NativeReg reg2 = jitcAllocRegister();

	for (int i=0; i<16; i+=4) {
		vec_getHalfZ(reg1, vrA, i+2);
		vec_getHalfZ(reg2, vrB, i+2);

		asmIMUL(reg1, reg2);

		vec_setWord(vrD, i, reg1);
	}

	return flowContinue;
#endif
}

/*	vmulesh		Vector Multiply Even Signed Half Word
 *	v.208
 */
void ppc_opc_vmulesh()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint32 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = (sint32)gCPU.vr[vrA].sh[VECT_EVEN(i)] *
			 (sint32)gCPU.vr[vrB].sh[VECT_EVEN(i)];

		gCPU.vr[vrD].sw[i] = res;
	}
}
JITCFlow ppc_opc_gen_vmulesh()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vmulesh);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg1 = jitcAllocRegister();
	NativeReg reg2 = jitcAllocRegister();

	for (int i=0; i<16; i+=4) {
		vec_getHalfS(reg1, vrA, i+2);
		vec_getHalfS(reg2, vrB, i+2);

		asmIMUL(reg1, reg2);

		vec_setWord(vrD, i, reg1);
	}

	return flowContinue;
#endif
}

/*	vmuloub		Vector Multiply Odd Unsigned Byte
 *	v.213
 */
void ppc_opc_vmuloub()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint16 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = (uint16)gCPU.vr[vrA].b[VECT_ODD(i)] *
			 (uint16)gCPU.vr[vrB].b[VECT_ODD(i)];

		gCPU.vr[vrD].h[i] = res;
	}
}
JITCFlow ppc_opc_gen_vmuloub()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vmuloub);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg1 = jitcAllocRegister();
	NativeReg reg2 = jitcAllocRegister();

	for (int i=0; i<16; i+=2) {
		vec_getByteZ(reg1, vrA, i);
		vec_getByteZ(reg2, vrB, i);

		asmIMUL(reg1, reg2);

		vec_setHalf(vrD, i, (NativeReg16)reg1);
	}

	return flowContinue;
#endif
}

/*	vmulosb		Vector Multiply Odd Signed Byte
 *	v.211
 */
void ppc_opc_vmulosb()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint16 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = (sint16)gCPU.vr[vrA].sb[VECT_ODD(i)] *
			 (sint16)gCPU.vr[vrB].sb[VECT_ODD(i)];

		gCPU.vr[vrD].sh[i] = res;
	}
}
JITCFlow ppc_opc_gen_vmulosb()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vmulosb);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg1 = jitcAllocRegister();
	NativeReg reg2 = jitcAllocRegister();

	for (int i=0; i<16; i+=2) {
		vec_getByteS(reg1, vrA, i);
		vec_getByteS(reg2, vrB, i);

		asmIMUL(reg1, reg2);

		vec_setHalf(vrD, i, (NativeReg16)reg1);
	}

	return flowContinue;
#endif
}

/*	vmulouh		Vector Multiply Odd Unsigned Half Word
 *	v.214
 */
void ppc_opc_vmulouh()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint32 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = (uint32)gCPU.vr[vrA].h[VECT_ODD(i)] *
			 (uint32)gCPU.vr[vrB].h[VECT_ODD(i)];

		gCPU.vr[vrD].w[i] = res;
	}
}
JITCFlow ppc_opc_gen_vmulouh()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vmulouh);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg1 = jitcAllocRegister();
	NativeReg reg2 = jitcAllocRegister();

	for (int i=0; i<16; i+=4) {
		vec_getHalfZ(reg1, vrA, i);
		vec_getHalfZ(reg2, vrB, i);

		asmIMUL(reg1, reg2);

		vec_setWord(vrD, i, reg1);
	}

	return flowContinue;
#endif
}

/*	vmulosh		Vector Multiply Odd Signed Half Word
 *	v.212
 */
void ppc_opc_vmulosh()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint32 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = (sint32)gCPU.vr[vrA].sh[VECT_ODD(i)] *
			 (sint32)gCPU.vr[vrB].sh[VECT_ODD(i)];

		gCPU.vr[vrD].sw[i] = res;
	}
}
JITCFlow ppc_opc_gen_vmulosh()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vmulosh);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg1 = jitcAllocRegister();
	NativeReg reg2 = jitcAllocRegister();

	for (int i=0; i<16; i+=4) {
		vec_getHalfS(reg1, vrA, i);
		vec_getHalfS(reg2, vrB, i);

		asmIMUL(reg1, reg2);

		vec_setWord(vrD, i, reg1);
	}

	return flowContinue;
#endif
}

/*	vmaddfp		Vector Multiply Add Floating Point
 *	v.177
 */
void ppc_opc_vmaddfp()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB, vrC;
	double res;
	PPC_OPC_TEMPL_A(gCPU.current_opc, vrD, vrA, vrB, vrC);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		res = (double)gCPU.vr[vrA].f[i] * (double)gCPU.vr[vrC].f[i];

		res = (double)gCPU.vr[vrB].f[i] + res;

		gCPU.vr[vrD].f[i] = (float)res;
	}
}
JITCFlow ppc_opc_gen_vmaddfp()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vmaddfp);
	return flowEndBlock;
#else
	int vrD, vrA, vrB, vrC;
	PPC_OPC_TEMPL_A(gJITC.current_opc, vrD, vrA, vrB, vrC);

	if (SSE_AVAIL) {
		commutative_operation(X86_MULPS, vrT, vrA, vrC);
		commutative_operation(X86_ADDPS, vrD, vrB, vrT);
		return flowContinue;
	}

	ppc_opc_gen_interpret(ppc_opc_vmaddfp);
	return flowEndBlock;
#endif
}

/*	vmhaddshs	Vector Multiply High and Add Signed Half Word Saturate
 *	v.185
 */
void ppc_opc_vmhaddshs()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB, vrC;
	sint32 prod;
	PPC_OPC_TEMPL_A(gCPU.current_opc, vrD, vrA, vrB, vrC);

	for (int i=0; i<8; i++) {
		prod = (sint32)gCPU.vr[vrA].sh[i] * (sint32)gCPU.vr[vrB].sh[i];

		prod = (prod >> 15) + (sint32)gCPU.vr[vrC].sh[i];

		gCPU.vr[vrD].sh[i] = SATURATE_SH(prod);
	}
}
JITCFlow ppc_opc_gen_vmhaddshs()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vmhaddshs);
	return flowEndBlock;
#else
	int vrD, vrA, vrB, vrC;
	PPC_OPC_TEMPL_A(gJITC.current_opc, vrD, vrA, vrB, vrC);

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcFlushClientVectorRegister(vrC);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg1 = jitcAllocRegister();
	NativeReg reg2 = jitcAllocRegister();

	for (int i=0; i<16; i+=2) {
		vec_getHalfS(reg1, vrA, i);
		vec_getHalfS(reg2, vrB, i);

		asmIMUL(reg1, reg2);
		asmShift(X86_SAR, reg1, 15);

		vec_getHalfS(reg2, vrC, i);
		asmALU(X86_ADD, reg1, reg2);

		vec_saturateSH(reg1, reg2);

		vec_setHalf(vrD, i, (NativeReg16)reg1);
	}

	return flowContinue;
#endif
}

/*	vmladduhm	Vector Multiply Low and Add Unsigned Half Word Modulo
 *	v.194
 */
void ppc_opc_vmladduhm()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB, vrC;
	uint32 prod;
	PPC_OPC_TEMPL_A(gCPU.current_opc, vrD, vrA, vrB, vrC);

	for (int i=0; i<8; i++) {
		prod = (uint32)gCPU.vr[vrA].h[i] * (uint32)gCPU.vr[vrB].h[i];

		prod = prod + (uint32)gCPU.vr[vrC].h[i];

		gCPU.vr[vrD].h[i] = prod;
	}
}
JITCFlow ppc_opc_gen_vmladduhm()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vmladduhm);
	return flowEndBlock;
#else
	int vrD, vrA, vrB, vrC;
	modrm_o modrm;
	PPC_OPC_TEMPL_A(gJITC.current_opc, vrD, vrA, vrB, vrC);

	if (SSE2_AVAIL) {
		commutative_operation(X86_PMULLW, vrT, vrA, vrB);
		commutative_operation(PALUW(X86_PADD), vrD, vrT, vrC);
		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcFlushClientVectorRegister(vrC);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg16 reg1 = (NativeReg16)jitcAllocRegister();
	NativeReg reg2 = jitcAllocRegister();

	for (int i=0; i<16; i+=2) {
		vec_getHalfZ((NativeReg)reg1, vrA, i);
		vec_getHalfZ(reg2, vrB, i);

		asmIMUL((NativeReg)reg1, reg2);

		if (vrC == vrD) {
			asmALU(X86_ADD,
				x86_mem2(modrm, &(gCPU.vr[vrD].b[i])), reg1);
		} else {
			asmALU(X86_ADD, reg1,
				x86_mem2(modrm, &(gCPU.vr[vrC].b[i])));

			vec_setHalf(vrD, i, reg1);
		}
	}

	return flowContinue;
#endif
}

/*	vmhraddshs	Vector Multiply High Round and Add Signed Half Word Saturate
 *	v.186
 */
void ppc_opc_vmhraddshs()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB, vrC;
	sint32 prod;
	PPC_OPC_TEMPL_A(gCPU.current_opc, vrD, vrA, vrB, vrC);

	for (int i=0; i<8; i++) {
		prod = (sint32)gCPU.vr[vrA].sh[i] * (sint32)gCPU.vr[vrB].sh[i];

		prod += 0x4000;
		prod = (prod >> 15) + (sint32)gCPU.vr[vrC].sh[i];

		gCPU.vr[vrD].sh[i] = SATURATE_SH(prod);
	}
}
JITCFlow ppc_opc_gen_vmhraddshs()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vmhraddshs);
	return flowEndBlock;
#else
	int vrD, vrA, vrB, vrC;
	PPC_OPC_TEMPL_A(gJITC.current_opc, vrD, vrA, vrB, vrC);

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcFlushClientVectorRegister(vrC);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg1 = jitcAllocRegister();
	NativeReg reg2 = jitcAllocRegister();

	for (int i=0; i<16; i+=2) {
		vec_getHalfS(reg1, vrA, i);
		vec_getHalfS(reg2, vrB, i);

		asmIMUL(reg1, reg2);
		asmALU(X86_ADD, reg1, 0x4000);
		asmShift(X86_SAR, reg1, 15);

		vec_getHalfS(reg2, vrC, i);
		asmALU(X86_ADD, reg1, reg2);

		vec_saturateSH(reg1, reg2);

		vec_setHalf(vrD, i, (NativeReg16)reg1);
	}

	return flowContinue;
#endif
}

/*	vmsumubm	Vector Multiply Sum Unsigned Byte Modulo
 *	v.204
 */
void ppc_opc_vmsumubm()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB, vrC;
	uint32 temp;
	PPC_OPC_TEMPL_A(gCPU.current_opc, vrD, vrA, vrB, vrC);

	for (int i=0; i<4; i++) {
		temp = gCPU.vr[vrC].w[i];

		temp += (uint16)gCPU.vr[vrA].b[i<<2] *
			(uint16)gCPU.vr[vrB].b[i<<2];

		temp += (uint16)gCPU.vr[vrA].b[(i<<2)+1] *
			(uint16)gCPU.vr[vrB].b[(i<<2)+1];

		temp += (uint16)gCPU.vr[vrA].b[(i<<2)+2] *
			(uint16)gCPU.vr[vrB].b[(i<<2)+2];

		temp += (uint16)gCPU.vr[vrA].b[(i<<2)+3] *
			(uint16)gCPU.vr[vrB].b[(i<<2)+3];

		gCPU.vr[vrD].w[i] = temp;
	}
}
JITCFlow ppc_opc_gen_vmsumubm()
{
	ppc_opc_gen_interpret(ppc_opc_vmsumubm);
	return flowEndBlock;
}

/*	vmsumuhm	Vector Multiply Sum Unsigned Half Word Modulo
 *	v.205
 */
void ppc_opc_vmsumuhm()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB, vrC;
	uint32 temp;
	PPC_OPC_TEMPL_A(gCPU.current_opc, vrD, vrA, vrB, vrC);

	for (int i=0; i<4; i++) {
		temp = gCPU.vr[vrC].w[i];

		temp += (uint32)gCPU.vr[vrA].h[i<<1] *
			(uint32)gCPU.vr[vrB].h[i<<1];
		temp += (uint32)gCPU.vr[vrA].h[(i<<1)+1] *
			(uint32)gCPU.vr[vrB].h[(i<<1)+1];

		gCPU.vr[vrD].w[i] = temp;
	}
}
JITCFlow ppc_opc_gen_vmsumuhm()
{
	ppc_opc_gen_interpret(ppc_opc_vmsumuhm);
	return flowEndBlock;
}

/*	vmsummbm	Vector Multiply Sum Mixed-Sign Byte Modulo
 *	v.201
 */
void ppc_opc_vmsummbm()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB, vrC;
	sint32 temp;
	PPC_OPC_TEMPL_A(gCPU.current_opc, vrD, vrA, vrB, vrC);

	for (int i=0; i<4; i++) {
		temp = gCPU.vr[vrC].sw[i];

		temp += (sint16)gCPU.vr[vrA].sb[i<<2] *
			(uint16)gCPU.vr[vrB].b[i<<2];
		temp += (sint16)gCPU.vr[vrA].sb[(i<<2)+1] *
			(uint16)gCPU.vr[vrB].b[(i<<2)+1];
		temp += (sint16)gCPU.vr[vrA].sb[(i<<2)+2] *
			(uint16)gCPU.vr[vrB].b[(i<<2)+2];
		temp += (sint16)gCPU.vr[vrA].sb[(i<<2)+3] *
			(uint16)gCPU.vr[vrB].b[(i<<2)+3];

		gCPU.vr[vrD].sw[i] = temp;
	}
}
JITCFlow ppc_opc_gen_vmsummbm()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vmsummbm);
	return flowEndBlock;
#else
	int vrD, vrA, vrB, vrC;
	PPC_OPC_TEMPL_A(gJITC.current_opc, vrD, vrA, vrB, vrC);

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcFlushClientVectorRegister(vrC);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg1 = jitcAllocRegister();
	NativeReg reg2 = jitcAllocRegister();
	NativeReg reg3 = jitcAllocRegister();

	for (int i=0; i<16; i+=4) {
		vec_getWord(reg1, vrC, i);

		for (int j=0; j<4; j++) {
			vec_getByteS(reg2, vrA, i+j);
			vec_getByteZ(reg3, vrB, i+j);

			asmIMUL(reg2, reg3);
			asmALU(X86_ADD, reg1, reg2);
		}

		vec_setWord(vrD, i, reg1);
	}

	return flowContinue;
#endif
}

/*	vmsumshm	Vector Multiply Sum Signed Half Word Modulo
 *	v.202
 */
void ppc_opc_vmsumshm()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB, vrC;
	sint32 temp;
	PPC_OPC_TEMPL_A(gCPU.current_opc, vrD, vrA, vrB, vrC);

	for (int i=0; i<4; i++) {
		temp = gCPU.vr[vrC].sw[i];

		temp += (sint32)gCPU.vr[vrA].sh[i<<1] *
			(sint32)gCPU.vr[vrB].sh[i<<1];
		temp += (sint32)gCPU.vr[vrA].sh[(i<<1)+1] *
			(sint32)gCPU.vr[vrB].sh[(i<<1)+1];

		gCPU.vr[vrD].sw[i] = temp;
	}
}
JITCFlow ppc_opc_gen_vmsumshm()
{
	ppc_opc_gen_interpret(ppc_opc_vmsumshm);
	return flowEndBlock;
}

/*	vmsumuhs	Vector Multiply Sum Unsigned Half Word Saturate
 *	v.206
 */
void ppc_opc_vmsumuhs()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB, vrC;
	uint64 temp;
	PPC_OPC_TEMPL_A(gCPU.current_opc, vrD, vrA, vrB, vrC);

	/* For this, there's no way to get around 64-bit math.  If we use
	 *   the hacks used before, then we have to do it so often, that
	 *   we'll outpace the 64-bit math in execution time.
	 */
	for (int i=0; i<4; i++) {
		temp = gCPU.vr[vrC].w[i];

		temp += (uint32)gCPU.vr[vrA].h[i<<1] *
			(uint32)gCPU.vr[vrB].h[i<<1];

		temp += (uint32)gCPU.vr[vrA].h[(i<<1)+1] *
			(uint32)gCPU.vr[vrB].h[(i<<1)+1];

		gCPU.vr[vrD].w[i] = SATURATE_UW(temp);
	}
}
JITCFlow ppc_opc_gen_vmsumuhs()
{
	ppc_opc_gen_interpret(ppc_opc_vmsumuhs);
	return flowEndBlock;
}

/*	vmsumshs	Vector Multiply Sum Signed Half Word Saturate
 *	v.203
 */
void ppc_opc_vmsumshs()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB, vrC;
	sint64 temp;
	PPC_OPC_TEMPL_A(gCPU.current_opc, vrD, vrA, vrB, vrC);

	/* For this, there's no way to get around 64-bit math.  If we use
	 *   the hacks used before, then we have to do it so often, that
	 *   we'll outpace the 64-bit math in execution time.
	 */

	for (int i=0; i<4; i++) {
		temp = gCPU.vr[vrC].sw[i];

		temp += (sint32)gCPU.vr[vrA].sh[i<<1] *
			(sint32)gCPU.vr[vrB].sh[i<<1];
		temp += (sint32)gCPU.vr[vrA].sh[(i<<1)+1] *
			(sint32)gCPU.vr[vrB].sh[(i<<1)+1];

		gCPU.vr[vrD].sw[i] = SATURATE_SW(temp);
	}
}
JITCFlow ppc_opc_gen_vmsumshs()
{
	ppc_opc_gen_interpret(ppc_opc_vmsumshs);
	return flowEndBlock;
}

/*	vsum4ubs	Vector Sum Across Partial (1/4) Unsigned Byte Saturate
 *	v.275
 */
void ppc_opc_vsum4ubs()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint64 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	/* For this, there's no way to get around 64-bit math.  If we use
	 *   the hacks used before, then we have to do it so often, that
	 *   we'll outpace the 64-bit math in execution time.
	 */

	for (int i=0; i<4; i++) {
		res = (uint64)gCPU.vr[vrB].w[i];

		res += (uint64)gCPU.vr[vrA].b[(i<<2)];
		res += (uint64)gCPU.vr[vrA].b[(i<<2)+1];
		res += (uint64)gCPU.vr[vrA].b[(i<<2)+2];
		res += (uint64)gCPU.vr[vrA].b[(i<<2)+3];

		gCPU.vr[vrD].w[i] = SATURATE_UW(res);
	}
}
JITCFlow ppc_opc_gen_vsum4ubs()
{
	ppc_opc_gen_interpret(ppc_opc_vsum4ubs);
	return flowEndBlock;
}

/*	vsum4sbs	Vector Sum Across Partial (1/4) Signed Byte Saturate
 *	v.273
 */
void ppc_opc_vsum4sbs()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint64 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = (sint64)gCPU.vr[vrB].sw[i];

		res += (sint64)gCPU.vr[vrA].sb[(i<<2)];
		res += (sint64)gCPU.vr[vrA].sb[(i<<2)+1];
		res += (sint64)gCPU.vr[vrA].sb[(i<<2)+2];
		res += (sint64)gCPU.vr[vrA].sb[(i<<2)+3];

		gCPU.vr[vrD].sw[i] = SATURATE_SW(res);
	}
}
JITCFlow ppc_opc_gen_vsum4sbs()
{
	ppc_opc_gen_interpret(ppc_opc_vsum4sbs);
	return flowEndBlock;
}

/*	vsum4shs	Vector Sum Across Partial (1/4) Signed Half Word Saturate
 *	v.274
 */
void ppc_opc_vsum4shs()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint64 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = (sint64)gCPU.vr[vrB].sw[i];

		res += (sint64)gCPU.vr[vrA].sh[(i<<1)];
		res += (sint64)gCPU.vr[vrA].sh[(i<<1)+1];

		gCPU.vr[vrD].sw[i] = SATURATE_SW(res);
	}
}
JITCFlow ppc_opc_gen_vsum4shs()
{
	ppc_opc_gen_interpret(ppc_opc_vsum4shs);
	return flowEndBlock;
}

/*	vsum2sws	Vector Sum Across Partial (1/2) Signed Word Saturate
 *	v.272
 */
void ppc_opc_vsum2sws()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint64 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	res = (sint64)gCPU.vr[vrA].sw[0] + (sint64)gCPU.vr[vrA].sw[1];
	res += (sint64)gCPU.vr[vrB].sw[VECT_ODD(0)];

	gCPU.vr[vrD].w[VECT_ODD(0)] = SATURATE_SW(res);
	gCPU.vr[vrD].w[VECT_EVEN(0)] = 0;

	res = (sint64)gCPU.vr[vrA].sw[2] + (sint64)gCPU.vr[vrA].sw[3];
	res += (sint64)gCPU.vr[vrB].sw[VECT_ODD(1)];

	gCPU.vr[vrD].w[VECT_ODD(1)] = SATURATE_SW(res);
	gCPU.vr[vrD].w[VECT_EVEN(1)] = 0;
}
JITCFlow ppc_opc_gen_vsum2sws()
{
	ppc_opc_gen_interpret(ppc_opc_vsum2sws);
	return flowEndBlock;
}

/*	vsumsws		Vector Sum Across Signed Word Saturate
 *	v.271
 */
void ppc_opc_vsumsws()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint64 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	res = (sint64)gCPU.vr[vrA].sw[0] + (sint64)gCPU.vr[vrA].sw[1];
	res += (sint64)gCPU.vr[vrA].sw[2] + (sint64)gCPU.vr[vrA].sw[3];

	res += (sint64)VECT_W(gCPU.vr[vrB], 3);

	VECT_W(gCPU.vr[vrD], 3) = SATURATE_SW(res);
	VECT_W(gCPU.vr[vrD], 2) = 0;
	VECT_W(gCPU.vr[vrD], 1) = 0;
	VECT_W(gCPU.vr[vrD], 0) = 0;
}
JITCFlow ppc_opc_gen_vsumsws()
{
	ppc_opc_gen_interpret(ppc_opc_vsumsws);
	return flowEndBlock;
}

/*	vnmsubfp	Vector Negative Multiply-Subtract Floating Point
 *	v.215
 */
void ppc_opc_vnmsubfp()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB, vrC;
	double res;
	PPC_OPC_TEMPL_A(gCPU.current_opc, vrD, vrA, vrB, vrC);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		res = (double)gCPU.vr[vrA].f[i] * (double)gCPU.vr[vrC].f[i];

		res = (double)gCPU.vr[vrB].f[i] - res;

		gCPU.vr[vrD].f[i] = (float)res;
	}
}
JITCFlow ppc_opc_gen_vnmsubfp()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vnmsubfp);
	return flowEndBlock;
#else
	int vrD, vrA, vrB, vrC;
	PPC_OPC_TEMPL_A(gJITC.current_opc, vrD, vrA, vrB, vrC);

	if (SSE_AVAIL) {
		commutative_operation(X86_MULPS, vrT, vrA, vrC);
		noncommutative_operation(X86_SUBPS, vrD, vrB, vrT);
		return flowContinue;
	}

	ppc_opc_gen_interpret(ppc_opc_vnmsubfp);
	return flowEndBlock;
#endif
}

/*	vavgub		Vector Average Unsigned Byte
 *	v.152
 */
void ppc_opc_vavgub()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint16 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		res = (uint16)gCPU.vr[vrA].b[i] +
			(uint16)gCPU.vr[vrB].b[i] + 1;

		gCPU.vr[vrD].b[i] = (res >> 1);
	}
}
JITCFlow ppc_opc_gen_vavgub()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vavgub);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrma, modrmb;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (SSE2_AVAIL) {
		commutative_operation(X86_PAVGB, vrD, vrA, vrB);
		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg = jitcAllocRegister();
	NativeReg reg2 = jitcAllocRegister();
	NativeReg reg3 = jitcAllocRegister();

	for (int i=0; i<16; i += 4) {
		x86_mem2(modrma, &(gCPU.vr[vrA].b[i]));
		x86_mem2(modrmb, &(gCPU.vr[vrB].b[i]));

		asmALU(X86_MOV, reg, modrma);
		asmALU(X86_MOV, reg2, modrmb);
		asmALU(X86_MOV, reg3, reg);

		asmALU(X86_OR, reg3, reg2);
		asmALU(X86_AND, reg3, 0x01010101);

		asmShift(X86_SHR, reg, 1);
		asmShift(X86_SHR, reg2, 1);

		asmALU(X86_AND, reg, 0x7f7f7f7f);
		asmALU(X86_AND, reg2, 0x7f7f7f7f);

		asmALU(X86_ADD, reg, reg2);
		asmALU(X86_ADD, reg, reg3);

		vec_setWord(vrD, i, reg);
	}

	return flowContinue;
#endif
}

/*	vavguh		Vector Average Unsigned Half Word
 *	v.153
 */
void ppc_opc_vavguh()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint32 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = (uint32)gCPU.vr[vrA].h[i] +
			(uint32)gCPU.vr[vrB].h[i] + 1;

		gCPU.vr[vrD].h[i] = (res >> 1);
	}
}
JITCFlow ppc_opc_gen_vavguh()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vavguh);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrma, modrmb;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (SSE2_AVAIL) {
		commutative_operation(X86_PAVGW, vrD, vrA, vrB);
		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg = jitcAllocRegister();
	NativeReg reg2 = jitcAllocRegister();
	NativeReg reg3 = jitcAllocRegister();

	for (int i=0; i<16; i += 4) {
		x86_mem2(modrma, &(gCPU.vr[vrA].b[i]));
		x86_mem2(modrmb, &(gCPU.vr[vrB].b[i]));

		asmALU(X86_MOV, reg, modrma);
		asmALU(X86_MOV, reg2, modrmb);
		asmALU(X86_MOV, reg3, reg);

		asmALU(X86_OR, reg3, reg2);
		asmALU(X86_AND, reg3, 0x00010001);

		asmShift(X86_SHR, reg, 1);
		asmShift(X86_SHR, reg2, 1);

		asmALU(X86_AND, reg, 0x7fff7fff);
		asmALU(X86_AND, reg2, 0x7fff7fff);

		asmALU(X86_ADD, reg, reg2);
		asmALU(X86_ADD, reg, reg3);

		vec_setWord(vrD, i, reg);
	}

	return flowContinue;
#endif
}

/*	vavguw		Vector Average Unsigned Word
 *	v.154
 */
void ppc_opc_vavguw()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint64 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = (uint64)gCPU.vr[vrA].w[i] +
			(uint64)gCPU.vr[vrB].w[i] + 1;

		gCPU.vr[vrD].w[i] = (res >> 1);
	}
}
JITCFlow ppc_opc_gen_vavguw()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vavguw);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrma, modrmb;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg = jitcAllocRegister();
	NativeReg reg2 = jitcAllocRegister();
	NativeReg reg3 = jitcAllocRegister();

	for (int i=0; i<16; i += 4) {
		x86_mem2(modrma, &(gCPU.vr[vrA].b[i]));
		x86_mem2(modrmb, &(gCPU.vr[vrB].b[i]));

		asmALU(X86_MOV, reg, modrma);
		asmALU(X86_MOV, reg2, modrmb);
		asmALU(X86_MOV, reg3, reg);

		asmALU(X86_OR, reg3, reg2);
		asmALU(X86_AND, reg3, 0x00000001);

		asmShift(X86_SHR, reg, 1);
		asmShift(X86_SHR, reg2, 1);

		asmALU(X86_ADD, reg, reg2);
		asmALU(X86_ADD, reg, reg3);

		vec_setWord(vrD, i, reg);
	}

	return flowContinue;
#endif
}

/*	vavgsb		Vector Average Signed Byte
 *	v.149
 */
void ppc_opc_vavgsb()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint16 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		res = (sint16)gCPU.vr[vrA].sb[i] +
			(sint16)gCPU.vr[vrB].sb[i] + 1;

		gCPU.vr[vrD].sb[i] = (res >> 1);
	}
}
JITCFlow ppc_opc_gen_vavgsb()
{
	ppc_opc_gen_interpret(ppc_opc_vavgsb);
	return flowEndBlock;
}

/*	vavgsh		Vector Average Signed Half Word
 *	v.150
 */
void ppc_opc_vavgsh()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint32 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = (sint32)gCPU.vr[vrA].sh[i] +
			(sint32)gCPU.vr[vrB].sh[i] + 1;

		gCPU.vr[vrD].sh[i] = (res >> 1);
	}
}
JITCFlow ppc_opc_gen_vavgsh()
{
	ppc_opc_gen_interpret(ppc_opc_vavgsh);
	return flowEndBlock;
}

/*	vavgsw		Vector Average Signed Word
 *	v.151
 */
void ppc_opc_vavgsw()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint64 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = (sint64)gCPU.vr[vrA].sw[i] +
			(sint64)gCPU.vr[vrB].sw[i] + 1;

		gCPU.vr[vrD].sw[i] = (res >> 1);
	}
}
JITCFlow ppc_opc_gen_vavgsw()
{
	ppc_opc_gen_interpret(ppc_opc_vavgsw);
	return flowEndBlock;
}

/*	vmaxub		Vector Maximum Unsigned Byte
 *	v.182
 */
void ppc_opc_vmaxub()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint8 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		res = gCPU.vr[vrA].b[i];

		if (res < gCPU.vr[vrB].b[i])
			res = gCPU.vr[vrB].b[i];

		gCPU.vr[vrD].b[i] = res;
	}
}
JITCFlow ppc_opc_gen_vmaxub()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vmaxub);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (vrA == vrD && vrB == vrD) {
		return flowContinue;
	} else if (vrA == vrB) {
		vec_Copy(vrD, vrA);

		return flowContinue;
	}

	if (SSE2_AVAIL) {
		commutative_operation(X86_PMAXUB, vrD, vrA, vrB);
		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg8 reg = (NativeReg8)jitcAllocRegister(NATIVE_REG_8);

	for (int i=0; i<16; i++) {
		vec_getByte(reg, vrA, REG_NO, i);

		x86_mem2(modrm, &(gCPU.vr[vrB].b[i]));

		asmALU(X86_CMP, reg, modrm);
		asmCMOV(X86_B, (NativeReg)reg, modrm);

		vec_setByte(vrD, REG_NO, i, reg);
	}

	return flowContinue;
#endif
}

/*	vmaxuh		Vector Maximum Unsigned Half Word
 *	v.183
 */
void ppc_opc_vmaxuh()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint16 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = gCPU.vr[vrA].h[i];

		if (res < gCPU.vr[vrB].h[i])
			res = gCPU.vr[vrB].h[i];

		gCPU.vr[vrD].h[i] = res;
	}
}
JITCFlow ppc_opc_gen_vmaxuh()
{
	ppc_opc_gen_interpret(ppc_opc_vmaxuh);
	return flowEndBlock;
}

/*	vmaxuw		Vector Maximum Unsigned Word
 *	v.184
 */
void ppc_opc_vmaxuw()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint32 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = gCPU.vr[vrA].w[i];

		if (res < gCPU.vr[vrB].w[i])
			res = gCPU.vr[vrB].w[i];

		gCPU.vr[vrD].w[i] = res;
	}
}
JITCFlow ppc_opc_gen_vmaxuw()
{
	ppc_opc_gen_interpret(ppc_opc_vmaxuw);
	return flowEndBlock;
}

/*	vmaxsb		Vector Maximum Signed Byte
 *	v.179
 */
void ppc_opc_vmaxsb()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint8 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		res = gCPU.vr[vrA].sb[i];

		if (res < gCPU.vr[vrB].sb[i])
			res = gCPU.vr[vrB].sb[i];

		gCPU.vr[vrD].sb[i] = res;
	}
}
JITCFlow ppc_opc_gen_vmaxsb()
{
	ppc_opc_gen_interpret(ppc_opc_vmaxsb);
	return flowEndBlock;
}

/*	vmaxsh		Vector Maximum Signed Half Word
 *	v.180
 */
void ppc_opc_vmaxsh()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint16 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = gCPU.vr[vrA].sh[i];

		if (res < gCPU.vr[vrB].sh[i])
			res = gCPU.vr[vrB].sh[i];

		gCPU.vr[vrD].sh[i] = res;
	}
}
JITCFlow ppc_opc_gen_vmaxsh()
{
	ppc_opc_gen_interpret(ppc_opc_vmaxsh);
	return flowEndBlock;
}

/*	vmaxsw		Vector Maximum Signed Word
 *	v.181
 */
void ppc_opc_vmaxsw()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint32 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = gCPU.vr[vrA].sw[i];

		if (res < gCPU.vr[vrB].sw[i])
			res = gCPU.vr[vrB].sw[i];

		gCPU.vr[vrD].sw[i] = res;
	}
}
JITCFlow ppc_opc_gen_vmaxsw()
{
	ppc_opc_gen_interpret(ppc_opc_vmaxsw);
	return flowEndBlock;
}

/*	vmaxfp		Vector Maximum Floating Point
 *	v.178
 */
void ppc_opc_vmaxfp()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	float res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		res = gCPU.vr[vrA].f[i];

		if (res < gCPU.vr[vrB].f[i])
			res = gCPU.vr[vrB].f[i];

		gCPU.vr[vrD].f[i] = res;
	}
}
JITCFlow ppc_opc_gen_vmaxfp()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vmaxfp);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (vrA == vrD && vrB == vrD) {
		return flowContinue;
	} else if (vrA == vrB) {
		vec_Copy(vrD, vrA);

		return flowContinue;
	}

	if (SSE_AVAIL) {
		commutative_operation(X86_MAXPS, vrD, vrA, vrB);
		return flowContinue;
	}

	ppc_opc_gen_interpret(ppc_opc_vmaxfp);
	return flowEndBlock;
#endif
}

/*	vminub		Vector Minimum Unsigned Byte
 *	v.191
 */
void ppc_opc_vminub()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint8 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		res = gCPU.vr[vrA].b[i];

		if (res > gCPU.vr[vrB].b[i])
			res = gCPU.vr[vrB].b[i];

		gCPU.vr[vrD].b[i] = res;
	}
}
JITCFlow ppc_opc_gen_vminub()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vminub);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (vrA == vrD && vrB == vrD) {
		return flowContinue;
	} else if (vrA == vrB) {
		vec_Copy(vrD, vrA);

		return flowContinue;
	}

	if (SSE2_AVAIL) {
		commutative_operation(X86_PMINUB, vrD, vrA, vrB);
		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg8 reg = (NativeReg8)jitcAllocRegister(NATIVE_REG_8);
	NativeReg8 reg2 = (NativeReg8)jitcAllocRegister(NATIVE_REG_8);

	for (int i=0; i<16; i++) {
		vec_getByte(reg, vrA, REG_NO, i);
		vec_getByte(reg2, vrB, REG_NO, i);

		asmALU(X86_CMP, reg, reg2);
		asmCMOV(X86_A, (NativeReg)reg, (NativeReg)reg2);

		vec_setByte(vrD, REG_NO, i, reg);
	}

	return flowContinue;
#endif
}

/*	vminuh		Vector Minimum Unsigned Half Word
 *	v.192
 */
void ppc_opc_vminuh()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint16 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = gCPU.vr[vrA].h[i];

		if (res > gCPU.vr[vrB].h[i])
			res = gCPU.vr[vrB].h[i];

		gCPU.vr[vrD].h[i] = res;
	}
}
JITCFlow ppc_opc_gen_vminuh()
{
	ppc_opc_gen_interpret(ppc_opc_vminuh);
	return flowEndBlock;
}

/*	vminuw		Vector Minimum Unsigned Word
 *	v.193
 */
void ppc_opc_vminuw()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	uint32 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = gCPU.vr[vrA].w[i];

		if (res > gCPU.vr[vrB].w[i])
			res = gCPU.vr[vrB].w[i];

		gCPU.vr[vrD].w[i] = res;
	}
}
JITCFlow ppc_opc_gen_vminuw()
{
	ppc_opc_gen_interpret(ppc_opc_vminuw);
	return flowEndBlock;
}

/*	vminsb		Vector Minimum Signed Byte
 *	v.188
 */
void ppc_opc_vminsb()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint8 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		res = gCPU.vr[vrA].sb[i];

		if (res > gCPU.vr[vrB].sb[i])
			res = gCPU.vr[vrB].sb[i];

		gCPU.vr[vrD].sb[i] = res;
	}
}
JITCFlow ppc_opc_gen_vminsb()
{
	ppc_opc_gen_interpret(ppc_opc_vminsb);
	return flowEndBlock;
}

/*	vminsh		Vector Minimum Signed Half Word
 *	v.189
 */
void ppc_opc_vminsh()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint16 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		res = gCPU.vr[vrA].sh[i];

		if (res > gCPU.vr[vrB].sh[i])
			res = gCPU.vr[vrB].sh[i];

		gCPU.vr[vrD].sh[i] = res;
	}
}
JITCFlow ppc_opc_gen_vminsh()
{
	ppc_opc_gen_interpret(ppc_opc_vminsh);
	return flowEndBlock;
}

/*	vminsw		Vector Minimum Signed Word
 *	v.190
 */
void ppc_opc_vminsw()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	sint32 res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		res = gCPU.vr[vrA].sw[i];

		if (res > gCPU.vr[vrB].sw[i])
			res = gCPU.vr[vrB].sw[i];

		gCPU.vr[vrD].sw[i] = res;
	}
}
JITCFlow ppc_opc_gen_vminsw()
{
	ppc_opc_gen_interpret(ppc_opc_vminsw);
	return flowEndBlock;
}

/*	vminfp		Vector Minimum Floating Point
 *	v.187
 */
void ppc_opc_vminfp()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	float res;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		res = gCPU.vr[vrA].f[i];

		if (res > gCPU.vr[vrB].f[i])
			res = gCPU.vr[vrB].f[i];

		gCPU.vr[vrD].f[i] = res;
	}
}
JITCFlow ppc_opc_gen_vminfp()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vminfp);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (vrA == vrD && vrB == vrD) {
		return flowContinue;
	} else if (vrA == vrB) {
		vec_Copy(vrD, vrA);

		return flowContinue;
	}

	if (SSE_AVAIL) {
		commutative_operation(X86_MINPS, vrD, vrA, vrB);
		return flowContinue;
	}

	ppc_opc_gen_interpret(ppc_opc_vminfp);
	return flowEndBlock;
#endif
}

#define RND_NEAREST	0x0000	// round to nearest
#define RND_ZERO	0x0c00  // round to zero
#define RND_PINF	0x0800	// round to +INF
#define RND_MINF	0x0400	// round to -INF

/* This functionality was borrowed from glibc 2.3.3 */
static inline void set_roundmode(int mode)
{
	modrm_o modrm;

	jitcClobberCarryAndFlags();
	NativeReg reg = jitcAllocRegister();

	x86_mem2(modrm, &gCPU.vfcw_save);

	asmFSTCW(x86_mem2(modrm, &gCPU.vfcw_save));

	asmMOV_NoFlags(reg, mode);	// avoid flag clobber
	asmALU(X86_OR, reg, modrm);

	if (mode != RND_ZERO) {
		asmALU(X86_AND, reg, (0xffff ^ (~mode & 0x0c00)));
	}

	x86_mem2(modrm, &gCPU.vfcw);

	asmALU(X86_MOV, modrm, reg);

	asmFLDCW(modrm);
}

static inline void restore_roundmode(void)
{
	modrm_o modrm;

	asmFLDCW(x86_mem2(modrm, &gCPU.vfcw_save));
}

/*	vrfin		Vector Round to Floating-Point Integer Nearest
 *	v.231
 */
void ppc_opc_vrfin()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);

	/* Documentation doesn't dictate how this instruction should
	 *   round from a middle point.  With a test on a real G4, it was
	 *   found to be round to nearest, with bias to even if equidistant.
	 *
	 * This is covered by the function rint()
	 */
	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		gCPU.vr[vrD].f[i] = rintf(gCPU.vr[vrB].f[i]);
	}
}
JITCFlow ppc_opc_gen_vrfin()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vrfin);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcFloatRegisterClobberAll();
	set_roundmode(RND_NEAREST);

	for (int i=0; i<16; i+=4) { //FIXME: This might not comply with Java FP
		asmFLD_Single(x86_mem2(modrm, &(gCPU.vr[vrB].b[i])));

		asmFSimple(FRNDINT);

		asmFSTP_Single(x86_mem2(modrm, &(gCPU.vr[vrD].b[i])));
	}

	restore_roundmode();

	return flowContinue;
#endif
}

/*	vrfip		Vector Round to Floating-Point Integer toward Plus Infinity
 *	v.232
 */
void ppc_opc_vrfip()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		gCPU.vr[vrD].f[i] = ceilf(gCPU.vr[vrB].f[i]);
	}
}
JITCFlow ppc_opc_gen_vrfip()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vrfip);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcFloatRegisterClobberAll();
	set_roundmode(RND_PINF);

	for (int i=0; i<16; i+=4) { //FIXME: This might not comply with Java FP
		asmFLD_Single(x86_mem2(modrm, &(gCPU.vr[vrB].b[i])));

		asmFSimple(FRNDINT);

		asmFSTP_Single(x86_mem2(modrm, &(gCPU.vr[vrD].b[i])));
	}

	restore_roundmode();

	return flowContinue;
#endif
}

/*	vrfim		Vector Round to Floating-Point Integer toward Minus Infinity
 *	v.230
 */
void ppc_opc_vrfim()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		gCPU.vr[vrD].f[i] = floorf(gCPU.vr[vrB].f[i]);
	}
}
JITCFlow ppc_opc_gen_vrfim()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vrfim);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcFloatRegisterClobberAll();
	set_roundmode(RND_MINF);

	for (int i=0; i<16; i+=4) { //FIXME: This might not comply with Java FP
		asmFLD_Single(x86_mem2(modrm, &(gCPU.vr[vrB].b[i])));

		asmFSimple(FRNDINT);

		asmFSTP_Single(x86_mem2(modrm, &(gCPU.vr[vrD].b[i])));
	}

	restore_roundmode();

	return flowContinue;
#endif
}

/*	vrfiz	Vector Round to Floating-Point Integer toward Zero
 *	v.233
 */
void ppc_opc_vrfiz()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		gCPU.vr[vrD].f[i] = rintf(gCPU.vr[vrD].f[i]);
	}
}
JITCFlow ppc_opc_gen_vrfiz()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vrfiz);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcFloatRegisterClobberAll();
	set_roundmode(RND_ZERO);

	for (int i=0; i<16; i+=4) { //FIXME: This might not comply with Java FP
		asmFLD_Single(x86_mem2(modrm, &(gCPU.vr[vrB].b[i])));

		asmFSimple(FRNDINT);

		asmFSTP_Single(x86_mem2(modrm, &(gCPU.vr[vrD].b[i])));
	}

	restore_roundmode();

	return flowContinue;
#endif
}

/*	vrefp		Vector Reciprocal Estimate Floating Point
 *	v.228
 */
void ppc_opc_vrefp()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);

	/* This emulation generates an exact value, instead of an estimate.
	 *   This is technically within specs, but some test-suites expect the
	 *   exact estimate value returned by G4s.  These anomolous failures
	 *   should be ignored.
	 */

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		gCPU.vr[vrD].f[i] = 1 / gCPU.vr[vrB].f[i];
	}
}
JITCFlow ppc_opc_gen_vrefp()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vrefp);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (!(SSE_AVAIL)) {
		ppc_opc_gen_interpret(ppc_opc_vrefp);
		return flowEndBlock;
	}

	if (vrB == vrD) {
		NativeVectorReg reg = jitcGetClientVectorRegisterDirty(vrB);

		asmALUPS(X86_RCPPS, reg, reg);

		return flowContinue;
	}

	NativeVectorReg reg1 = jitcAllocVectorRegister();
	NativeVectorReg reg2 = jitcGetClientVectorRegisterMapping(vrB);

	if (reg2 == VECTREG_NO) {
		asmALUPS(X86_RCPPS, reg1, x86_mem2(modrm, &gCPU.vr[vrB]));
	} else {
		asmALUPS(X86_RCPPS, reg1, reg2);
	}

	jitcRenameVectorRegisterDirty(reg1, vrD);

	return flowContinue;
#endif
}

/*	vrsqrtefp	Vector Reciprocal Square Root Estimate Floating Point
 *	v.237
 */
void ppc_opc_vrsqrtefp()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);

	/* This emulation generates an exact value, instead of an estimate.
	 *   This is technically within specs, but some test-suites expect the
	 *   exact estimate value returned by G4s.  These anomolous failures
	 *   should be ignored.
	 */

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		gCPU.vr[vrD].f[i] = 1 / sqrt(gCPU.vr[vrB].f[i]);
	}
}
JITCFlow ppc_opc_gen_vrsqrtefp()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vrsqrtefp);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (!(SSE_AVAIL)) {
		ppc_opc_gen_interpret(ppc_opc_vrsqrtefp);
		return flowEndBlock;
	}

	if (vrB == vrD) {
		NativeVectorReg reg = jitcGetClientVectorRegisterDirty(vrB);

		asmALUPS(X86_SQRTPS, reg, reg);
		asmALUPS(X86_RCPPS, reg, reg);

		return flowContinue;
	}

	NativeVectorReg reg1 = jitcAllocVectorRegister();
	NativeVectorReg reg2 = jitcGetClientVectorRegisterMapping(vrB);

	if (reg2 == VECTREG_NO) {
		asmALUPS(X86_SQRTPS, reg1, x86_mem2(modrm, &gCPU.vr[vrB]));
	} else {
		asmALUPS(X86_SQRTPS, reg1, reg2);
	}
	asmALUPS(X86_RCPPS, reg1, reg1);

	jitcRenameVectorRegisterDirty(reg1, vrD);

	return flowContinue;
#endif
}

/*	vlogefp		Vector Log2 Estimate Floating Point
 *	v.175
 */
void ppc_opc_vlogefp()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);

	/* This emulation generates an exact value, instead of an estimate.
	 *   This is technically within specs, but some test-suites expect the
	 *   exact estimate value returned by G4s.  These anomolous failures
	 *   should be ignored.
	 */

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		gCPU.vr[vrD].f[i] = log2(gCPU.vr[vrB].f[i]);
	}
}
JITCFlow ppc_opc_gen_vlogefp()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vlogefp);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcFloatRegisterClobberAll();

	for (int i=0; i<16; i+=4) { //FIXME: This might not comply with Java FP
		asmFSimple(FLD1);

		asmFLD_Single(x86_mem2(modrm, &(gCPU.vr[vrB].b[i])));

		asmFSimple(FYL2X);

		asmFSTP_Single(x86_mem2(modrm, &(gCPU.vr[vrB].b[i])));
	}

	return flowContinue;
#endif
}

/*	vexptefp	Vector 2 Raised to the Exponent Estimate Floating Point
 *	v.173
 */
void ppc_opc_vexptefp()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);
	PPC_OPC_ASSERT(vrA==0);

	/* This emulation generates an exact value, instead of an estimate.
	 *   This is technically within specs, but some test-suites expect the
	 *   exact estimate value returned by G4s.  These anomolous failures
	 *   should be ignored.
	 */

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		gCPU.vr[vrD].f[i] = exp2(gCPU.vr[vrB].f[i]);
	}
}
JITCFlow ppc_opc_gen_vexptefp()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vexptefp);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcFloatRegisterClobberAll();

	for (int i=0; i<16; i+=4) { //FIXME: This might not comply with Java FP
		asmFSimple(FLD1);

		asmFLD_Single(x86_mem2(modrm, &(gCPU.vr[vrB].b[i])));

		asmFSimple(F2XM1);
		asmFArithP_STi(X86_FADD, Float_ST1);

		asmFSTP_Single(x86_mem2(modrm, &(gCPU.vr[vrD].b[i])));
	}

	return flowContinue;
#endif
}

/*	vcfux		Vector Convert from Unsigned Fixed-Point Word
 *	v.156
 */
void ppc_opc_vcfux()
{
	VECTOR_DEBUG;
	int vrD, vrB;
	uint32 uimm;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, uimm, vrB);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		gCPU.vr[vrD].f[i] = ((float)gCPU.vr[vrB].w[i]) / (1 << uimm);
 	}
}
JITCFlow ppc_opc_gen_vcfux()
{
	ppc_opc_gen_interpret(ppc_opc_vcfux);
	return flowEndBlock;
}

/*	vcfsx		Vector Convert from Signed Fixed-Point Word
 *	v.155
 */
void ppc_opc_vcfsx()
{
	VECTOR_DEBUG;
	int vrD, vrB;
	uint32 uimm;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, uimm, vrB);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		gCPU.vr[vrD].f[i] = ((float)gCPU.vr[vrB].sw[i]) / (1 << uimm);
 	}
}
JITCFlow ppc_opc_gen_vcfsx()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vcfsx);
	return flowEndBlock;
#else
	int vrD, vrB;
	uint32 uimm;
	modrm_o modrm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, uimm, vrB);

	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	jitcClobberRegister(NATIVE_REG | EAX);

	jitcFloatRegisterClobberAll();

	if (uimm == 1) {
		asmFSimple(FLD1);
	} else if (uimm > 1) {
		asmALU_D(X86_MOV, x86_mem2(modrm, &gCPU.vtemp), uimm);
		asmFILD_D(modrm);
	}

	if (uimm)	asmFSimple(FCHS);

	for (int i=0; i<16; i+=4) { //FIXME: This might not comply with Java FP
		asmFILD_D(x86_mem2(modrm, &(gCPU.vr[vrB].b[i])));

		if (uimm) {
			asmFSimple(FSCALE);
	 	}

		asmFSTP_Single(x86_mem2(modrm, &(gCPU.vr[vrD].b[i])));
	}

	if (uimm)	asmFSTP(Float_ST0);

	return flowContinue;
#endif
}

/*	vctsxs		Vector Convert To Signed Fixed-Point Word Saturate
 *	v.171
 */
void ppc_opc_vctsxs()
{
	VECTOR_DEBUG;
	int vrD, vrB;
	uint32 uimm;
	float ftmp;
	sint32 tmp;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, uimm, vrB);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		ftmp = gCPU.vr[vrB].f[i] * (float)(1 << uimm);
		ftmp = rintf(ftmp);

		tmp = (sint32)ftmp;

		if (ftmp > 2147483647.0) {
			tmp = 2147483647;		// 0x7fffffff
			gCPU.vscr |= VSCR_SAT;
		} else if (ftmp < -2147483648.0) {
			tmp = -2147483648LL;		// 0x80000000
			gCPU.vscr |= VSCR_SAT;
		}

		gCPU.vr[vrD].sw[i] = tmp;
 	}
}
JITCFlow ppc_opc_gen_vctsxs()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vctsxs);
	return flowEndBlock;
#else
	int vrD, vrB;
	uint32 uimm;
	modrm_o modrm, dest_modrm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, uimm, vrB);

	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	jitcClobberRegister(NATIVE_REG | EAX);

	jitcFloatRegisterClobberAll();
	set_roundmode(RND_ZERO);

	if (uimm == 1) {
		asmFSimple(FLD1);
	} else if (uimm > 1) {
		asmALU_D(X86_MOV, x86_mem2(modrm, &gCPU.vtemp), uimm);
		asmFILD_D(modrm);
	}

	for (int i=0; i<16; i+=4) { //FIXME: This might not comply with Java FP
		asmFLD_Single(x86_mem2(modrm, &(gCPU.vr[vrB].b[i])));

		if (uimm) {
			asmFSimple(FSCALE);
		}

		asmFSimple(FRNDINT);

		asmFIComp(X86_FICOM32, x86_mem2(modrm, &sint32_max));
		asmFSTSW_EAX();
		asmALU(X86_TEST, EAX, 0x4100);
		NativeAddress of_high = asmJxxFixup(X86_Z);

		asmFIComp(X86_FICOM32, x86_mem2(modrm, &sint32_min));
		asmFSTSW_EAX();
		asmALU(X86_TEST, EAX, 0x0100);
		NativeAddress of_low = asmJxxFixup(X86_NZ);

		asmFISTP_D(x86_mem2(dest_modrm, &(gCPU.vr[vrD].b[i])));
		NativeAddress skip_end = asmJMPFixup();

		asmResolveFixup(of_high);
		asmALU_D(X86_MOV, dest_modrm, 2147483647);
		NativeAddress skip1 = asmJMPFixup();

		asmResolveFixup(of_low);
		asmALU_D(X86_MOV, dest_modrm, -2147483648);

		asmResolveFixup(skip1);
		vec_raise_saturate();

		asmResolveFixup(skip_end);
 	}

	if (uimm)	asmFSTP(Float_ST0);

	restore_roundmode();

	return flowContinue;
#endif
}

/*	vctuxs		Vector Convert to Unsigned Fixed-Point Word Saturate
 *	v.172
 */
void ppc_opc_vctuxs()
{
	VECTOR_DEBUG;
	int vrD, vrB;
	uint32 tmp, uimm;
	float ftmp;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, uimm, vrB);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		ftmp = gCPU.vr[vrB].f[i] * (float)(1 << uimm);
		ftmp = rintf(ftmp);

		tmp = (uint32)ftmp;

		if (ftmp > 4294967295.0) {
			tmp = 0xffffffff;
			gCPU.vscr |= VSCR_SAT;
		} else if (ftmp < 0) {
			tmp = 0;
			gCPU.vscr |= VSCR_SAT;
		}

		gCPU.vr[vrD].w[i] = tmp;
 	}
}
JITCFlow ppc_opc_gen_vctuxs()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vctuxs);
	return flowEndBlock;
#else
	int vrD, vrB;
	uint32 uimm;
	modrm_o modrm, dest_modrm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, uimm, vrB);

	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	jitcClobberRegister(NATIVE_REG | EAX);

	jitcFloatRegisterClobberAll();
	set_roundmode(RND_ZERO);

	NativeReg reg = jitcAllocRegister();

	if (uimm == 1) {
		asmFSimple(FLD1);
	} else if (uimm > 1) {
		asmALU_D(X86_MOV, x86_mem2(modrm, &gCPU.vtemp), uimm);
		asmFILD_D(modrm);
	}

	for (int i=0; i<16; i+=4) { //FIXME: This might not comply with Java FP
		asmFLD_Single(x86_mem2(modrm, &(gCPU.vr[vrB].b[i])));

		if (uimm) {
			asmFSimple(FSCALE);
		}

		asmFSimple(FRNDINT);

		asmFISTP_Q(x86_mem2(modrm, &gCPU.vtemp64));

		asmMOV(reg, (byte *)&gCPU.vtemp64+4);

		asmALU(X86_TEST, reg, 0x80000000);
		NativeAddress of_low = asmJxxFixup(X86_NZ);

		asmALU(X86_TEST, reg, 0xffffffff);
		NativeAddress of_high = asmJxxFixup(X86_NZ);

		asmMOV(reg, &gCPU.vtemp64);

		x86_mem2(dest_modrm, &(gCPU.vr[vrD].b[i]));

		asmALU(X86_MOV, dest_modrm, reg);
		NativeAddress skip_end = asmJMPFixup();

		asmResolveFixup(of_high);
		asmALU_D(X86_MOV, dest_modrm, 0xffffffff);
		NativeAddress skip1 = asmJMPFixup();

		asmResolveFixup(of_low);
		asmALU_D(X86_MOV, dest_modrm, 0);

		asmResolveFixup(skip1);
		vec_raise_saturate();

		asmResolveFixup(skip_end);
 	}

	if (uimm)	asmFSTP(Float_ST0);

	restore_roundmode();

	return flowContinue;
#endif
}

static inline void commutative_operation(X86ALUPSopc opc, int vrD, int vrA, int vrB)
{
	NativeVectorReg d, s;
	modrm_o modrm;
	int vrS;

	if (vrA == vrD) {
		d = jitcGetClientVectorRegisterDirty(vrD);
		s = jitcGetClientVectorRegisterMapping(vrB);
		vrS = vrB;
	} else if (vrB == vrD) {
		d = jitcGetClientVectorRegisterDirty(vrD);
		s = jitcGetClientVectorRegisterMapping(vrA);
		vrS = vrA;
	} else {
		d = jitcMapClientVectorRegisterDirty(vrD);
		NativeVectorReg a = jitcGetClientVectorRegisterMapping(vrA);
		s = jitcGetClientVectorRegisterMapping(vrB);
		vrS = vrB;

		if (a == VECTREG_NO)
			asmMOVAPS(d, &gCPU.vr[vrA]);
		else
			asmALUPS(X86_MOVAPS, d, a);
	}

	if (s == VECTREG_NO) {
		asmALUPS(opc, d, x86_mem2(modrm, &gCPU.vr[vrS]));
	} else
		asmALUPS(opc, d, s);
}

static inline void commutative_operation(X86PALUopc opc, int vrD, int vrA, int vrB)
{
	NativeVectorReg d, s;
	modrm_o modrm;
	int vrS;

	if (vrA == vrD) {
		d = jitcGetClientVectorRegisterDirty(vrD);
		s = jitcGetClientVectorRegisterMapping(vrB);
		vrS = vrB;
	} else if (vrB == vrD) {
		d = jitcGetClientVectorRegisterDirty(vrD);
		s = jitcGetClientVectorRegisterMapping(vrA);
		vrS = vrA;
	} else {
		d = jitcMapClientVectorRegisterDirty(vrD);
		NativeVectorReg a = jitcGetClientVectorRegisterMapping(vrA);
		s = jitcGetClientVectorRegisterMapping(vrB);
		vrS = vrB;

		if (a == VECTREG_NO)
			asmMOVAPS(d, &gCPU.vr[vrA]);
		else
			asmALUPS(X86_MOVAPS, d, a);
	}

	if (s == VECTREG_NO) {
		asmPALU(opc, d, x86_mem2(modrm, &gCPU.vr[vrS]));
	} else
		asmPALU(opc, d, s);
}

static inline void noncommutative_operation(X86ALUPSopc opc, int vrD, int vrA, int vrB)
{
	NativeVectorReg d, s;
	modrm_o modrm;

	if (vrA == vrD) {
		d = jitcGetClientVectorRegisterDirty(vrA);
		s = jitcGetClientVectorRegisterMapping(vrB);
	} else {
		if (vrB == vrD)
			d = jitcAllocVectorRegister();
		else
			d = jitcMapClientVectorRegisterDirty(vrD);

		NativeVectorReg a = jitcGetClientVectorRegisterMapping(vrA);
		s = jitcGetClientVectorRegisterMapping(vrB);

		if (a == VECTREG_NO)
			asmMOVAPS(d, &gCPU.vr[vrA]);
		else
			asmALUPS(X86_MOVAPS, d, a);
	}

	if (s == VECTREG_NO) {
		asmALUPS(opc, d, x86_mem2(modrm, &gCPU.vr[vrB]));
	} else
		asmALUPS(opc, d, s);

	if (vrB == vrD)
		jitcRenameVectorRegisterDirty(d, vrD);
}

static inline void noncommutative_operation(X86PALUopc opc, int vrD, int vrA, int vrB)
{
	NativeVectorReg d, s;
	modrm_o modrm;

	if (vrA == vrD) {
		d = jitcGetClientVectorRegisterDirty(vrA);
		s = jitcGetClientVectorRegisterMapping(vrB);
	} else {
		if (vrB == vrD)
			d = jitcAllocVectorRegister();
		else
			d = jitcMapClientVectorRegisterDirty(vrD);

		NativeVectorReg a = jitcGetClientVectorRegisterMapping(vrA);
		s = jitcGetClientVectorRegisterMapping(vrB);

		if (a == VECTREG_NO)
			asmMOVAPS(d, &gCPU.vr[vrA]);
		else
			asmALUPS(X86_MOVAPS, d, a);
	}

	if (s == VECTREG_NO) {
		asmPALU(opc, d, x86_mem2(modrm, &gCPU.vr[vrB]));
	} else
		asmPALU(opc, d, s);

	if (vrB == vrD)
		jitcRenameVectorRegisterDirty(d, vrD);
}

/*	vand		Vector Logical AND
 *	v.147
 */
void ppc_opc_vand()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	gCPU.vr[vrD].d[0] = gCPU.vr[vrA].d[0] & gCPU.vr[vrB].d[0];
	gCPU.vr[vrD].d[1] = gCPU.vr[vrA].d[1] & gCPU.vr[vrB].d[1];
}
JITCFlow ppc_opc_gen_vand()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vand);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (vrA == vrB) {
		vec_Copy(vrD, vrA);
		return flowContinue;
	}

	if (SSE_AVAIL) {
		commutative_operation(X86_ANDPS, vrD, vrA, vrB);
		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg = jitcAllocRegister();

	vec_getWord(reg, vrA, 0);
	asmALU(X86_AND, reg, x86_mem2(modrm, &(gCPU.vr[vrB])));
	vec_setWord(vrD, 0, reg);

	vec_getWord(reg, vrA, 4);
	asmALU(X86_AND, reg, x86_mem2(modrm, &(gCPU.vr[vrB].b[4])));
	vec_setWord(vrD, 4, reg);

	vec_getWord(reg, vrA, 8);
	asmALU(X86_AND, reg, x86_mem2(modrm, &(gCPU.vr[vrB].b[8])));
	vec_setWord(vrD, 8, reg);

	vec_getWord(reg, vrA,12);
	asmALU(X86_AND, reg, x86_mem2(modrm, &(gCPU.vr[vrB].b[12])));
	vec_setWord(vrD,12, reg);

	return flowContinue;
#endif
}

/*	vandc		Vector Logical AND with Complement
 *	v.148
 */
void ppc_opc_vandc()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	gCPU.vr[vrD].d[0] = gCPU.vr[vrA].d[0] & ~gCPU.vr[vrB].d[0];
	gCPU.vr[vrD].d[1] = gCPU.vr[vrA].d[1] & ~gCPU.vr[vrB].d[1];
}
JITCFlow ppc_opc_gen_vandc()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vandc);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (vrA == vrB) {
		vec_Zero(vrD);
		return flowContinue;
	}

	if (SSE_AVAIL) {
		noncommutative_operation(X86_ANDNPS, vrD, vrB, vrA);
		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg = jitcAllocRegister();

	vec_getWord(reg, vrB, 0);
	asmALU(X86_NOT, reg);
	asmALU(X86_AND, reg, x86_mem2(modrm, &gCPU.vr[vrB]));
	vec_setWord(vrD, 0, reg);

	vec_getWord(reg, vrB, 4);
	asmALU(X86_NOT, reg);
	asmALU(X86_AND, reg, x86_mem2(modrm, &(gCPU.vr[vrB].b[4])));
	vec_setWord(vrD, 4, reg);

	vec_getWord(reg, vrB, 8);
	asmALU(X86_NOT, reg);
	asmALU(X86_AND, reg, x86_mem2(modrm, &(gCPU.vr[vrB].b[8])));
	vec_setWord(vrD, 8, reg);

	vec_getWord(reg, vrB,12);
	asmALU(X86_NOT, reg);
	asmALU(X86_AND, reg, x86_mem2(modrm, &(gCPU.vr[vrB].b[12])));
	vec_setWord(vrD,12, reg);

	return flowContinue;
#endif
}

/*	vor		Vector Logical OR
 *	v.217
 */
void ppc_opc_vor()
{
	VECTOR_DEBUG_COMMON;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	gCPU.vr[vrD].d[0] = gCPU.vr[vrA].d[0] | gCPU.vr[vrB].d[0];
	gCPU.vr[vrD].d[1] = gCPU.vr[vrA].d[1] | gCPU.vr[vrB].d[1];
}
JITCFlow ppc_opc_gen_vor()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vor);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (vrA == vrB) {
		vec_Copy(vrD, vrA);
		return flowContinue;
	}

	if (SSE_AVAIL) {
		commutative_operation(X86_ORPS, vrD, vrA, vrB);
		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg = jitcAllocRegister();

	vec_getWord(reg, vrA, 0);
	asmALU(X86_OR, reg, x86_mem2(modrm, &gCPU.vr[vrB]));
	vec_setWord(vrD, 0, reg);

	vec_getWord(reg, vrA, 4);
	asmALU(X86_OR, reg, x86_mem2(modrm, &(gCPU.vr[vrB].b[4])));
	vec_setWord(vrD, 4, reg);

	vec_getWord(reg, vrA, 8);
	asmALU(X86_OR, reg, x86_mem2(modrm, &(gCPU.vr[vrB].b[8])));
	vec_setWord(vrD, 8, reg);

	vec_getWord(reg, vrA,12);
	asmALU(X86_OR, reg, x86_mem2(modrm, &(gCPU.vr[vrB].b[12])));
	vec_setWord(vrD,12, reg);

	return flowContinue;
#endif
}

/*	vnor		Vector Logical NOR
 *	v.216
 */
void ppc_opc_vnor()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	gCPU.vr[vrD].d[0] = ~(gCPU.vr[vrA].d[0] | gCPU.vr[vrB].d[0]);
	gCPU.vr[vrD].d[1] = ~(gCPU.vr[vrA].d[1] | gCPU.vr[vrB].d[1]);
}
JITCFlow ppc_opc_gen_vnor()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vnor);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (vrA == vrB) {
		vec_Not(vrD, vrA);
		return flowContinue;
	}

	if (SSE_AVAIL) {
		commutative_operation(X86_ORPS, vrD, vrA, vrB);
		vec_Not(vrD, vrD);
		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg = jitcAllocRegister();

	vec_getWord(reg, vrA, 0);
	asmALU(X86_OR, reg, x86_mem2(modrm, &gCPU.vr[vrB]));
	asmALU(X86_NOT, reg);
	vec_setWord(vrD, 0, reg);

	vec_getWord(reg, vrA, 4);
	asmALU(X86_OR, reg, x86_mem2(modrm, &(gCPU.vr[vrB].b[4])));
	asmALU(X86_NOT, reg);
	vec_setWord(vrD, 4, reg);

	vec_getWord(reg, vrA, 8);
	asmALU(X86_OR, reg, x86_mem2(modrm, &(gCPU.vr[vrB].b[8])));
	asmALU(X86_NOT, reg);
	vec_setWord(vrD, 8, reg);

	vec_getWord(reg, vrA,12);
	asmALU(X86_OR, reg, x86_mem2(modrm, &(gCPU.vr[vrB].b[12])));
	asmALU(X86_NOT, reg);
	vec_setWord(vrD,12, reg);

	return flowContinue;
#endif
}

/*	vxor		Vector Logical XOR
 *	v.282
 */
void ppc_opc_vxor()
{
	VECTOR_DEBUG_COMMON;
	int vrD, vrA, vrB;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	gCPU.vr[vrD].d[0] = gCPU.vr[vrA].d[0] ^ gCPU.vr[vrB].d[0];
	gCPU.vr[vrD].d[1] = gCPU.vr[vrA].d[1] ^ gCPU.vr[vrB].d[1];
}
JITCFlow ppc_opc_gen_vxor()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vxor);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (vrA == vrB) {
		vec_Zero(vrD);
		return flowContinue;
	}

	if (SSE_AVAIL) {
		commutative_operation(X86_XORPS, vrD, vrA, vrB);
		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg = jitcAllocRegister();

	vec_getWord(reg, vrA, 0);
	asmALU(X86_XOR, reg, x86_mem2(modrm, &gCPU.vr[vrB]));
	vec_setWord(vrD, 0, reg);

	vec_getWord(reg, vrA, 4);
	asmALU(X86_XOR, reg, x86_mem2(modrm, &(gCPU.vr[vrB].b[4])));
	vec_setWord(vrD, 4, reg);

	vec_getWord(reg, vrA, 8);
	asmALU(X86_XOR, reg, x86_mem2(modrm, &(gCPU.vr[vrB].b[8])));
	vec_setWord(vrD, 8, reg);

	vec_getWord(reg, vrA,12);
	asmALU(X86_XOR, reg, x86_mem2(modrm, &(gCPU.vr[vrB].b[12])));
	vec_setWord(vrD,12, reg);

	return flowContinue;
#endif
}


#define CR_CR6		(0x00f0)
#define CR_CR6_EQ	(1<<7)
#define CR_CR6_NE_SOME	(1<<6)
#define CR_CR6_NE	(1<<5)
#define CR_CR6_EQ_SOME	(1<<4)

/*	vcmpequbx	Vector Compare Equal-to Unsigned Byte
 *	v.160
 */
void ppc_opc_vcmpequbx()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	int tf=CR_CR6_EQ | CR_CR6_NE;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		if (gCPU.vr[vrA].b[i] == gCPU.vr[vrB].b[i]) {
			gCPU.vr[vrD].b[i] = 0xff;
			tf &= ~CR_CR6_NE;
			tf |= CR_CR6_EQ_SOME;
		} else {
			gCPU.vr[vrD].b[i] = 0;
			tf &= ~CR_CR6_EQ;
			tf |= CR_CR6_NE_SOME;
		}
	}

	if (PPC_OPC_VRc & gCPU.current_opc) {
		gCPU.cr &= ~CR_CR6;
		gCPU.cr |= tf;
	}
}
JITCFlow ppc_opc_gen_vcmpequbx()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vcmpequbx);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (0 && vrA == vrB) {
		vec_Neg1(vrD);

		if (PPC_OPC_VRc & gJITC.current_opc) {
			jitcClobberCarryAndFlags();

			asmAND(&gCPU.cr, ~CR_CR6);
			asmOR(&gCPU.cr, CR_CR6_EQ | CR_CR6_EQ_SOME);
		}

		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg = REG_NO;
	NativeReg8 reg2 = (NativeReg8)jitcAllocRegister(NATIVE_REG_8);

	if (PPC_OPC_VRc & gJITC.current_opc) {
		reg = jitcAllocRegister();

		asmMOV_NoFlags(reg, CR_CR6_EQ | CR_CR6_NE);
	}

	for (int i=0; i<16; i++) {
		vec_getByte(reg2, vrA, REG_NO, i);

		asmALU(X86_CMP, reg2, x86_mem2(modrm, &gCPU.vr[vrB]+i));

		if (PPC_OPC_VRc & gJITC.current_opc) {
			NativeAddress skip1 = asmJxxFixup(X86_NE);

			asmMOV_NoFlags((NativeReg)reg2, 0xff);
			asmALU(X86_AND, reg, ~CR_CR6_NE);
			asmALU(X86_OR, reg, CR_CR6_EQ_SOME);
			NativeAddress skip2 = asmJMPFixup();

			asmResolveFixup(skip1);
			asmALU(X86_XOR, reg2, reg2);
			asmALU(X86_AND, reg, ~CR_CR6_EQ);
			asmALU(X86_OR, reg, CR_CR6_NE_SOME);

			asmResolveFixup(skip2);
		} else {
			asmSET(X86_NE, reg2);

			asmALU(X86_SUB, reg2, 1);
		}

		vec_setByte(vrD, REG_NO, i, reg2);
	}

	if (PPC_OPC_VRc & gJITC.current_opc) {
		asmAND(&gCPU.cr, ~CR_CR6);
		asmALU(X86_OR, x86_mem2(modrm, &gCPU.cr), reg);
	}

	return flowContinue;
#endif
}

/*	vcmpequhx	Vector Compare Equal-to Unsigned Half Word
 *	v.161
 */
void ppc_opc_vcmpequhx()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	int tf=CR_CR6_EQ | CR_CR6_NE;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		if (gCPU.vr[vrA].h[i] == gCPU.vr[vrB].h[i]) {
			gCPU.vr[vrD].h[i] = 0xffff;
			tf &= ~CR_CR6_NE;
			tf |= CR_CR6_EQ_SOME;
		} else {
			gCPU.vr[vrD].h[i] = 0;
			tf &= ~CR_CR6_EQ;
			tf |= CR_CR6_NE_SOME;
		}
	}

	if (PPC_OPC_VRc & gCPU.current_opc) {
		gCPU.cr &= ~CR_CR6;
		gCPU.cr |= tf;
	}
}
JITCFlow ppc_opc_gen_vcmpequhx()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vcmpequhx);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (vrA == vrB) {
		vec_Neg1(vrD);

		if (PPC_OPC_VRc & gJITC.current_opc) {
			jitcClobberCarryAndFlags();

			asmAND(&gCPU.cr, ~CR_CR6);
			asmOR(&gCPU.cr, CR_CR6_EQ | CR_CR6_EQ_SOME);
		}

		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg = REG_NO;
	NativeReg16 reg2 = (NativeReg16)jitcAllocRegister(NATIVE_REG_8);

	if (PPC_OPC_VRc & gJITC.current_opc) {
		reg = jitcAllocRegister();

		asmMOV_NoFlags(reg, CR_CR6_EQ | CR_CR6_NE);
	}

	for (int i=0; i<16; i += 2) {
		vec_getHalf(reg2, vrA, i);

		asmALU(X86_CMP, reg2, x86_mem2(modrm, &(gCPU.vr[vrB].b[i])));

		if (PPC_OPC_VRc & gJITC.current_opc) {
			NativeAddress skip1 = asmJxxFixup(X86_NE);

			asmMOV_NoFlags(reg2, 0xffff);
			asmALU(X86_AND, reg, ~CR_CR6_NE);
			asmALU(X86_OR, reg, CR_CR6_EQ_SOME);
			NativeAddress skip2 = asmJMPFixup();

			asmResolveFixup(skip1);
			asmALU(X86_XOR, reg2, reg2);
			asmALU(X86_AND, reg, ~CR_CR6_EQ);
			asmALU(X86_OR, reg, CR_CR6_NE_SOME);

			asmResolveFixup(skip2, asmHERE());
		} else {
			asmSET(X86_NE, (NativeReg8)reg2);
			asmMOVxx(X86_MOVZX, (NativeReg)reg2, (NativeReg8)reg2);

			asmALU(X86_SUB, reg2, 1);
		}

		vec_setHalf(vrD, i, reg2);
	}

	if (PPC_OPC_VRc & gJITC.current_opc) {
		asmAND(&gCPU.cr, ~CR_CR6);
		asmALU(X86_OR, x86_mem2(modrm, &gCPU.cr), reg);
	}

	return flowContinue;
#endif
}

/*	vcmpequwx	Vector Compare Equal-to Unsigned Word
 *	v.162
 */
void ppc_opc_vcmpequwx()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	int tf=CR_CR6_EQ | CR_CR6_NE;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		if (gCPU.vr[vrA].w[i] == gCPU.vr[vrB].w[i]) {
			gCPU.vr[vrD].w[i] = 0xffffffff;
			tf &= ~CR_CR6_NE;
			tf |= CR_CR6_EQ_SOME;
		} else {
			gCPU.vr[vrD].w[i] = 0;
			tf &= ~CR_CR6_EQ;
			tf |= CR_CR6_NE_SOME;
		}
	}

	if (PPC_OPC_VRc & gCPU.current_opc) {
		gCPU.cr &= ~CR_CR6;
		gCPU.cr |= tf;
	}
}
JITCFlow ppc_opc_gen_vcmpequwx()
{
	ppc_opc_gen_interpret(ppc_opc_vcmpequwx);
	return flowEndBlock;
}

/*	vcmpeqfpx	Vector Compare Equal-to-Floating Point
 *	v.159
 */
void ppc_opc_vcmpeqfpx()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	int tf=CR_CR6_EQ | CR_CR6_NE;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		if (gCPU.vr[vrA].f[i] == gCPU.vr[vrB].f[i]) {
			gCPU.vr[vrD].w[i] = 0xffffffff;
			tf &= ~CR_CR6_NE;
			tf |= CR_CR6_EQ_SOME;
		} else {
			gCPU.vr[vrD].w[i] = 0;
			tf &= ~CR_CR6_EQ;
			tf |= CR_CR6_NE_SOME;
		}
	}

	if (PPC_OPC_VRc & gCPU.current_opc) {
		gCPU.cr &= ~CR_CR6;
		gCPU.cr |= tf;
	}
}
JITCFlow ppc_opc_gen_vcmpeqfpx()
{
	ppc_opc_gen_interpret(ppc_opc_vcmpeqfpx);
	return flowEndBlock;
}

/*	vcmpgtubx	Vector Compare Greater-Than Unsigned Byte
 *	v.168
 */
void ppc_opc_vcmpgtubx()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	int tf=CR_CR6_EQ | CR_CR6_NE;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		if (gCPU.vr[vrA].b[i] > gCPU.vr[vrB].b[i]) {
			gCPU.vr[vrD].b[i] = 0xff;
			tf &= ~CR_CR6_NE;
			tf |= CR_CR6_EQ_SOME;
		} else {
			gCPU.vr[vrD].b[i] = 0;
			tf &= ~CR_CR6_EQ;
			tf |= CR_CR6_NE_SOME;
		}
	}

	if (PPC_OPC_VRc & gCPU.current_opc) {
		gCPU.cr &= ~CR_CR6;
		gCPU.cr |= tf;
	}
}
JITCFlow ppc_opc_gen_vcmpgtubx()
{
	ppc_opc_gen_interpret(ppc_opc_vcmpgtubx);
	return flowEndBlock;
}

/*	vcmpgtsbx	Vector Compare Greater-Than Signed Byte
 *	v.165
 */
void ppc_opc_vcmpgtsbx()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	int tf=CR_CR6_EQ | CR_CR6_NE;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<16; i++) {
		if (gCPU.vr[vrA].sb[i] > gCPU.vr[vrB].sb[i]) {
			gCPU.vr[vrD].b[i] = 0xff;
			tf &= ~CR_CR6_NE;
			tf |= CR_CR6_EQ_SOME;
		} else {
			gCPU.vr[vrD].b[i] = 0;
			tf &= ~CR_CR6_EQ;
			tf |= CR_CR6_NE_SOME;
		}
	}

	if (PPC_OPC_VRc & gCPU.current_opc) {
		gCPU.cr &= ~CR_CR6;
		gCPU.cr |= tf;
	}
}
JITCFlow ppc_opc_gen_vcmpgtsbx()
{
	ppc_opc_gen_interpret(ppc_opc_vcmpgtsbx);
	return flowEndBlock;
}

/*	vcmpgtuhx	Vector Compare Greater-Than Unsigned Half Word
 *	v.169
 */
void ppc_opc_vcmpgtuhx()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	int tf=CR_CR6_EQ | CR_CR6_NE;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		if (gCPU.vr[vrA].h[i] > gCPU.vr[vrB].h[i]) {
			gCPU.vr[vrD].h[i] = 0xffff;
			tf &= ~CR_CR6_NE;
			tf |= CR_CR6_EQ_SOME;
		} else {
			gCPU.vr[vrD].h[i] = 0;
			tf &= ~CR_CR6_EQ;
			tf |= CR_CR6_NE_SOME;
		}
	}

	if (PPC_OPC_VRc & gCPU.current_opc) {
		gCPU.cr &= ~CR_CR6;
		gCPU.cr |= tf;
	}
}
JITCFlow ppc_opc_gen_vcmpgtuhx()
{
#if 0
	ppc_opc_gen_interpret(ppc_opc_vcmpgtuhx);
	return flowEndBlock;
#else
	int vrD, vrA, vrB;
	modrm_o modrm;
	PPC_OPC_TEMPL_X(gJITC.current_opc, vrD, vrA, vrB);

	if (vrA == vrB) {
		vec_Zero(vrD);

		if (PPC_OPC_VRc & gJITC.current_opc) {
			jitcClobberCarryAndFlags();

			asmAND(&gCPU.cr, ~CR_CR6);
			asmOR(&gCPU.cr, CR_CR6_NE | CR_CR6_NE_SOME);
		}

		return flowContinue;
	}

	jitcFlushClientVectorRegister(vrA);
	jitcFlushClientVectorRegister(vrB);
	jitcDropClientVectorRegister(vrD);

	jitcClobberCarryAndFlags();
	NativeReg reg = REG_NO;
	NativeReg16 reg2 = (NativeReg16)jitcAllocRegister(NATIVE_REG_8);

	if (PPC_OPC_VRc & gJITC.current_opc) {
		reg = jitcAllocRegister();

		asmMOV_NoFlags(reg, CR_CR6_EQ | CR_CR6_NE);
	}

	for (int i=0; i<16; i += 2) {
		vec_getHalf(reg2, vrA, i);

		asmALU(X86_CMP, reg2, x86_mem2(modrm, &(gCPU.vr[vrB].b[i])));

		if (PPC_OPC_VRc & gJITC.current_opc) {
			NativeAddress skip1 = asmJxxFixup(X86_BE);

			asmMOV_NoFlags(reg2, 0xffff);
			asmALU(X86_AND, reg, ~CR_CR6_NE);
			asmALU(X86_OR, reg, CR_CR6_EQ_SOME);
			NativeAddress skip2 = asmJMPFixup();

			asmResolveFixup(skip1);
			asmALU(X86_XOR, reg2, reg2);
			asmALU(X86_AND, reg, ~CR_CR6_EQ);
			asmALU(X86_OR, reg, CR_CR6_NE_SOME);

			asmResolveFixup(skip2);
		} else {
			asmSET(X86_BE, (NativeReg8)reg2);
			asmMOVxx(X86_MOVZX, (NativeReg)reg2, (NativeReg8)reg2);

			asmALU(X86_SUB, reg2, 1);
		}

		vec_setHalf(vrD, i, reg2);
	}

	if (PPC_OPC_VRc & gJITC.current_opc) {
		asmAND(&gCPU.cr, ~CR_CR6);
		asmALU(X86_OR, x86_mem2(modrm, &gCPU.cr), reg);
	}

	return flowContinue;
#endif
}

/*	vcmpgtshx	Vector Compare Greater-Than Signed Half Word
 *	v.166
 */
void ppc_opc_vcmpgtshx()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	int tf=CR_CR6_EQ | CR_CR6_NE;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<8; i++) {
		if (gCPU.vr[vrA].sh[i] > gCPU.vr[vrB].sh[i]) {
			gCPU.vr[vrD].h[i] = 0xffff;
			tf &= ~CR_CR6_NE;
			tf |= CR_CR6_EQ_SOME;
		} else {
			gCPU.vr[vrD].h[i] = 0;
			tf &= ~CR_CR6_EQ;
			tf |= CR_CR6_NE_SOME;
		}
	}

	if (PPC_OPC_VRc & gCPU.current_opc) {
		gCPU.cr &= ~CR_CR6;
		gCPU.cr |= tf;
	}
}
JITCFlow ppc_opc_gen_vcmpgtshx()
{
	ppc_opc_gen_interpret(ppc_opc_vcmpgtshx);
	return flowEndBlock;
}

/*	vcmpgtuwx	Vector Compare Greater-Than Unsigned Word
 *	v.170
 */
void ppc_opc_vcmpgtuwx()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	int tf=CR_CR6_EQ | CR_CR6_NE;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		if (gCPU.vr[vrA].w[i] > gCPU.vr[vrB].w[i]) {
			gCPU.vr[vrD].w[i] = 0xffffffff;
			tf &= ~CR_CR6_NE;
			tf |= CR_CR6_EQ_SOME;
		} else {
			gCPU.vr[vrD].w[i] = 0;
			tf &= ~CR_CR6_EQ;
			tf |= CR_CR6_NE_SOME;
		}
	}

	if (PPC_OPC_VRc & gCPU.current_opc) {
		gCPU.cr &= ~CR_CR6;
		gCPU.cr |= tf;
	}
}
JITCFlow ppc_opc_gen_vcmpgtuwx()
{
	ppc_opc_gen_interpret(ppc_opc_vcmpgtuwx);
	return flowEndBlock;
}

/*	vcmpgtswx	Vector Compare Greater-Than Signed Word
 *	v.167
 */
void ppc_opc_vcmpgtswx()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	int tf=CR_CR6_EQ | CR_CR6_NE;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) {
		if (gCPU.vr[vrA].sw[i] > gCPU.vr[vrB].sw[i]) {
			gCPU.vr[vrD].w[i] = 0xffffffff;
			tf &= ~CR_CR6_NE;
			tf |= CR_CR6_EQ_SOME;
		} else {
			gCPU.vr[vrD].w[i] = 0;
			tf &= ~CR_CR6_EQ;
			tf |= CR_CR6_NE_SOME;
		}
	}

	if (PPC_OPC_VRc & gCPU.current_opc) {
		gCPU.cr &= ~CR_CR6;
		gCPU.cr |= tf;
	}
}
JITCFlow ppc_opc_gen_vcmpgtswx()
{
	ppc_opc_gen_interpret(ppc_opc_vcmpgtswx);
	return flowEndBlock;
}

/*	vcmpgtfpx	Vector Compare Greater-Than Floating-Point
 *	v.164
 */
void ppc_opc_vcmpgtfpx()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	int tf=CR_CR6_EQ | CR_CR6_NE;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		if (gCPU.vr[vrA].f[i] > gCPU.vr[vrB].f[i]) {
			gCPU.vr[vrD].w[i] = 0xffffffff;
			tf &= ~CR_CR6_NE;
			tf |= CR_CR6_EQ_SOME;
		} else {
			gCPU.vr[vrD].w[i] = 0;
			tf &= ~CR_CR6_EQ;
			tf |= CR_CR6_NE_SOME;
		}
	}

	if (PPC_OPC_VRc & gCPU.current_opc) {
		gCPU.cr &= ~CR_CR6;
		gCPU.cr |= tf;
	}
}
JITCFlow ppc_opc_gen_vcmpgtfpx()
{
	ppc_opc_gen_interpret(ppc_opc_vcmpgtfpx);
	return flowEndBlock;
}

/*	vcmpgefpx	Vector Compare Greater-Than-or-Equal-to Floating Point
 *	v.163
 */
void ppc_opc_vcmpgefpx()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	int tf=CR_CR6_EQ | CR_CR6_NE;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		if (gCPU.vr[vrA].f[i] >= gCPU.vr[vrB].f[i]) {
			gCPU.vr[vrD].w[i] = 0xffffffff;
			tf &= ~CR_CR6_NE;
			tf |= CR_CR6_EQ_SOME;
		} else {
			gCPU.vr[vrD].w[i] = 0;
			tf &= ~CR_CR6_EQ;
			tf |= CR_CR6_NE_SOME;
		}
	}

	if (PPC_OPC_VRc & gCPU.current_opc) {
		gCPU.cr &= ~CR_CR6;
		gCPU.cr |= tf;
	}
}
JITCFlow ppc_opc_gen_vcmpgefpx()
{
	ppc_opc_gen_interpret(ppc_opc_vcmpgefpx);
	return flowEndBlock;
}

/*	vcmpbfpx	Vector Compare Bounds Floating Point
 *	v.157
 */
void ppc_opc_vcmpbfpx()
{
	VECTOR_DEBUG;
	int vrD, vrA, vrB;
	int le, ge;
	int ib=CR_CR6_NE;
	PPC_OPC_TEMPL_X(gCPU.current_opc, vrD, vrA, vrB);

	for (int i=0; i<4; i++) { //FIXME: This might not comply with Java FP
		le = (gCPU.vr[vrA].f[i] <= gCPU.vr[vrB].f[i]) ? 0 : 0x80000000;
		ge = (gCPU.vr[vrA].f[i] >= -gCPU.vr[vrB].f[i]) ? 0 : 0x40000000;

		gCPU.vr[vrD].w[i] = le | ge;
		if (le | ge) {
			ib = 0;
 		}
	}

	if (PPC_OPC_VRc & gCPU.current_opc) {
		gCPU.cr &= ~CR_CR6;
		gCPU.cr |= ib;
	}
}
JITCFlow ppc_opc_gen_vcmpbfpx()
{
	ppc_opc_gen_interpret(ppc_opc_vcmpbfpx);
	return flowEndBlock;
}
#endif
