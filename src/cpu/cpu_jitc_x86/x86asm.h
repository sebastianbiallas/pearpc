/*
 *	PearPC
 *	x86asm.h
 *
 *	Copyright (C) 2004 Sebastian Biallas (sb@biallas.net)
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

#ifndef __X86ASM_H__
#define __X86ASM_H__

#include "system/types.h"
#include "ppc_cpu.h"
#include "jitc_types.h"

/* FSCALE is also defined in FreeBSD's sys/param.h */
#ifdef FSCALE 
#undef FSCALE 
#endif /* FSCALE */
		
enum NativeReg {
	EAX = 0,
	ECX = 1,
	EDX = 2,
	EBX = 3,
	ESP = 4,	// don't mess with me, buddy
	EBP = 5,
	ESI = 6,
	EDI = 7,
	REG_NO = 0xffffffff,
};

enum NativeReg16 {
	AX = 0,
	CX = 1,
	DX = 2,
	BX = 3,
	SP = 4,	// don't mess with me, buddy
	BP = 5,
	SI = 6,
	DI = 7,
	REG16_NO = 0xffffffff,
};

enum NativeReg8 {
	AL = 0,
	CL = 1,
	DL = 2,
	BL = 3,
	AH = 4,
	CH = 5,
	DH = 6,
	BH = 7,
	REG8_NO = 0xffffffff,
};

#define NATIVE_REG_8	(1<<8)	 // eax,ecx,edx,ebx -> al,cl,dl,bl
#define NATIVE_REG	(2<<8)	 // used as a bitmask to specify register
#define NATIVE_REG_PREFER (4<<8) // used as a bitmask to specify register

#define NATIVE_REGS_ALL 0

struct X86CPUCaps {
	char vendor[13];
	bool rdtsc;
	bool cmov;
	bool mmx;
	bool _3dnow;
	bool _3dnow2;
	bool sse;
	bool sse2;
	bool sse3;
	bool ssse3;
	bool sse4;
	int  loop_align;
};

void x86GetCaps(X86CPUCaps &caps);

NativeReg FASTCALL jitcAllocRegister(int options = 0);
NativeReg FASTCALL jitcDirtyRegister(NativeReg reg);
NativeReg FASTCALL jitcMapClientRegisterDirty(PPC_Register creg, int options = 0);
NativeReg FASTCALL jitcGetClientRegister(PPC_Register creg, int options = 0);
NativeReg FASTCALL jitcGetClientRegisterDirty(PPC_Register reg, int options = 0);
NativeReg FASTCALL jitcGetClientRegisterMapping(PPC_Register creg);

void FASTCALL jitcFlushAll();
void FASTCALL jitcClobberAll();
void FASTCALL jitcInvalidateAll();
void FASTCALL jitcTouchRegister(NativeReg reg);
void FASTCALL jitcFlushRegister(int options = NATIVE_REGS_ALL);
void FASTCALL jitcFlushRegisterDirty(int options = NATIVE_REGS_ALL);
void FASTCALL jitcClobberRegister(int options = NATIVE_REGS_ALL);
void FASTCALL jitcGetClientCarry();
void FASTCALL jitcMapFlagsDirty(PPC_CRx cr = PPC_CR0);
void FASTCALL jitcMapCarryDirty();
void FASTCALL jitcClobberFlags();
void FASTCALL jitcClobberCarry();
void FASTCALL jitcClobberCarryAndFlags();
void FASTCALL jitcFlushCarryAndFlagsDirty(); // ONLY FOR DEBUG! DON'T CALL!

PPC_CRx FASTCALL jitcGetFlagsMapping();

bool FASTCALL jitcFlagsMapped();
bool FASTCALL jitcCarryMapped();

void FASTCALL jitcFlushFlagsAfterCMPL_L(int disp);
void FASTCALL jitcFlushFlagsAfterCMPL_U(int disp);
void FASTCALL jitcFlushFlagsAfterCMP_L(int disp);
void FASTCALL jitcFlushFlagsAfterCMP_U(int disp);

enum X86ALUopc {
	X86_ADC  = 2,
	X86_ADD  = 0,
	X86_AND  = 4,
	X86_MOV  = 8,
	X86_CMP  = 7,
	X86_LEA  = 11,
	X86_OR   = 1,
	X86_SBB  = 3,
	X86_SUB  = 5,
	X86_TEST = 9,
	X86_XCHG = 10,
	X86_XOR  = 6,
};

enum X86ALUopc1 {
	X86_NOT = 0xd0,
	X86_NEG = 0xd8,
	X86_MUL = 0xe0,
	X86_IMUL = 0xe8,
	X86_DIV = 0xf0,
	X86_IDIV = 0xf8,
};
enum X86MOVxx {
	X86_MOVSX = 0xbe,
	X86_MOVZX = 0xb6,
};

enum X86SimpleOpc {
	X86_CBW = 0x9866,
	X86_CWDE = 0x98,
	X86_CWD = 0x9966,
	X86_CDQ = 0x99,
	X86_CMC = 0xf5,
	X86_LAHF = 0x9f,
	X86_PUSHA = 0x60,
	X86_POPA = 0x61,
	X86_PUSHF = 0x9c,
	X86_POPF = 0x9d,
	X86_RET = 0xc3,
	X86_STC = 0xf9,
};

enum X86FlagTest {
	X86_O   = 0,
	X86_NO  = 1,
	X86_B   = 2,
	X86_C   = 2,
	X86_NAE = 2,
	X86_NB  = 3,
	X86_NC  = 3,
	X86_AE  = 3,
	X86_E   = 4,
	X86_Z   = 4,
	X86_NE  = 5,
	X86_NZ  = 5,
	X86_NA  = 6,
	X86_BE  = 6,
	X86_A   = 7,
	X86_NBE = 7,
	X86_S   = 8,
	X86_NS  = 9,
	X86_PE  = 10,
	X86_PO  = 11,
	X86_L   = 12,
	X86_NGE = 12,
	X86_NL  = 13,
	X86_GE	= 13,
	X86_NG  = 14,
	X86_LE	= 14,
	X86_G   = 15,
	X86_NLE = 15,
};

enum X86ShiftOpc {
	X86_ROL = 0x00,
	X86_ROR = 0x08,
	X86_RCL = 0x10,
	X86_RCR = 0x18,
	X86_SHL = 0x20,
	X86_SHR = 0x28,
	X86_SAL = 0x20,
	X86_SAR = 0x38,
};

enum X86BitTest {
	X86_BT  = 4,
	X86_BTC = 7,
	X86_BTR = 6,
	X86_BTS = 5,
};

enum X86BitSearch {
	X86_BSF  = 0xbc,
	X86_BSR  = 0xbd,
};

	NativeAddress FASTCALL asmHERE();

	void FASTCALL asmNOP(int n);
	void FASTCALL asmSimple(X86SimpleOpc simple);

	void FASTCALL asmALU32(X86ALUopc opc, NativeReg reg1, NativeReg reg2);
	void FASTCALL asmALU32(X86ALUopc opc, NativeReg reg, uint32 imm);
	void FASTCALL asmALU32(X86ALUopc opc, NativeReg reg, NativeReg base, uint32 disp);
	void FASTCALL asmALU32(X86ALUopc opc, NativeReg reg, NativeReg base, int scale, NativeReg index, uint32 disp);
	void FASTCALL asmALU32(X86ALUopc opc, NativeReg base, uint32 disp, NativeReg reg);
	void FASTCALL asmALU32(X86ALUopc opc, NativeReg base, int scale, NativeReg index, uint32 disp, NativeReg reg);
	void FASTCALL asmALU32(X86ALUopc opc, NativeReg base, uint32 disp, uint32 imm);
	void FASTCALL asmALU32(X86ALUopc1 opc, NativeReg reg);

	static inline void asmALU32(X86ALUopc opc, const void *mem, uint32 imm)
	{
		asmALU32(opc, REG_NO, uint32(mem), imm);
	}

	static inline void asmALU32(X86ALUopc opc, const void *mem, NativeReg reg)
	{
		asmALU32(opc, REG_NO, uint32(mem), reg);
	}

	static inline void asmALU32(X86ALUopc opc, NativeReg reg, const void *mem)
	{
		asmALU32(opc, reg, REG_NO, uint32(mem));
	}

	void FASTCALL asmMOV32_NoFlags(NativeReg reg, uint32 imm);

	void FASTCALL asmALU16(X86ALUopc opc, NativeReg16 reg, const void *mem);
	void FASTCALL asmALU16(X86ALUopc opc, NativeReg16 reg, uint16 imm);
	void FASTCALL asmALU16(X86ALUopc opc, const void *mem, NativeReg16 reg);

	void FASTCALL asmALU8(X86ALUopc opc, NativeReg8 reg1, NativeReg8 reg2);
	void FASTCALL asmALU8(X86ALUopc opc, NativeReg base, uint32 disp, uint8 imm);
	void FASTCALL asmALU8(X86ALUopc opc, NativeReg8 reg, NativeReg base, uint32 disp);
	void FASTCALL asmALU8(X86ALUopc opc, NativeReg base, uint32 disp, NativeReg8 reg);
	void FASTCALL asmALU8(X86ALUopc opc, NativeReg8 reg1, uint8 imm);
	
	static inline void asmALU8(X86ALUopc opc, const void *mem, uint8 imm)
	{
		asmALU8(opc, REG_NO, uint32(mem), imm);
	}

	static inline void FASTCALL asmALU8(X86ALUopc opc, NativeReg8 reg1, const void *mem)
	{
		asmALU8(opc, reg1, REG_NO, uint32(mem));
	}

	static inline void FASTCALL asmALU8(X86ALUopc opc, const void *mem, NativeReg8 reg1)
	{
		asmALU8(opc, REG_NO, uint32(mem), reg1);
	}

	void FASTCALL asmAND32(NativeReg base, uint32 disp, uint32 imm);
	static inline void asmAND32(const void *mem, uint32 imm) 
	{
		asmAND32(REG_NO, uint32(mem), imm);
	}

	void FASTCALL asmOR32(NativeReg base, uint32 disp, uint32 imm);
	static inline void asmOR32(const void *mem, uint32 imm) 
	{
		asmOR32(REG_NO, uint32(mem), imm);
	}

	void FASTCALL asmTEST32(NativeReg base, uint32 disp, uint32 imm);
	static inline void asmTEST32(const void *mem, uint32 imm) 
	{
		asmTEST32(REG_NO, uint32(mem), imm);
	}
	
	void FASTCALL asmCMOV32(X86FlagTest flags, NativeReg reg1, NativeReg reg2);
	void FASTCALL asmCMOV32(X86FlagTest flags, NativeReg reg, NativeReg base, uint32 disp);
	static inline void asmCMOV32(X86FlagTest flags, NativeReg reg1, const void *mem) 
	{
		asmCMOV32(flags, reg1, REG_NO, uint32(mem));
	}

	void FASTCALL asmSET8(X86FlagTest flags, NativeReg8 regb);
	void FASTCALL asmSET8(X86FlagTest flags, NativeReg base, uint32 disp);
	static inline void asmSET8(X86FlagTest flags, const void *mem)
	{
		asmSET8(flags, REG_NO, uint32(mem));
	}

	void FASTCALL asmMOVxx32_8(X86MOVxx opc, NativeReg reg1, NativeReg8 reg2);
	void FASTCALL asmMOVxx32_8(X86MOVxx opc, NativeReg reg1, const void *mem);
	void FASTCALL asmMOVxx32_16(X86MOVxx opc, NativeReg reg1, NativeReg reg2);
	void FASTCALL asmMOVxx32_16(X86MOVxx opc, NativeReg reg1, const void *mem);
	void FASTCALL asmShift16(X86ShiftOpc opc, NativeReg reg, uint imm);
	void FASTCALL asmShift32(X86ShiftOpc opc, NativeReg reg, uint imm);
	void FASTCALL asmShift8CL(X86ShiftOpc opc, NativeReg8 reg);
	void FASTCALL asmShift16CL(X86ShiftOpc opc, NativeReg16 reg);
	void FASTCALL asmShift32CL(X86ShiftOpc opc, NativeReg reg);
	void FASTCALL asmINC32(NativeReg reg);
	void FASTCALL asmDEC32(NativeReg reg);

	void FASTCALL asmIMUL32(NativeReg reg1, NativeReg reg2, uint32 imm);
	void FASTCALL asmIMUL32(NativeReg reg1, NativeReg reg2);

	void FASTCALL asmBTx32(X86BitTest opc, NativeReg reg, int value);
	void FASTCALL asmBTx32(X86BitTest opc, NativeReg base, uint32 disp, int value);
	void FASTCALL asmBTx32(X86BitTest opc, const void *mem, int value);
	void FASTCALL asmBSx32(X86BitSearch opc, NativeReg reg1, NativeReg reg2);

	void FASTCALL asmBSWAP32(NativeReg reg);

	void FASTCALL asmJMP(NativeAddress to);
	void FASTCALL asmJxx(X86FlagTest flags, NativeAddress to);
	NativeAddress FASTCALL asmJMPFixup();
	NativeAddress FASTCALL asmJxxFixup(X86FlagTest flags);
	void FASTCALL asmCALL(NativeAddress to);
 
	void FASTCALL asmResolveFixup(NativeAddress at, NativeAddress to=0);

enum NativeFloatReg {
	Float_ST0=0,
	Float_ST1=1,
	Float_ST2=2,
	Float_ST3=3,
	Float_ST4=4,
	Float_ST5=5,
	Float_ST6=6,
	Float_ST7=7,
};

#define X86_FLOAT_ST(i) (NativeFloatReg(i))

typedef int JitcFloatReg;
#define JITC_FLOAT_REG_NONE 0

NativeFloatReg	FASTCALL jitcFloatRegisterToNative(JitcFloatReg r);
bool		FASTCALL jitcFloatRegisterIsTOP(JitcFloatReg r);
JitcFloatReg	FASTCALL jitcFloatRegisterXCHGToFront(JitcFloatReg r);
JitcFloatReg	FASTCALL jitcFloatRegisterDirty(JitcFloatReg r);
void		FASTCALL jitcFloatRegisterInvalidate(JitcFloatReg r);
JitcFloatReg	FASTCALL jitcFloatRegisterDup(JitcFloatReg r, JitcFloatReg hint=JITC_FLOAT_REG_NONE);
void		FASTCALL jitcFloatRegisterClobberAll();
void		FASTCALL jitcFloatRegisterStoreAndPopTOP(JitcFloatReg r);

void		FASTCALL jitcPopFloatStack(JitcFloatReg hint1, JitcFloatReg hint2);
void		FASTCALL jitcClobberClientRegisterForFloat(int creg);
void		FASTCALL jitcInvalidateClientRegisterForFloat(int creg);
JitcFloatReg	FASTCALL jitcGetClientFloatRegisterMapping(int creg);
JitcFloatReg	FASTCALL jitcGetClientFloatRegister(int creg, JitcFloatReg hint1=JITC_FLOAT_REG_NONE, JitcFloatReg hint2=JITC_FLOAT_REG_NONE);
JitcFloatReg	FASTCALL jitcGetClientFloatRegisterUnmapped(int creg, JitcFloatReg hint1=JITC_FLOAT_REG_NONE, JitcFloatReg hint2=JITC_FLOAT_REG_NONE);
JitcFloatReg	FASTCALL jitcMapClientFloatRegisterDirty(int creg, JitcFloatReg freg=JITC_FLOAT_REG_NONE);

enum X86FloatFlagTest {
	X86_FB=0,
	X86_FE=1,
	X86_FBE=2,
	X86_FU=3,
	X86_FNB=4,
	X86_FNE=5,
	X86_FNBE=6,
	X86_FNU=7,
};

enum X86FloatArithOp {
	X86_FADD = 0xc0,  // .238

	// st(i)/st(0)
	X86_FDIV = 0xf8,  //  .261

	// st(0)/st(i)
	X86_FDIVR = 0xf0,  //  .265

	X86_FMUL = 0xc8,  // .288

	// st(i) - st(0)
	X86_FSUB = 0xe8,  //  .327

	// st(0) - st(i)
	X86_FSUBR = 0xe0,  //  .330
};

enum X86FloatCompOp {
	//dbf0+i
	X86_FCOMI = 0xf0db,  // .255

	//dff0+i
	X86_FCOMIP = 0xf0df,  // .255

	//dbe8+i
	X86_FUCOMI = 0xe8db, // .255 
	//dfe8+i
	X86_FUCOMIP = 0xe8df, // .255 
};
	
enum X86FloatICompOp {
	X86_FICOM16 = 0xde,
	X86_FICOM32 = 0xda,
};

enum X86FloatOp {
	FABS = 0xe1d9,
	FCOMPP = 0xd9de, // .252
	FCHS = 0xe0d9, // .246
	FLD1 = 0xe8d9, // .282
	FLDL2T = 0xe9d9, // .282
	FLDL2E = 0xead9, // .282
	FLDPI = 0xebd9, // .282
	FLDLG2 = 0xecd9, // .282
	FLDLN2 = 0xedd9, // .282
	FLDZ = 0xeed9, // .282
	FRNDINT = 0xfcd9,
	FSQRT = 0xfad9, // .314
	F2XM1 = 0xf0d9, // .236
	FYL2X = 0xf1d9, // .353
	FYL2XP1 = 0xf9d9, // .355
	FSCALE = 0xfdd9, // .308
	FTST = 0xe4d9, // .333
};

// .250 FCMOVcc
// .277 FISTP [mem32]  0xDB /3

void FASTCALL asmFSTSW_EAX(void);

void FASTCALL asmFComp(X86FloatCompOp op, NativeFloatReg sti);
void FASTCALL asmFArith(X86FloatArithOp op, const void *mem);
void FASTCALL asmFArith_ST0(X86FloatArithOp op, NativeFloatReg sti);
void FASTCALL asmFArith_STi(X86FloatArithOp op, NativeFloatReg sti);
void FASTCALL asmFArithP_STi(X86FloatArithOp op, NativeFloatReg sti);
void FASTCALL asmFXCH(NativeFloatReg sti);
void FASTCALL asmFFREE(NativeFloatReg sti);
void FASTCALL asmFFREEP(NativeFloatReg sti);
void FASTCALL asmFSimple(X86FloatOp op);
void FASTCALL asmFLD(NativeFloatReg sti);
void FASTCALL asmFST(NativeFloatReg sti);
void FASTCALL asmFSTP(NativeFloatReg sti);
void FASTCALL asmFISTTP(const void *mem);
void FASTCALL asmFISTP_D(const void *mem);

void FASTCALL asmFLD_Single(const void *mem);
void FASTCALL asmFLD_Double(const void *mem);
void FASTCALL asmFILD_W(const void *mem);
void FASTCALL asmFILD_D(const void *mem);
void FASTCALL asmFILD_Q(const void *mem);
void FASTCALL asmFISTP_Q(const void *mem);
void FASTCALL asmFIComp(X86FloatICompOp op, const void *mem);
void FASTCALL asmFICompP(X86FloatICompOp op, const void *mem);
void FASTCALL asmFSTP_Single(const void *mem);
void FASTCALL asmFSTP_Double(const void *mem);
/*
void FASTCALL asmFST_Single(modrm_p modrm);
void FASTCALL asmFST_Double(modrm_p modrm);
void FASTCALL asmFISTP_W(modrm_p modrm);
void FASTCALL asmFSTSW(modrm_p modrm);
*/
void FASTCALL asmFSTSW_EAX(void);

void FASTCALL asmFLDCW(const void *mem);
void FASTCALL asmFSTCW(const void *mem);

enum NativeVectorReg {
	XMM0 = 0,
	XMM1 = 1,
	XMM2 = 2,
	XMM3 = 3,
	XMM4 = 4,
	XMM5 = 5,
	XMM6 = 6,
	XMM7 = 7,
	XMM_SENTINEL = 8,
	VECTREG_NO = 0xffffffff,
};

enum X86ALUPSopc {
	X86_ANDPS  = 0x54,
	X86_ANDNPS = 0x55,
	X86_ORPS   = 0x56,
	X86_XORPS  = 0x57,
	X86_MOVAPS = 0x28,
	X86_MOVUPS = 0x10,
	X86_ADDPS = 0x58,
	X86_DIVPS = 0x53,
	X86_MAXPS = 0x5F,
	X86_MINPS = 0x5D,
	X86_MULPS = 0x59,
	X86_RCPPS = 0x53,
	X86_RSQRTPS = 0x52,
	X86_SQRTPS = 0x51,
	X86_SUBPS = 0x5C,
	X86_UNPCKLPS = 0x14,
	X86_UNPCKHPS = 0x15,
};

enum X86PALUopc {
	X86_PACKSSWB = 0x63,	// Do *NOT* use PALU*() macros on these
	X86_PACKUSWB = 0x67,
	X86_PACKSSDW = 0x6B,
	X86_PMULLW   = 0xD5,
	X86_PMINUB   = 0xDA,
	X86_PMAXUB   = 0xDE,
	X86_PAVGB    = 0xE0,
        X86_PAVGW    = 0xE3,
	X86_PMULHUW  = 0xE4,
	X86_PMULHW   = 0xE5,
	X86_PMINSW   = 0xEA,
	X86_PMAXSW   = 0xEE,

	X86_PAND    = 0xDB,
	X86_PANDN   = 0xDF,
	X86_POR     = 0xEB,
	X86_PXOR    = 0xEF,

	X86_PUNPCKL = 0x60,
	X86_PCMPGT  = 0x64,
	X86_PUNPCKH = 0x68,
	X86_PCMPEQ  = 0x74,
	X86_PSRL    = 0xD0,
	X86_PSUBUS  = 0xD8,
	X86_PADDUS  = 0xDC,
	X86_PSRA    = 0xE0,
	X86_PSUBS   = 0xE8,
	X86_PADDS   = 0xEC,
	X86_PSLL    = 0xF0,
	X86_PSUB    = 0xF8,
	X86_PADD    = 0xFC,
};

#define PALUB(op)	((X86PALUopc)((op) | 0x00))
#define PALUW(op)	((X86PALUopc)((op) | 0x01))
#define PALUD(op)	((X86PALUopc)((op) | 0x02))
#define PALUQ(op)	((X86PALUopc)((op) | 0x03))

#define X86_VECTOR_VR(i) ((NativeVectorReg)(i))
typedef int JitcVectorReg;

#define JITC_VECTOR_REGS_ALL 0

#define JITC_VECTOR_TEMP	32
#define JITC_VECTOR_NEG1	33

#define PPC_VECTREG_NO		0xffffffff

NativeVectorReg FASTCALL jitcAllocVectorRegister(int hint=0);
void FASTCALL jitcDirtyVectorRegister(NativeVectorReg nreg);
void FASTCALL jitcTouchVectorRegister(NativeVectorReg nreg);

int FASTCALL jitcAssertFlushedVectorRegister(JitcVectorReg creg);
int FASTCALL jitcAssertFlushedVectorRegisters();
void FASTCALL jitcShowVectorRegisterStatus(JitcVectorReg creg);

NativeVectorReg FASTCALL jitcMapClientVectorRegisterDirty(JitcVectorReg creg, int hint=0);
NativeVectorReg FASTCALL jitcGetClientVectorRegister(JitcVectorReg creg, int hint=0);
NativeVectorReg FASTCALL jitcGetClientVectorRegisterDirty(JitcVectorReg creg, int hint=0);
NativeVectorReg FASTCALL jitcGetClientVectorRegisterMapping(JitcVectorReg creg);
NativeVectorReg FASTCALL jitcRenameVectorRegisterDirty(NativeVectorReg reg, JitcVectorReg creg, int hint=0);

void FASTCALL jitcFlushVectorRegister(int options=0);
void FASTCALL jitcFlushVectorRegisterDirty(int options=0);
void FASTCALL jitcClobberVectorRegister(int options=0);
void FASTCALL jitcTrashVectorRegister(int options=0);
void FASTCALL jitcDropVectorRegister(int options=0);

void FASTCALL jitcFlushClientVectorRegister(JitcVectorReg creg);
void FASTCALL jitcTrashClientVectorRegister(JitcVectorReg creg);
void FASTCALL jitcClobberClientVectorRegister(JitcVectorReg creg);
void FASTCALL jitcDropClientVectorRegister(JitcVectorReg creg);

void FASTCALL asmMOVAPS(NativeVectorReg reg, const void *disp);
void FASTCALL asmMOVAPS(const void *disp, NativeVectorReg reg);
void FASTCALL asmMOVUPS(NativeVectorReg reg, const void *disp);
void FASTCALL asmMOVUPS(const void *disp, NativeVectorReg reg);
void FASTCALL asmMOVSS(NativeVectorReg reg, const void *disp);
void FASTCALL asmMOVSS(const void *disp, NativeVectorReg reg);

void FASTCALL asmALUPS(X86ALUPSopc opc, NativeVectorReg reg1, NativeVectorReg reg2);
void FASTCALL asmALUPS(X86ALUPSopc opc, NativeVectorReg reg1, NativeReg base, uint32 disp);
static inline void asmALUPS(X86ALUPSopc opc, NativeVectorReg reg1, const void *mem)
{
	asmALUPS(opc, reg1, REG_NO, uint32(mem));
}
void FASTCALL asmPALU(X86PALUopc opc, NativeVectorReg reg1, NativeVectorReg reg2);
void FASTCALL asmPALU(X86PALUopc opc, NativeVectorReg reg1, const void *mem);

void FASTCALL asmSHUFPS(NativeVectorReg reg1, NativeVectorReg reg2, int order);
void FASTCALL asmSHUFPS(NativeVectorReg reg1, const void *mem, int order);
void FASTCALL asmPSHUFD(NativeVectorReg reg1, NativeVectorReg reg2, int order);
void FASTCALL asmPSHUFD(NativeVectorReg reg1, const void *mem, int order);

#endif
