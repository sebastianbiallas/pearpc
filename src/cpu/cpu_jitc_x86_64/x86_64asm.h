/*
 *	PearPC
 *	x86asm_64.h
 *
 *	Copyright (C) 2004-2006 Sebastian Biallas (sb@biallas.net)
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

#ifndef __X86ASM_64_H__
#define __X86ASM_64_H__

#include "system/types.h"
#include "ppc_cpu.h"
#include "jitc_types.h"

enum NativeReg {
	RAX = 0,
	RCX = 1,
	RDX = 2,
	RBX = 3,
	RSP = 4,	// don't mess with me, buddy
	RBP = 5,
	RSI = 6,
	RDI = 7,
	R8 = 8,
	R9 = 9,
	R10 = 10,
	R11 = 11,
	R12 = 12,
	R13 = 13,
	R14 = 14,
	R15 = 15,
	REG_NO = 0xffffffff,
};

#define NATIVE_REG	(2<<8)	 // used as a bitmask to specify register
#define NATIVE_REG_PREFER (4<<8) // used as a bitmask to specify register

#define NATIVE_REGS_ALL 0

struct X86CPUCaps {
	char vendor[13];
	bool _3dnow;
	bool _3dnow2;
	bool sse3;
	bool ssse3;
	bool sse4;
	uint loop_align;
};

void x86GetCaps(X86CPUCaps &caps);

enum X86ALUopc {
	X86_ADC  = 2,
	X86_ADD  = 0,
	X86_AND  = 4,
	X86_CMP  = 7,
	X86_LEA  = 11,
	X86_MOV  = 8,
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
	X86_CDQE = 0x9848,
	X86_CQO = 0x9948,
	X86_CMC = 0xf5,
	X86_LAHF = 0x9f,
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

NativeAddress asmHERE();

void asmNOP(int n);
void asmSimple(X86SimpleOpc simple);

void asmMOVABS(NativeReg reg, uint64 value);

void asmALU64(X86ALUopc opc, NativeReg reg1, NativeReg reg2);

void asmALU32(X86ALUopc opc, NativeReg reg1, NativeReg reg2);
void asmALU32(X86ALUopc opc, NativeReg reg, uint32 imm);
void asmALU32(X86ALUopc opc, NativeReg reg, void *mem);
void asmALU32(X86ALUopc opc, NativeReg reg, NativeReg base, uint32 disp);
void asmALU32(X86ALUopc opc, NativeReg reg, NativeReg base, int scale, NativeReg index, uint32 disp);
void asmALU32(X86ALUopc opc, NativeReg base, uint64 disp, NativeReg reg);
void asmALU32(X86ALUopc opc, NativeReg base, int scale, NativeReg index, uint64 disp, NativeReg reg);
void asmALU32(X86ALUopc opc, NativeReg base, uint64 disp, uint32 imm);
void asmMOV32(void *mem, NativeReg reg);
void asmMOV32(NativeReg reg, void *mem);
void asmMOV32(void *mem, uint32 imm);
void asmMOV32(NativeReg reg, uint32 imm);
void asmMOV32_NoFlags(NativeReg reg, uint32 imm);

void asmCMOV(X86FlagTest flags, NativeReg reg1, NativeReg reg2);
void asmCMOV8(X86FlagTest flags, NativeReg reg, NativeReg base, uint64 disp);

void asmSET8(X86FlagTest flags, NativeReg regb);
void asmSET8(X86FlagTest flags, void *mem);

void asmTEST(void *mem, uint32 imm);
void asmAND(void *mem, uint32 imm);
void asmOR(void *mem, uint32 imm);

void asmMOVxx32_8(X86MOVxx opc, NativeReg reg1, NativeReg reg2);
void asmMOVxx32_16(X86MOVxx opc, NativeReg reg1, NativeReg reg2);

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

void asmShift32(X86ShiftOpc opc, NativeReg reg, uint32 imm);
void asmShift32CL(X86ShiftOpc opc, NativeReg reg);
void asmINC32(NativeReg reg);
void asmDEC32(NativeReg reg);

void asmIMUL32(NativeReg reg1, NativeReg reg2, uint32 imm);
void asmIMUL32(NativeReg reg1, NativeReg reg2);

void asmBTx32(X86BitTest opc, NativeReg reg, int value);
void asmBTx32(X86BitTest opc, NativeReg reg1, NativeReg reg2, uint32 disp, int value);
void asmBSx32(X86BitSearch opc, NativeReg reg1, NativeReg reg2);

void asmBSWAP32(NativeReg reg);
void asmBSWAP64(NativeReg reg);

void asmJMP(NativeAddress to);
void asmJxx(X86FlagTest flags, NativeAddress to);
NativeAddress asmJMPFixup();
NativeAddress asmJxxFixup(X86FlagTest flags);
void asmCALL(NativeAddress to);
 
void asmResolveFixup(NativeAddress at, NativeAddress to=0);


enum NativeVectorReg {
	XMM0 = 0,
	XMM1 = 1,
	XMM2 = 2,
	XMM3 = 3,
	XMM4 = 4,
	XMM5 = 5,
	XMM6 = 6,
	XMM7 = 7,
	XMM8 = 8,
	XMM9 = 9,
	XMM10 = 10,
	XMM11 = 11,
	XMM12 = 12,
	XMM13 = 13,
	XMM14 = 14,
	XMM15 = 15,
	XMM_SENTINEL = 16,
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

NativeVectorReg jitcAllocVectorRegister(int hint=0);
void jitcDirtyVectorRegister(NativeVectorReg nreg);
void jitcTouchVectorRegister(NativeVectorReg nreg);

int jitcAssertFlushedVectorRegister(JitcVectorReg creg);
int jitcAssertFlushedVectorRegisters();
void jitcShowVectorRegisterStatus(JitcVectorReg creg);

NativeVectorReg jitcMapClientVectorRegisterDirty(JitcVectorReg creg, int hint=0);
NativeVectorReg jitcGetClientVectorRegister(JitcVectorReg creg, int hint=0);
NativeVectorReg jitcGetClientVectorRegisterDirty(JitcVectorReg creg, int hint=0);
NativeVectorReg jitcGetClientVectorRegisterMapping(JitcVectorReg creg);
NativeVectorReg jitcRenameVectorRegisterDirty(NativeVectorReg reg, JitcVectorReg creg, int hint=0);

void jitcFlushVectorRegister(int options=0);
void jitcFlushVectorRegisterDirty(int options=0);
void jitcClobberVectorRegister(int options=0);
void jitcTrashVectorRegister(int options=0);
void jitcDropVectorRegister(int options=0);

void jitcFlushClientVectorRegister(JitcVectorReg creg);
void jitcTrashClientVectorRegister(JitcVectorReg creg);
void jitcClobberClientVectorRegister(JitcVectorReg creg);
void jitcDropClientVectorRegister(JitcVectorReg creg);

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

#endif
