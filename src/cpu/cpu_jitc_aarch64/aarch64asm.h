/*
 *  PearPC
 *  aarch64asm.h
 *
 *  Copyright (C) 2026 Sebastian Biallas (sb@biallas.net)
 *
 *  AArch64 instruction encoding for the JIT compiler.
 *  This is the architecture-specific code emitter.
 */

#ifndef __AARCH64ASM_H__
#define __AARCH64ASM_H__

#include "system/types.h"

/*
 *  AArch64 register definitions.
 *
 *  Register allocation strategy:
 *    X0-X7    : scratch / C ABI arguments (caller-saved)
 *    X8       : indirect result (caller-saved)
 *    X9-X15   : scratch (caller-saved) - PPC register cache
 *    X16-X17  : IP0/IP1 intra-procedure scratch (avoid in JIT)
 *    X18      : RESERVED on macOS (platform register)
 *    X19      : JITC pointer (callee-saved)
 *    X20      : CPU state pointer (callee-saved)
 *    X21-X28  : PPC register cache (callee-saved)
 *    X29 (FP) : frame pointer
 *    X30 (LR) : link register
 *    SP       : stack pointer (16-byte aligned)
 */

/* Encode a single AArch64 instruction (always 4 bytes) */
typedef uint32 A64Instr;

/*
 *  Emit helpers for the JITC.
 *  These encode AArch64 instructions into the translation cache.
 */

/* Data processing - immediate */
A64Instr a64_MOVZ(int rd, uint16 imm16, int shift); // MOV Xd, #imm16 << shift
A64Instr a64_MOVK(int rd, uint16 imm16, int shift); // MOVK Xd, #imm16 << shift
A64Instr a64_MOVN(int rd, uint16 imm16, int shift); // MOVN Xd, #imm16 << shift

A64Instr a64_ADD_imm(int rd, int rn, uint32 imm12);  // ADD Xd, Xn, #imm12
A64Instr a64_SUB_imm(int rd, int rn, uint32 imm12);  // SUB Xd, Xn, #imm12
A64Instr a64_ADDS_imm(int rd, int rn, uint32 imm12); // ADDS Xd, Xn, #imm12
A64Instr a64_SUBS_imm(int rd, int rn, uint32 imm12); // SUBS Xd, Xn, #imm12
A64Instr a64_CMP_imm(int rn, uint32 imm12);          // CMP Xn, #imm12

/* 32-bit variants (Wd registers) */
A64Instr a64_MOVZw(int rd, uint16 imm16, int shift);
A64Instr a64_MOVKw(int rd, uint16 imm16, int shift);
A64Instr a64_ADDw_imm(int rd, int rn, uint32 imm12);
A64Instr a64_SUBw_imm(int rd, int rn, uint32 imm12);
A64Instr a64_ADDSw_imm(int rd, int rn, uint32 imm12);
A64Instr a64_SUBSw_imm(int rd, int rn, uint32 imm12);
A64Instr a64_CMPw_imm(int rn, uint32 imm12);

/* Data processing - register */
A64Instr a64_ADD_reg(int rd, int rn, int rm); // ADD Xd, Xn, Xm
A64Instr a64_SUB_reg(int rd, int rn, int rm);
A64Instr a64_AND_reg(int rd, int rn, int rm);
A64Instr a64_ORR_reg(int rd, int rn, int rm);
A64Instr a64_EOR_reg(int rd, int rn, int rm);
A64Instr a64_ADDS_reg(int rd, int rn, int rm);
A64Instr a64_SUBS_reg(int rd, int rn, int rm);

/* 32-bit register ops */
A64Instr a64_ADDw_reg(int rd, int rn, int rm);
A64Instr a64_SUBw_reg(int rd, int rn, int rm);
A64Instr a64_ANDw_reg(int rd, int rn, int rm);
A64Instr a64_ORRw_reg(int rd, int rn, int rm);
A64Instr a64_EORw_reg(int rd, int rn, int rm);
A64Instr a64_ADDSw_reg(int rd, int rn, int rm);
A64Instr a64_SUBSw_reg(int rd, int rn, int rm);
A64Instr a64_CMPw_reg(int rn, int rm);

/* Shifted register ADD (32-bit) */
A64Instr a64_ADDw_reg_lsr(int rd, int rn, int rm, int shift); // ADD Wd, Wn, Wm, LSR #shift

/* CMN (immediate, 32-bit) */
A64Instr a64_CMNw_imm(int rn, uint32 imm12); // CMN Wn, #imm12 = ADDS WZR, Wn, #imm12

/* Sign extend (32-bit) */
A64Instr a64_SXTBw(int rd, int rn); // SXTB Wd, Wn = SBFM Wd, Wn, #0, #7
A64Instr a64_SXTHw(int rd, int rn); // SXTH Wd, Wn = SBFM Wd, Wn, #0, #15

/* 64-bit EOR with logical immediate */
A64Instr a64_EOR_imm(int rd, int rn, int N, int immr, int imms); // EOR Xd, Xn, #bitmask

/* MOV (register) */
A64Instr a64_MOV(int rd, int rn);  // MOV Xd, Xn
A64Instr a64_MOVw(int rd, int rn); // MOV Wd, Wn

/* Logical (register) with flag setting */
A64Instr a64_ANDSw_reg(int rd, int rn, int rm); // ANDS Wd, Wn, Wm (sets flags)
A64Instr a64_TSTw_reg(int rn, int rm);           // TST Wn, Wm = ANDS WZR, Wn, Wm

/* Logical (immediate) with flag setting */
A64Instr a64_ANDSw_imm(int rd, int rn, int immr, int imms); // ANDS Wd, Wn, #bitmask
A64Instr a64_TSTw_imm(int rn, int immr, int imms);          // TST Wn, #bitmask

/* Multiply */
A64Instr a64_MADDw(int rd, int rn, int rm, int ra); // MADD Wd, Wn, Wm, Wa
A64Instr a64_MULw(int rd, int rn, int rm);           // MUL Wd, Wn, Wm = MADD Wd, Wn, Wm, WZR

/* Negate */
A64Instr a64_NEGw(int rd, int rm); // NEG Wd, Wm = SUB Wd, WZR, Wm

/* Shifts */
A64Instr a64_LSLw_imm(int rd, int rn, int shift);
A64Instr a64_LSRw_imm(int rd, int rn, int shift);
A64Instr a64_LSR_imm(int rd, int rn, int shift);  // LSR Xd, Xn, #shift (64-bit)
A64Instr a64_ASRw_imm(int rd, int rn, int shift);
A64Instr a64_RORw_imm(int rd, int rn, int shift); // ROR Wd, Wn, #shift = EXTR Wd, Wn, Wn, #shift
A64Instr a64_LSLw_reg(int rd, int rn, int rm);     // LSL Wd, Wn, Wm = LSLV
A64Instr a64_LSRw_reg(int rd, int rn, int rm);     // LSR Wd, Wn, Wm = LSRV
A64Instr a64_RORw_reg(int rd, int rn, int rm);     // ROR Wd, Wn, Wm = RORV

/* Logical (immediate) */
A64Instr a64_ORRw_imm(int rd, int rn, int immr, int imms); // ORR Wd, Wn, #bitmask
A64Instr a64_ANDw_imm(int rd, int rn, int immr, int imms); // AND Wd, Wn, #bitmask
A64Instr a64_EORw_imm(int rd, int rn, int immr, int imms); // EOR Wd, Wn, #bitmask

/* Load/Store */
A64Instr a64_LDR(int rt, int rn, int offset);   // LDR Xt, [Xn, #offset]
A64Instr a64_STR(int rt, int rn, int offset);   // STR Xt, [Xn, #offset]
A64Instr a64_LDRw(int rt, int rn, int offset);  // LDR Wt, [Xn, #offset]
A64Instr a64_STRw(int rt, int rn, int offset);  // STR Wt, [Xn, #offset]
A64Instr a64_LDRBw(int rt, int rn, int offset); // LDRB Wt, [Xn, #offset]
A64Instr a64_STRBw(int rt, int rn, int offset); // STRB Wt, [Xn, #offset]
A64Instr a64_LDRHw(int rt, int rn, int offset); // LDRH Wt, [Xn, #offset]
A64Instr a64_STRHw(int rt, int rn, int offset); // STRH Wt, [Xn, #offset]

/* Load/Store register-indexed */
A64Instr a64_LDRw_reg(int rt, int rn, int rm, bool shift); // LDR Wt, [Xn, Xm{, LSL #2}]
A64Instr a64_LDR_reg(int rt, int rn, int rm, bool shift);  // LDR Xt, [Xn, Xm{, LSL #3}]

/* Load/Store pair (for push/pop) */
A64Instr a64_STP_pre(int rt1, int rt2, int rn, int offset);  // STP Xt1, Xt2, [Xn, #offset]!
A64Instr a64_LDP_post(int rt1, int rt2, int rn, int offset); // LDP Xt1, Xt2, [Xn], #offset

/* Branches */
A64Instr a64_B(sint32 offset);  // B #offset (PC-relative, +/-128MB)
A64Instr a64_BL(sint32 offset); // BL #offset
A64Instr a64_BR(int rn);        // BR Xn
A64Instr a64_BLR(int rn);       // BLR Xn
A64Instr a64_RET(int rn = 30);  // RET {Xn}

/* Conditional branches */
enum A64Cond {
    A64_EQ = 0x0, // equal
    A64_NE = 0x1, // not equal
    A64_CS = 0x2, // carry set / HS
    A64_CC = 0x3, // carry clear / LO
    A64_MI = 0x4, // minus/negative
    A64_PL = 0x5, // plus/positive
    A64_VS = 0x6, // overflow
    A64_VC = 0x7, // no overflow
    A64_HI = 0x8, // unsigned higher
    A64_LS = 0x9, // unsigned lower or same
    A64_GE = 0xa, // signed >=
    A64_LT = 0xb, // signed <
    A64_GT = 0xc, // signed >
    A64_LE = 0xd, // signed <=
    A64_AL = 0xe, // always
};

A64Instr a64_Bcc(A64Cond cond, sint32 offset);     // B.cond #offset (+/-1MB)
A64Instr a64_CBZ(int rt, sint32 offset);           // CBZ Xt, #offset
A64Instr a64_CBNZ(int rt, sint32 offset);          // CBNZ Xt, #offset
A64Instr a64_CBZw(int rt, sint32 offset);          // CBZ Wt, #offset
A64Instr a64_CBNZw(int rt, sint32 offset);         // CBNZ Wt, #offset
A64Instr a64_TBZ(int rt, int bit, sint32 offset);  // TBZ Xt, #bit, #offset
A64Instr a64_TBNZ(int rt, int bit, sint32 offset); // TBNZ Xt, #bit, #offset

/* Conditional select */
A64Instr a64_CSELw(int rd, int rn, int rm, A64Cond cond);   // CSEL Wd, Wn, Wm, cond
A64Instr a64_CSINCw(int rd, int rn, int rm, A64Cond cond);  // CSINC Wd, Wn, Wm, cond

/* Bit field operations */
A64Instr a64_BFIw(int rd, int rn, int lsb, int width); // BFI Wd, Wn, #lsb, #width

/* Data processing (2 source) */
A64Instr a64_UDIVw(int rd, int rn, int rm);  // UDIV Wd, Wn, Wm
A64Instr a64_SDIVw(int rd, int rn, int rm);  // SDIV Wd, Wn, Wm
A64Instr a64_LSLVw(int rd, int rn, int rm);  // LSLV Wd, Wn, Wm
A64Instr a64_LSRVw(int rd, int rn, int rm);  // LSRV Wd, Wn, Wm

/* 64-bit shift variable (for PPC shift-by-register with 6-bit amounts) */
A64Instr a64_LSLV(int rd, int rn, int rm);   // LSLV Xd, Xn, Xm
A64Instr a64_LSRV(int rd, int rn, int rm);   // LSRV Xd, Xn, Xm
A64Instr a64_ASRV(int rd, int rn, int rm);   // ASRV Xd, Xn, Xm
A64Instr a64_SXTW(int rd, int rn);           // SXTW Xd, Wn

/* Widening multiply */
A64Instr a64_UMULL(int rd, int rn, int rm);   // UMULL Xd, Wn, Wm
A64Instr a64_SMULL(int rd, int rn, int rm);   // SMULL Xd, Wn, Wm

/* Logical (register) with invert */
A64Instr a64_ORNw(int rd, int rn, int rm);    // ORN Wd, Wn, Wm = Wn | ~Wm
A64Instr a64_MVNw(int rd, int rm);            // MVN Wd, Wm = ~Wm

/* Count leading zeros */
A64Instr a64_CLZw(int rd, int rn);            // CLZ Wd, Wn

/* Misc */
A64Instr a64_NOP();
A64Instr a64_REV(int rd, int rn);  // byte swap 64-bit
A64Instr a64_REVw(int rd, int rn);   // byte swap 32-bit
A64Instr a64_REV16w(int rd, int rn); // byte swap within 16-bit halves

/* ADRP + ADD for PC-relative address loading */
A64Instr a64_ADRP(int rd, sint64 offset);
A64Instr a64_ADR(int rd, sint32 offset);

/*
 * Encode a 32-bit value as an AArch64 logical immediate (N=0, immr, imms).
 * Returns true if encodable, filling out_immr and out_imms.
 * AArch64 logical immediates represent bitmasks that are a contiguous run
 * of set bits, rotated, at element sizes of 2/4/8/16/32 bits.
 */
static inline bool a64_encode_log_imm32(uint32 val, int &out_immr, int &out_imms)
{
    if (val == 0 || val == 0xFFFFFFFF) return false; // not encodable

    // Try each element size: 2, 4, 8, 16, 32
    for (int size = 32; size >= 2; size >>= 1) {
        uint32 mask = (size == 32) ? 0xFFFFFFFF : (1u << size) - 1;
        uint32 elem = val & mask;

        // Check that val is a repeating pattern of this element
        bool repeats = true;
        for (int i = size; i < 32; i += size) {
            if (((val >> i) & mask) != elem) { repeats = false; break; }
        }
        if (!repeats) continue;

        // elem must be a rotated contiguous run of ones within 'size' bits
        // Rotate elem to find the run: count trailing zeros, rotate, count ones
        if (elem == 0 || elem == mask) continue; // all-0 or all-1 not valid at this size

        // Double the element to handle wrap-around
        uint64 doubled = ((uint64)elem << size) | elem;
        // Find lowest set bit position
        int lo = __builtin_ctz(elem);
        // Rotate right by lo to put the run at bit 0
        uint32 rotated = (uint32)(doubled >> lo) & mask;
        // Count consecutive ones from bit 0
        int ones = __builtin_ctz(~rotated);
        // Check it's a clean run
        if (rotated != ((1u << ones) - 1)) continue;

        // Encode: immr = (size - lo) % size, imms = (ones - 1) | size_encoding
        out_immr = (size - lo) % size;
        // imms encodes both the run length (ones-1) and element size:
        // size=32: imms = 0b0xxxxx (bit 5 = 0), run = imms & 0x1F + 1 → imms = ones-1
        // size=16: imms = 0b10xxxx, run bits in [3:0]
        // size=8:  imms = 0b110xxx, run bits in [2:0]
        // size=4:  imms = 0b1110xx, run bits in [1:0]
        // size=2:  imms = 0b11110x, run bits in [0:0]
        int size_enc;
        switch (size) {
        case 32: size_enc = 0b000000; break;
        case 16: size_enc = 0b100000; break;
        case  8: size_enc = 0b110000; break;
        case  4: size_enc = 0b111000; break;
        case  2: size_enc = 0b111100; break;
        default: continue;
        }
        // imms = size_enc | (ones - 1), but the size_enc sets the upper bits
        // to indicate element size, and the lower bits hold (ones - 1)
        // The encoding uses ~size as a prefix: for 32-bit, bit 5 must be 0.
        // For smaller sizes, higher bits are set to 1.
        out_imms = size_enc | (ones - 1);
        return true;
    }
    return false;
}

/* Instruction size computation for precomputed branch offsets.
 * Must match the instruction count in asmMOV / asmMOV64. */
static inline uint a64_movw_size(uint32 imm)
{
    uint16 lo = imm & 0xFFFF;
    uint16 hi = (imm >> 16) & 0xFFFF;
    if (lo == 0 && hi != 0) return 4; // single MOVZ shifted
    return hi ? 8 : 4;               // MOVZ + optional MOVK
}

static inline uint a64_mov64_size(uint64 imm)
{
    uint s = 4;
    if (imm >> 16) s += 4;
    if (imm >> 32) s += 4;
    if (imm >> 48) s += 4;
    return s;
}

static inline uint a64_bl_size(uint64 addr) { return a64_mov64_size(addr) + 4; }

#endif
