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

/* Misc */
A64Instr a64_NOP();
A64Instr a64_REV(int rd, int rn);  // byte swap 64-bit
A64Instr a64_REVw(int rd, int rn); // byte swap 32-bit

/* ADRP + ADD for PC-relative address loading */
A64Instr a64_ADRP(int rd, sint64 offset);
A64Instr a64_ADR(int rd, sint32 offset);

/* Instruction size computation for precomputed branch offsets */
static inline uint a64_movw_size(uint32 imm) { return (imm >> 16) ? 8 : 4; }

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
