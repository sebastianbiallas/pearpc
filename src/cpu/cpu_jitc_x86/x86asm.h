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
		
typedef byte modrm_o[8];
typedef byte *modrm_p;

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
	X86_OR   = 1,
	X86_SBB  = 3,
	X86_SUB  = 5,
	X86_TEST = 9,
	X86_XCHG = 10,
	X86_XOR  = 6,
};

enum X86ALUopc1 {
	X86_NOT,
	X86_NEG,
	X86_MUL,
	X86_IMUL,
	X86_DIV,
	X86_IDIV,
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

NativeAddress FASTCALL asmHERE();

#ifndef X86ASM_V2_ONLY
/* Begin: X86Asm v1.0 */
void FASTCALL asmALURegReg(X86ALUopc opc, NativeReg reg1, NativeReg reg2);
void FASTCALL asmALURegImm(X86ALUopc opc, NativeReg reg1, uint32 imm);
void FASTCALL asmALUMemReg(X86ALUopc opc, byte *modrm, int len, NativeReg reg2);
void FASTCALL asmALUMemImm(X86ALUopc opc, byte *modrm, int len, uint32 imm);
void FASTCALL asmALURegMem(X86ALUopc opc, NativeReg reg1, byte *modrm, int len);
void FASTCALL asmALUReg(X86ALUopc1 opc, NativeReg reg1);
void FASTCALL asmALURegReg16(X86ALUopc opc, NativeReg reg1, NativeReg reg2);
void FASTCALL asmALURegImm16(X86ALUopc opc, NativeReg reg1, uint32 imm);
void FASTCALL asmALUMemReg16(X86ALUopc opc, byte *modrm, int len, NativeReg reg2);
void FASTCALL asmALUMemImm16(X86ALUopc opc, byte *modrm, int len, uint32 imm);
void FASTCALL asmALURegMem16(X86ALUopc opc, NativeReg reg1, byte *modrm, int len);
void FASTCALL asmALUReg16(X86ALUopc1 opc, NativeReg reg1);
void FASTCALL asmMOVRegImm_NoFlags(NativeReg reg1, uint32 imm);
void FASTCALL asmMOVRegImm16_NoFlags(NativeReg reg1, uint16 imm);
void FASTCALL asmCMOVRegReg(X86FlagTest flags, NativeReg reg1, NativeReg reg2);
void FASTCALL asmCMOVRegMem(X86FlagTest flags, NativeReg reg1, byte *modrm, int len);
void FASTCALL asmSETReg8(X86FlagTest flags, NativeReg8 reg1);
void FASTCALL asmSETMem(X86FlagTest flags, byte *modrm, int len);
void FASTCALL asmALURegReg8(X86ALUopc opc, NativeReg8 reg1, NativeReg8 reg2);
void FASTCALL asmALURegImm8(X86ALUopc opc, NativeReg8 reg1, uint8 imm);
void FASTCALL asmALURegMem8(X86ALUopc opc, NativeReg8 reg1, byte *modrm, int len);
void FASTCALL asmALUMemReg8(X86ALUopc opc, byte *modrm, int len, NativeReg8 reg2);
void FASTCALL asmALUMemImm8(X86ALUopc opc, byte *modrm, int len, uint8 imm);
void FASTCALL asmMOVDMemReg(uint32 disp, NativeReg reg1);
void FASTCALL asmMOVDMemReg16(uint32 disp, NativeReg reg1);
void FASTCALL asmMOVRegDMem(NativeReg reg1, uint32 disp);
void FASTCALL asmMOVRegDMem16(NativeReg reg1, uint32 disp);
void FASTCALL asmTESTDMemImm(uint32 disp, uint32 imm);
void FASTCALL asmANDDMemImm(uint32 disp, uint32 imm);
void FASTCALL asmORDMemImm(uint32 disp, uint32 imm);
void FASTCALL asmMOVxxRegReg8(X86MOVxx opc, NativeReg reg1, NativeReg8 reg2);
void FASTCALL asmMOVxxRegReg16(X86MOVxx opc, NativeReg reg1, NativeReg reg2);
void FASTCALL asmMOVxxRegMem8(X86MOVxx opc, NativeReg reg1, byte *modrm, int len);
void FASTCALL asmMOVxxRegMem16(X86MOVxx opc, NativeReg reg1, byte *modrm, int len);
/* END: X86Asm v1.0 */
#endif // X86ASM_V2_ONLY

/* BEGIN: X86Asm v2.0 */
void FASTCALL asmNOP(int n);	// v2.0 also
void FASTCALL asmALU(X86ALUopc opc, NativeReg reg1, NativeReg reg2);
void FASTCALL asmALU(X86ALUopc opc, NativeReg reg1, uint32 imm);
void FASTCALL asmALU(X86ALUopc opc, modrm_p modrm, NativeReg reg2);
void FASTCALL asmALU_D(X86ALUopc opc, modrm_p modrm, uint32 imm);
void FASTCALL asmALU(X86ALUopc opc, NativeReg reg1, modrm_p modrm);
void FASTCALL asmALU(X86ALUopc1 opc, NativeReg reg1);
                                                                                
void FASTCALL asmALU(X86ALUopc opc, NativeReg16 reg1, NativeReg16 reg2);
void FASTCALL asmALU(X86ALUopc opc, NativeReg16 reg1, uint16 imm);
void FASTCALL asmALU(X86ALUopc opc, modrm_p modrm, NativeReg16 reg2);
void FASTCALL asmALU_W(X86ALUopc opc, modrm_p modrm, uint16 imm);
void FASTCALL asmALU(X86ALUopc opc, NativeReg16 reg1, modrm_p modrm);
void FASTCALL asmALU(X86ALUopc1 opc, NativeReg16 reg1);
                                                                                
void FASTCALL asmMOV_NoFlags(NativeReg reg1, uint32 imm);
void FASTCALL asmMOV_NoFlags(NativeReg16 reg1, uint16 imm);
void FASTCALL asmCMOV(X86FlagTest flags, NativeReg reg1, NativeReg reg2);
void FASTCALL asmCMOV(X86FlagTest flags, NativeReg reg1, modrm_p modrm);
                                                                                
void FASTCALL asmSET(X86FlagTest flags, NativeReg8 reg1);
void FASTCALL asmSET(X86FlagTest flags, modrm_p modrm);
                                                                                
void FASTCALL asmALU(X86ALUopc opc, NativeReg8 reg1, NativeReg8 reg2);
void FASTCALL asmALU(X86ALUopc opc, NativeReg8 reg1, uint8 imm);
void FASTCALL asmALU(X86ALUopc opc, NativeReg8 reg1, modrm_p modrm);
void FASTCALL asmALU(X86ALUopc opc, modrm_p modrm, NativeReg8 reg2);
void FASTCALL asmALU_B(X86ALUopc opc, modrm_p modrm, uint8 imm);
                                                                                
void FASTCALL asmMOV(const void *disp, NativeReg reg1);
void FASTCALL asmMOV(const void *disp, NativeReg16 reg1);
void FASTCALL asmMOV(NativeReg reg1, const void *disp);
void FASTCALL asmMOV(NativeReg16 reg1, const void *disp);
                                                                                
void FASTCALL asmTEST(const void *disp, uint32 imm);
void FASTCALL asmAND(const void *disp, uint32 imm);
void FASTCALL asmOR(const void *disp, uint32 imm);
                                                                                
void FASTCALL asmMOVxx(X86MOVxx opc, NativeReg reg1, NativeReg8 reg2);
void FASTCALL asmMOVxx(X86MOVxx opc, NativeReg reg1, NativeReg16 reg2);
void FASTCALL asmMOVxx_B(X86MOVxx opc, NativeReg reg1, modrm_p modrm);
void FASTCALL asmMOVxx_W(X86MOVxx opc, NativeReg reg1, modrm_p modrm);
void FASTCALL asmSimple(X86SimpleOpc simple);
/* End: X86Asm v2.0 */

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

#ifndef X86ASM_V2_ONLY
/* Begin: X86Asm v1.0 */
void FASTCALL asmShiftRegImm(X86ShiftOpc opc, NativeReg reg1, uint32 imm);
void FASTCALL asmShiftRegCL(X86ShiftOpc opc, NativeReg reg1);
void FASTCALL asmShiftReg16Imm(X86ShiftOpc opc, NativeReg reg1, uint32 imm);
void FASTCALL asmShiftReg16CL(X86ShiftOpc opc, NativeReg reg1);
void FASTCALL asmShiftReg8Imm(X86ShiftOpc opc, NativeReg8 reg1, uint32 imm);
void FASTCALL asmShiftReg8CL(X86ShiftOpc opc, NativeReg8 reg1);
void FASTCALL asmINCReg(NativeReg reg1);
void FASTCALL asmDECReg(NativeReg reg1);

void FASTCALL asmIMULRegRegImm(NativeReg reg1, NativeReg reg2, uint32 imm);
void FASTCALL asmIMULRegReg(NativeReg reg1, NativeReg reg2);

void FASTCALL asmLEA(NativeReg reg1, byte *modrm, int len);
void FASTCALL asmBTxRegImm(X86BitTest opc, NativeReg reg1, int value);
void FASTCALL asmBTxMemImm(X86BitTest opc, byte *modrm, int len, int value);
void FASTCALL asmBSxRegReg(X86BitSearch opc, NativeReg reg1, NativeReg reg2);
/* End: X86Asm v1.0 */
#endif // X86ASM_V2_ONLY

/* Begin: X86Asm v2.0 */
void FASTCALL asmShift(X86ShiftOpc opc, NativeReg reg1, uint32 imm);
void FASTCALL asmShift_CL(X86ShiftOpc opc, NativeReg reg1);
void FASTCALL asmShift(X86ShiftOpc opc, NativeReg16 reg1, uint32 imm);
void FASTCALL asmShift_CL(X86ShiftOpc opc, NativeReg16 reg1);
void FASTCALL asmShift(X86ShiftOpc opc, NativeReg8 reg1, uint32 imm);
void FASTCALL asmShift_CL(X86ShiftOpc opc, NativeReg8 reg1);
void FASTCALL asmINC(NativeReg reg1);
void FASTCALL asmDEC(NativeReg reg1);

void FASTCALL asmIMUL(NativeReg reg1, NativeReg reg2, uint32 imm);
void FASTCALL asmIMUL(NativeReg reg1, NativeReg reg2);

void FASTCALL asmLEA(NativeReg reg1, modrm_p modrm);
void FASTCALL asmBTx(X86BitTest opc, NativeReg reg1, int value);
void FASTCALL asmBTx(X86BitTest opc, modrm_p modrm, int value);
void FASTCALL asmBSx(X86BitSearch opc, NativeReg reg1, NativeReg reg2);
/* End: X86Asm v2.0 */

void FASTCALL asmBSWAP(NativeReg reg);

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

#define X86_FLOAT_ST(i) ((NativeFloatReg)(i))

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
JitcFloatReg	FASTCALL jitcGetClientFloatRegister(int creg, JitcFloatReg hint1=JITC_FLOAT_REG_NONE, JitcFloatReg hint1=JITC_FLOAT_REG_NONE);
JitcFloatReg	FASTCALL jitcGetClientFloatRegisterUnmapped(int creg, JitcFloatReg hint1=JITC_FLOAT_REG_NONE, JitcFloatReg hint1=JITC_FLOAT_REG_NONE);
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

#ifndef X86ASM_V2_ONLY
/* Begin: X86Asm v1.0 */
void FASTCALL asmFCompSTi(X86FloatCompOp op, NativeFloatReg sti);
void FASTCALL asmFICompMem(X86FloatICompOp op, byte *modrm, int len);
void FASTCALL asmFICompPMem(X86FloatICompOp op, byte *modrm, int len);
void FASTCALL asmFArithMem(X86FloatArithOp op, byte *modrm, int len);
void FASTCALL asmFArithST0(X86FloatArithOp op, NativeFloatReg sti);
void FASTCALL asmFArithSTi(X86FloatArithOp op, NativeFloatReg sti);
void FASTCALL asmFArithSTiP(X86FloatArithOp op, NativeFloatReg sti);
void FASTCALL asmFXCHSTi(NativeFloatReg sti);
void FASTCALL asmFFREESTi(NativeFloatReg sti);
void FASTCALL asmFFREEPSTi(NativeFloatReg sti);
void FASTCALL asmFSimpleST0(X86FloatOp op);
void FASTCALL asmFLDSingleMem(byte *modrm, int len);
void FASTCALL asmFLDDoubleMem(byte *modrm, int len);
void FASTCALL asmFLDSTi(NativeFloatReg sti);
void FASTCALL asmFILD16(byte *modrm, int len);
void FASTCALL asmFILD(byte *modrm, int len);
void FASTCALL asmFSTSingleMem(byte *modrm, int len);
void FASTCALL asmFSTPSingleMem(byte *modrm, int len);
void FASTCALL asmFSTDoubleMem(byte *modrm, int len);
void FASTCALL asmFSTPDoubleMem(byte *modrm, int len);
void FASTCALL asmFSTDSTi(NativeFloatReg sti);
void FASTCALL asmFSTDPSTi(NativeFloatReg sti);
void FASTCALL asmFISTPMem(byte *modrm, int len);
void FASTCALL asmFISTPMem64(byte *modrm, int len);
void FASTCALL asmFISTTPMem(byte *modrm, int len);
                                                                                
void FASTCALL asmFSTSWMem(byte *modrm, int len);
void FASTCALL asmFSTSW_EAX(void);
                                                                                
void FASTCALL asmFLDCWMem(byte *modrm, int len);
void FASTCALL asmFSTCWMem(byte *modrm, int len);
/* End: X86Asm v1.0 */
#endif // X86ASM_V2_ONLY

/* Begin: X86Asm v2.0 */
void FASTCALL asmFComp(X86FloatCompOp op, NativeFloatReg sti);
void FASTCALL asmFIComp(X86FloatICompOp op, modrm_p modrm);
void FASTCALL asmFICompP(X86FloatICompOp op, modrm_p modrm);
void FASTCALL asmFArith(X86FloatArithOp op, modrm_p modrm);
void FASTCALL asmFArith_ST0(X86FloatArithOp op, NativeFloatReg sti);
void FASTCALL asmFArith_STi(X86FloatArithOp op, NativeFloatReg sti);
void FASTCALL asmFArithP_STi(X86FloatArithOp op, NativeFloatReg sti);
void FASTCALL asmFXCH(NativeFloatReg sti);
void FASTCALL asmFFREE(NativeFloatReg sti);
void FASTCALL asmFFREEP(NativeFloatReg sti);
void FASTCALL asmFSimple(X86FloatOp op);
void FASTCALL asmFLD_Single(modrm_p modrm);
void FASTCALL asmFLD_Double(modrm_p modrm);
void FASTCALL asmFLD(NativeFloatReg sti);
void FASTCALL asmFILD_W(modrm_p modrm);
void FASTCALL asmFILD_D(modrm_p modrm);
void FASTCALL asmFILD_Q(modrm_p modrm);
void FASTCALL asmFST_Single(modrm_p modrm);
void FASTCALL asmFSTP_Single(modrm_p modrm);
void FASTCALL asmFST_Double(modrm_p modrm);
void FASTCALL asmFSTP_Double(modrm_p modrm);
void FASTCALL asmFST(NativeFloatReg sti);
void FASTCALL asmFSTP(NativeFloatReg sti);
void FASTCALL asmFISTP_W(modrm_p modrm);
void FASTCALL asmFISTP_D(modrm_p modrm);
void FASTCALL asmFISTP_Q(modrm_p modrm);
void FASTCALL asmFISTTP(modrm_p modrm);

void FASTCALL asmFSTSW(modrm_p modrm);
void FASTCALL asmFSTSW_EAX(void);

void FASTCALL asmFLDCW(modrm_p modrm);
void FASTCALL asmFSTCW(modrm_p modrm);
/* End: X86Asm v2.0 */

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

void asmMOVAPS(NativeVectorReg reg, const void *disp);
void asmMOVAPS(const void *disp, NativeVectorReg reg);
void asmMOVUPS(NativeVectorReg reg, const void *disp);
void asmMOVUPS(const void *disp, NativeVectorReg reg);
void asmMOVSS(NativeVectorReg reg, const void *disp);
void asmMOVSS(const void *disp, NativeVectorReg reg);

void asmALUPS(X86ALUPSopc opc, NativeVectorReg reg1, NativeVectorReg reg2);
void asmALUPS(X86ALUPSopc opc, NativeVectorReg reg1, modrm_p modrm);
void asmPALU(X86PALUopc opc, NativeVectorReg reg1, NativeVectorReg reg2);
void asmPALU(X86PALUopc opc, NativeVectorReg reg1, modrm_p modrm);

void asmSHUFPS(NativeVectorReg reg1, NativeVectorReg reg2, int order);
void asmSHUFPS(NativeVectorReg reg1, modrm_p modrm, int order);
void asmPSHUFD(NativeVectorReg reg1, NativeVectorReg reg2, int order);
void asmPSHUFD(NativeVectorReg reg1, modrm_p modrm, int order);

#ifndef X86ASM_V2_ONLY
/*
 *	reg1 must not be ESP
 */
static inline int x86_mem_r(byte *modrm, NativeReg reg, uint32 disp)
{
	if (((uint32)(disp) > 0x7f) && ((uint32)(disp) < 0xffffff80)) {
/*		if (reg == ESP) {
			modrm[0] = 0x84;
			modrm[1] = 0x24;
			*((uint32 *)&modrm[2]) = disp;
			return 6;
		}*/
		modrm[0] = 0x80+reg;
		*((uint32 *)&modrm[1]) = disp;
		return 5;
	} else if (reg == EBP) {
		modrm[0] = 0x45;
		modrm[1] = disp;
		return 2;
/*	} else if (reg == ESP) {
		if (disp) {
			modrm[0] = 0x44;
			modrm[1] = 0x24;
			modrm[2] = disp;
			return 3;
		} else {
			modrm[0] = 0x04;
			modrm[1] = 0x24;
			return 2;
		} */
	} else if (disp) {
		modrm[0] = 0x40+reg;
		modrm[1] = disp;
		return 2;
	} else {
		modrm[0] = reg;
		return 1;
	}
}

static inline int x86_mem(byte *modrm, NativeReg reg, uint32 disp)
{		
	if (reg == REG_NO) {
		modrm[0] = 0x05;
		*((uint32 *)&modrm[1]) = disp;
		return 5;
	} else return x86_mem_r(modrm, reg, disp);
}

/*
 *	reg1, reg2 must not be ESP
 */
static inline int x86_mem_sib_r(byte *modrm, NativeReg reg1, int factor, NativeReg reg2, uint32 disp=0)
{
	switch (factor) {
		case 1:
		case 4:
		case 8: // ok
			break;
		case 2: if (reg1 == REG_NO) {
				// [eax+eax] is shorter than [eax*2+0]
				reg1 = reg2;
				factor = 1;
			}
			break;
		case 3:
		case 5:
		case 9: // [eax*(2^n+1)] -> [eax+eax*2^n]
			if (reg1 != REG_NO) { /* internal error */ }
			reg1 = reg2;
			factor--;
			break;
		default: 
			/* internal error */
			break;
	}
	//                              0     1     2  3     4  5  6  7     8
	static const byte factors[9] = {0, 0x00, 0x40, 0, 0x80, 0, 0, 0, 0xc0};
	if (reg1 == REG_NO) {
		// [eax*4+disp]
		modrm[0] = 0x04;
		modrm[1] = factors[factor]+(reg2<<3)+EBP;
		*((uint32 *)&modrm[2]) = disp;
		return 6;
	} else if (((uint32)(disp) > 0x7f) && ((uint32)(disp) < 0xffffff80)) {
		modrm[0] = 0x84;
		modrm[1] = factors[factor]+(reg2<<3)+reg1;
		*((uint32 *)&modrm[2]) = disp;
		return 6;
	} else if (disp || reg1 == EBP) {
		modrm[0] = 0x44;
		modrm[1] = factors[factor]+(reg2<<3)+reg1;
		modrm[2] = disp;
		return 3;
	} else {
		modrm[0] = 0x04;
		modrm[1] = factors[factor]+(reg2<<3)+reg1;
		return 2;
	}
}

/*
 *	reg1, reg2 must not be ESP
 */
static inline int x86_mem_sib(byte *modrm, NativeReg reg1, int factor, NativeReg reg2, uint32 disp=0)
{
	if (reg2 == REG_NO) return x86_mem(modrm, reg1, disp);
	return x86_mem_sib_r(modrm, reg1, factor, reg2, disp);
}

#endif // X86ASM_V2_ONLY

/*
 *	reg1 must not be ESP
 */
static inline modrm_p x86_mem2_r(modrm_o modrm, NativeReg reg, uint32 disp)
{
	if (((uint32)(disp) > 0x7f) && ((uint32)(disp) < 0xffffff80)) {
/*		if (reg == ESP) {
			modrm[0] = 6;
			modrm[1] = 0x84;
			modrm[2] = 0x24;
			*((uint32 *)&modrm[3]) = disp;
			return modrm;
		}*/
		modrm[0] = 5;
		modrm[1] = 0x80+reg;
		*((uint32 *)&modrm[2]) = disp;
		return modrm;
	} else if (reg == EBP) {
		modrm[0] = 2;
		modrm[1] = 0x45;
		modrm[2] = disp;
		return modrm;
/*	} else if (reg == ESP) {
		if (disp) {
			modrm[0] = 3;
			modrm[1] = 0x44;
			modrm[2] = 0x24;
			modrm[3] = disp;
			return modrm;
		} else {
			modrm[0] = 2;
			modrm[1] = 0x04;
			modrm[2] = 0x24;
			return modrm;
		} */
	} else if (disp) {
		modrm[0] = 2;
		modrm[1] = 0x40+reg;
		modrm[2] = disp;
		return modrm;
	} else {
		modrm[0] = 1;
		modrm[1] = reg;
		return modrm;
	}
}

static inline modrm_p x86_mem2(modrm_o modrm, NativeReg reg, uint32 disp=0)
{		
	if (reg == REG_NO) {
		modrm[0] = 5;
		modrm[1] = 0x05;
		*((uint32 *)&modrm[2]) = disp;
		return modrm;
	} else return x86_mem2_r(modrm, reg, disp);
}

static inline modrm_p x86_mem2(modrm_o modrm, NativeReg reg, const void *disp)
{		
	return x86_mem2(modrm, reg, (uint32)disp);
}

static inline modrm_p x86_mem2(modrm_o modrm, const void *disp)
{		
	modrm[0] = 5;
	modrm[1] = 0x05;
	*((uint32 *)&modrm[2]) = (uint32)disp;
	return modrm;
}

/*
 *	reg1, reg2 must not be ESP
 */
static inline modrm_p x86_mem2_sib_r(modrm_o modrm, NativeReg reg1, int factor, NativeReg reg2, uint32 disp=0)
{
	switch (factor) {
		case 1:
		case 4:
		case 8: // ok
			break;
		case 2: if (reg1 == REG_NO) {
				// [eax+eax] is shorter than [eax*2+0]
				reg1 = reg2;
				factor = 1;
			}
			break;
		case 3:
		case 5:
		case 9: // [eax*(2^n+1)] -> [eax+eax*2^n]
			if (reg1 != REG_NO) { /* internal error */ }
			reg1 = reg2;
			factor--;
			break;
		default: 
			/* internal error */
			break;
	}
	//                              0     1     2  3     4  5  6  7     8
	static const byte factors[9] = {0, 0x00, 0x40, 0, 0x80, 0, 0, 0, 0xc0};
	if (reg1 == REG_NO) {
		// [eax*4+disp]
		modrm[0] = 6;
		modrm[1] = 0x04;
		modrm[2] = factors[factor]+(reg2<<3)+EBP;
		*((uint32 *)&modrm[3]) = disp;
		return modrm;
	} else if (((uint32)(disp) > 0x7f) && ((uint32)(disp) < 0xffffff80)) {
		modrm[0] = 6;
		modrm[1] = 0x84;
		modrm[2] = factors[factor]+(reg2<<3)+reg1;
		*((uint32 *)&modrm[3]) = disp;
		return modrm;
	} else if (disp || reg1 == EBP) {
		modrm[0] = 3;
		modrm[1] = 0x44;
		modrm[2] = factors[factor]+(reg2<<3)+reg1;
		modrm[3] = disp;
		return modrm;
	} else {
		modrm[0] = 2;
		modrm[1] = 0x04;
		modrm[2] = factors[factor]+(reg2<<3)+reg1;
		return modrm;
	}
}

/*
 *	reg1, reg2 must not be ESP
 */
static inline modrm_p x86_mem2(modrm_o modrm, NativeReg reg1, int factor, NativeReg reg2, uint32 disp=0)
{
	if (reg2 == REG_NO) return x86_mem2(modrm, reg1, disp);
	return x86_mem2_sib_r(modrm, reg1, factor, reg2, disp);
}

#endif
