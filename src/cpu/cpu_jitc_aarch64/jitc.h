/*
 *  PearPC
 *  jitc.h
 *
 *  Copyright (C) 2004 Sebastian Biallas (sb@biallas.net)
 *  Copyright (C) 2026 AArch64 port
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __JITC_H__
#define __JITC_H__

#include "ppc_cpu.h"
#include "jitc_types.h"

/*
 * AArch64 general-purpose registers
 *
 * Register allocation convention for JITC:
 *   X0-X15  = scratch / allocatable for PPC register mapping
 *   X16-X17 = intra-procedure-call temporaries (not allocatable)
 *   X18     = platform register (reserved on macOS, not allocatable)
 *   X19     = JITC pointer (reserved)
 *   X20     = PPC_CPU_State pointer (reserved)
 *   X21-X28 = callee-saved, can be used for long-lived mappings
 *   X29     = frame pointer (not allocatable)
 *   X30     = link register (not allocatable)
 */
enum NativeReg {
    X0 = 0,
    X1 = 1,
    X2 = 2,
    X3 = 3,
    X4 = 4,
    X5 = 5,
    X6 = 6,
    X7 = 7,
    X8 = 8,
    X9 = 9,
    X10 = 10,
    X11 = 11,
    X12 = 12,
    X13 = 13,
    X14 = 14,
    X15 = 15,
    X16 = 16, // IP0 - don't allocate
    X17 = 17, // IP1 - don't allocate
    X18 = 18, // Platform register - don't allocate
    X19 = 19, // JITC pointer - reserved
    X20 = 20, // CPU state pointer - reserved
    X21 = 21,
    X22 = 22,
    X23 = 23,
    X24 = 24,
    X25 = 25,
    X26 = 26,
    X27 = 27,
    X28 = 28,
    X29 = 29, // FP - don't allocate
    X30 = 30, // LR - don't allocate
    REG_NO = 0xff,

    // W-register aliases (same encoding, for readability in 32-bit ops)
    W0 = 0, W1 = 1, W2 = 2, W3 = 3, W4 = 4, W5 = 5, W6 = 6, W7 = 7,
    W8 = 8, W9 = 9, W10 = 10, W11 = 11, W12 = 12, W13 = 13, W14 = 14, W15 = 15,
    W16 = 16, W17 = 17, W18 = 18, W19 = 19, W20 = 20,
    W21 = 21, W22 = 22, W23 = 23, W24 = 24, W25 = 25,
    W26 = 26, W27 = 27, W28 = 28, W29 = 29, W30 = 30,
    WZR = 31,
    XZR = 31,
};

enum NativeVectorReg {
    V0 = 0,
    V1 = 1,
    V2 = 2,
    V3 = 3,
    V4 = 4,
    V5 = 5,
    V6 = 6,
    V7 = 7,
    V8 = 8,
    V9 = 9,
    V10 = 10,
    V11 = 11,
    V12 = 12,
    V13 = 13,
    V14 = 14,
    V15 = 15,
    V16 = 16,
    V17 = 17,
    V18 = 18,
    V19 = 19,
    V20 = 20,
    V21 = 21,
    V22 = 22,
    V23 = 23,
    V24 = 24,
    V25 = 25,
    V26 = 26,
    V27 = 27,
    V28 = 28,
    V29 = 29,
    V30 = 30,
    V31 = 31,
    VECTREG_NO = 0xffffffff,
};

/*
 *  The size of a fragment
 *  This is the size of portion of translated client code
 */
#define FRAGMENT_SIZE 512

/*
 *  Used to describe a fragment of translated client code
 */
struct TranslationCacheFragment {
    NativeAddress base;
    TranslationCacheFragment *prev;
};

/*
 *  Used to describe a (not neccessarily translated) client page
 */
struct ClientPage {
    NativeAddress entrypoints[1024];
    TranslationCacheFragment *tcf_current;
    uint32 baseaddress;
    uint bytesLeft;
    NativeAddress tcp;
    ClientPage *moreRU;
    ClientPage *lessRU;
};

struct NativeRegType {
    NativeReg reg;
    NativeRegType *moreRU;
    NativeRegType *lessRU;
};

enum RegisterState {
    rsUnused = 0,
    rsMapped = 1,
    rsDirty = 2,
};

#define NATIVE_REG (2 << 8)
#define NATIVE_REG_PREFER (4 << 8)

#define NATIVE_REGS_ALL 0

/*
 * Number of allocatable registers.
 * X0-X15 = 16 registers for general allocation.
 * X21-X28 = 8 more callee-saved registers.
 * Total = 24, but we use 31 slots indexed by register number.
 */
#define JITC_NUM_NATIVE_REGS 31

typedef int JitcVectorReg;

#define JITC_VECTOR_REGS_ALL 0
#define JITC_VECTOR_TEMP 32
#define JITC_VECTOR_NEG1 33
#define PPC_VECTREG_NO 0xffffffff

struct JITC {
    /*
     *  This is the array of all (physical) pages of the client.
     */
    ClientPage **clientPages;

    /*
     *  If nativeReg[i] is set, it indicates to which client
     *  register this native register corresponds.
     */
    PPC_Register nativeReg[JITC_NUM_NATIVE_REGS];

    RegisterState nativeRegState[JITC_NUM_NATIVE_REGS];

    PPC_CRx nativeFlags;
    RegisterState nativeFlagsState;
    RegisterState nativeCarryState;

    /*
     *  If clientRegister is set, it indicates to which native
     *  register this client register corresponds.
     */
    NativeReg clientReg[sizeof(PPC_CPU_State)];

    bool checkedPriviledge;
    bool checkedFloat;
    bool checkedVector;

    NativeRegType *nativeRegsList[JITC_NUM_NATIVE_REGS];

    NativeRegType *LRUreg;
    NativeRegType *MRUreg;

    ClientPage *LRUpage;
    ClientPage *MRUpage;

    TranslationCacheFragment *freeFragmentsList;
    ClientPage *freeClientPages;

    byte *translationCache;

    /*
     *  Statistics
     */
    uint64 destroy_write;
    uint64 destroy_oopages;
    uint64 destroy_ootc;

    /*
     *  Alignment for emitted code (in bytes).
     *  AArch64 instructions are always 4-byte aligned.
     */
    uint loop_align;

    /*********************************************************************
     *  Only valid while compiling
     */
    ClientPage *currentPage;
    uint32 pc;
    uint32 current_opc;

    JitcVectorReg n2cVectorReg[17];
    NativeVectorReg c2nVectorReg[36];
    RegisterState nativeVectorRegState[9];
    NativeVectorReg LRUvregs[17];
    NativeVectorReg MRUvregs[17];
    int nativeVectorReg;

    bool init(uint maxClientPages, uint32 tcSize);
    void done();

    NativeReg allocRegister(int options = 0);
    NativeReg dirtyRegister(NativeReg reg);
    NativeReg mapClientRegisterDirty(PPC_Register creg, int options = 0);
    NativeReg getClientRegister(PPC_Register creg, int options = 0);
    NativeReg getClientRegisterDirty(PPC_Register reg, int options = 0);
    NativeReg getClientRegisterMapping(PPC_Register creg);

public:
    void flushAll();
    void clobberAll();
    void invalidateAll();
    void touchRegister(NativeReg reg);
    void flushRegister(int options = NATIVE_REGS_ALL);
    void flushRegisterDirty(int options = NATIVE_REGS_ALL);
    void clobberRegister(int options = NATIVE_REGS_ALL);
    void getClientCarry();
    void mapFlagsDirty(PPC_CRx cr = PPC_CR0);
    void mapCarryDirty();
    void clobberFlags();
    void clobberCarry();
    void clobberCarryAndFlags();
    void flushCarryAndFlagsDirty();

    PPC_CRx getFlagsMapping();

    bool flagsMapped();
    bool carryMapped();

private:
    void mapRegister(NativeReg nreg, PPC_Register creg);
    void unmapRegister(NativeReg reg);
    void loadRegister(NativeReg nreg, PPC_Register creg);
    void storeRegister(NativeReg nreg, PPC_Register creg);
    void storeRegisterUndirty(NativeReg nreg, PPC_Register creg);
    PPC_Register getRegisterMapping(NativeReg reg);
    void discardRegister(NativeReg r);
    void clobberAndTouchRegister(NativeReg reg);
    void clobberAndDiscardRegister(NativeReg reg);
    void clobberSingleRegister(NativeReg reg);
    NativeReg allocFixedRegister(NativeReg reg);
    void flushSingleRegister(NativeReg reg);
    void flushSingleRegisterDirty(NativeReg reg);
    void flushCarry();
    void flushFlags();

public:
    /*
     *  AArch64 instructions are always 4 bytes.
     *  emit32 emits a single instruction word.
     */
    void emit32(uint32 insn);
    void emit(byte *instr, uint size);
    bool emitAssure(uint size);

    NativeAddress asmHERE()
    {
        return currentPage->tcp;
    }

    /*
     *  AArch64 assembler methods.
     *  Named to match x86_64 JIT style: jitc.asmXXX(reg, ...)
     */
    void asmNOP();

    // MOV immediate (using MOVZ/MOVK sequence)
    void asmMOV(NativeReg rd, uint32 imm);     // MOV Wd, #imm32
    void asmMOV64(NativeReg rd, uint64 imm);    // MOV Xd, #imm64
    void asmMOV(NativeReg rd, NativeReg rs);    // MOV Xd, Xs (register)

    // Load/store from CPU state (X20-relative)
    void asmLDRw_cpu(NativeReg rd, uint32 offset);  // LDR Wd, [X20, #offset]
    void asmSTRw_cpu(NativeReg rs, uint32 offset);  // STR Ws, [X20, #offset]
    void asmLDR_cpu(NativeReg rd, uint32 offset);   // LDR Xd, [X20, #offset]
    void asmSTR_cpu(NativeReg rs, uint32 offset);   // STR Xs, [X20, #offset]

    // Branch helpers
    void asmB(NativeAddress to);
    void asmBL(NativeAddress to);       // BL via BLR X16 (far call)
    void asmBR(NativeReg rn);           // BR Xn
    void asmCALL(NativeAddress to);     // alias for asmBL (x86 compat name)
    void asmRET();

    // ALU 32-bit register-register
    void asmADDw(NativeReg rd, NativeReg rn, NativeReg rm);
    void asmSUBw(NativeReg rd, NativeReg rn, NativeReg rm);
    void asmANDw(NativeReg rd, NativeReg rn, NativeReg rm);
    void asmORRw(NativeReg rd, NativeReg rn, NativeReg rm);
    void asmEORw(NativeReg rd, NativeReg rn, NativeReg rm);
    void asmMULw(NativeReg rd, NativeReg rn, NativeReg rm);
    void asmNEGw(NativeReg rd, NativeReg rm);

    // ALU 32-bit immediate
    void asmADDw(NativeReg rd, NativeReg rn, uint32 imm12);
    void asmSUBw(NativeReg rd, NativeReg rn, uint32 imm12);

    // Compare
    void asmCMPw(NativeReg rn, NativeReg rm);
    void asmCMPw(NativeReg rn, uint32 imm12);
    void asmTSTw(NativeReg rn, int immr, int imms);  // TST Wn, #bitmask

    // Fixup support
    NativeAddress asmJxxFixup();  // emit placeholder B, return address for fixup
    void asmResolveFixup(NativeAddress at, NativeAddress to = 0);
    void asmResolveCondFixup(NativeAddress at, NativeAddress to, uint8 cond);

    // Compatibility aliases for old names (to be removed incrementally)
    void emitNOP()                                              { asmNOP(); }
    void emitMOV32(NativeReg rd, uint32 imm)                   { asmMOV(rd, imm); }
    void emitMOV64(NativeReg rd, uint64 imm)                   { asmMOV64(rd, imm); }
    void emitLDR32_cpu(NativeReg rd, uint32 offset)             { asmLDRw_cpu(rd, offset); }
    void emitSTR32_cpu(NativeReg rs, uint32 offset)             { asmSTRw_cpu(rs, offset); }
    void emitLDR64_cpu(NativeReg rd, uint32 offset)             { asmLDR_cpu(rd, offset); }
    void emitSTR64_cpu(NativeReg rs, uint32 offset)             { asmSTR_cpu(rs, offset); }
    void emitBL(NativeAddress to)                               { asmBL(to); }
    void emitB(NativeAddress to)                                { asmB(to); }
    void emitBR(NativeReg rn)                                   { asmBR(rn); }
    void emitBLR(NativeAddress to)                              { asmBL(to); }
    void emitRET()                                              { asmRET(); }
    void emitADD32(NativeReg rd, NativeReg rn, NativeReg rm)   { asmADDw(rd, rn, rm); }
    void emitSUB32(NativeReg rd, NativeReg rn, NativeReg rm)   { asmSUBw(rd, rn, rm); }
    void emitAND32(NativeReg rd, NativeReg rn, NativeReg rm)   { asmANDw(rd, rn, rm); }
    void emitORR32(NativeReg rd, NativeReg rn, NativeReg rm)   { asmORRw(rd, rn, rm); }
    void emitEOR32(NativeReg rd, NativeReg rn, NativeReg rm)   { asmEORw(rd, rn, rm); }
    void emitCMP32(NativeReg rn, NativeReg rm)                 { asmCMPw(rn, rm); }
    void emitCMP32_imm(NativeReg rn, uint32 imm12)             { asmCMPw(rn, imm12); }
    NativeAddress emitBxxFixup()                                { return asmJxxFixup(); }
    void resolveFixup(NativeAddress at, NativeAddress to = 0)   { asmResolveFixup(at, to); }
    void resolveCondFixup(NativeAddress at, NativeAddress to, uint8 cond) { asmResolveCondFixup(at, to, cond); }

    void floatRegisterClobberAll() {}
};

extern "C" void jitcDestroyAndFreeClientPage(JITC &aJITC, ClientPage *cp);
extern "C" NativeAddress jitcNewPC(JITC &aJITC, uint32 entry);

#endif
