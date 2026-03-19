/*
 *  PearPC
 *  aarch64asm.cc
 *
 *  Copyright (C) 2026 Sebastian Biallas (sb@biallas.net)
 *
 *  AArch64 instruction encoding.
 */

#include <cstdio>
#include <cstdlib>
#include "aarch64asm.h"
#include "debug/tracers.h"

#define A64_ASSERT_RANGE(val, lo, hi, name) \
    do { if ((val) < (lo) || (val) > (hi)) { \
        PPC_CPU_ERR("[A64] %s: offset %d out of range [%d, %d]\n", name, (int)(val), (int)(lo), (int)(hi)); \
    } } while(0)

/*
 *  Move wide (immediate)
 *  MOVZ: Xd = imm16 << (hw*16), zero other bits
 *  MOVK: Xd[hw*16 +: 16] = imm16, keep other bits
 *  MOVN: Xd = ~(imm16 << (hw*16))
 */
A64Instr a64_MOVZ(int rd, uint16 imm16, int shift)
{
    int hw = shift / 16;
    return 0xD2800000 | (hw << 21) | ((uint32)imm16 << 5) | rd;
}

A64Instr a64_MOVK(int rd, uint16 imm16, int shift)
{
    int hw = shift / 16;
    return 0xF2800000 | (hw << 21) | ((uint32)imm16 << 5) | rd;
}

A64Instr a64_MOVN(int rd, uint16 imm16, int shift)
{
    int hw = shift / 16;
    return 0x92800000 | (hw << 21) | ((uint32)imm16 << 5) | rd;
}

/* 32-bit move wide */
A64Instr a64_MOVZw(int rd, uint16 imm16, int shift)
{
    int hw = shift / 16;
    return 0x52800000 | (hw << 21) | ((uint32)imm16 << 5) | rd;
}

A64Instr a64_MOVKw(int rd, uint16 imm16, int shift)
{
    int hw = shift / 16;
    return 0x72800000 | (hw << 21) | ((uint32)imm16 << 5) | rd;
}

/*
 *  Add/subtract (immediate)
 *  ADD Xd, Xn, #imm12
 */
A64Instr a64_ADD_imm(int rd, int rn, uint32 imm12)
{
    return 0x91000000 | ((imm12 & 0xFFF) << 10) | (rn << 5) | rd;
}

A64Instr a64_SUB_imm(int rd, int rn, uint32 imm12)
{
    return 0xD1000000 | ((imm12 & 0xFFF) << 10) | (rn << 5) | rd;
}

A64Instr a64_ADDS_imm(int rd, int rn, uint32 imm12)
{
    return 0xB1000000 | ((imm12 & 0xFFF) << 10) | (rn << 5) | rd;
}

A64Instr a64_SUBS_imm(int rd, int rn, uint32 imm12)
{
    return 0xF1000000 | ((imm12 & 0xFFF) << 10) | (rn << 5) | rd;
}

A64Instr a64_CMP_imm(int rn, uint32 imm12)
{
    return a64_SUBS_imm(31, rn, imm12); // XZR as destination
}

/* 32-bit add/subtract immediate */
A64Instr a64_ADDw_imm(int rd, int rn, uint32 imm12)
{
    return 0x11000000 | ((imm12 & 0xFFF) << 10) | (rn << 5) | rd;
}

A64Instr a64_SUBw_imm(int rd, int rn, uint32 imm12)
{
    return 0x51000000 | ((imm12 & 0xFFF) << 10) | (rn << 5) | rd;
}

A64Instr a64_ADDSw_imm(int rd, int rn, uint32 imm12)
{
    return 0x31000000 | ((imm12 & 0xFFF) << 10) | (rn << 5) | rd;
}

A64Instr a64_SUBSw_imm(int rd, int rn, uint32 imm12)
{
    return 0x71000000 | ((imm12 & 0xFFF) << 10) | (rn << 5) | rd;
}

A64Instr a64_CMPw_imm(int rn, uint32 imm12)
{
    return a64_SUBSw_imm(31, rn, imm12); // WZR
}

/*
 *  Data processing (register)
 */
A64Instr a64_ADD_reg(int rd, int rn, int rm)
{
    return 0x8B000000 | (rm << 16) | (rn << 5) | rd;
}

A64Instr a64_SUB_reg(int rd, int rn, int rm)
{
    return 0xCB000000 | (rm << 16) | (rn << 5) | rd;
}

A64Instr a64_AND_reg(int rd, int rn, int rm)
{
    return 0x8A000000 | (rm << 16) | (rn << 5) | rd;
}

A64Instr a64_ORR_reg(int rd, int rn, int rm)
{
    return 0xAA000000 | (rm << 16) | (rn << 5) | rd;
}

A64Instr a64_EOR_reg(int rd, int rn, int rm)
{
    return 0xCA000000 | (rm << 16) | (rn << 5) | rd;
}

A64Instr a64_ADDS_reg(int rd, int rn, int rm)
{
    return 0xAB000000 | (rm << 16) | (rn << 5) | rd;
}

A64Instr a64_SUBS_reg(int rd, int rn, int rm)
{
    return 0xEB000000 | (rm << 16) | (rn << 5) | rd;
}

/* 32-bit register ops */
A64Instr a64_ADDw_reg(int rd, int rn, int rm)
{
    return 0x0B000000 | (rm << 16) | (rn << 5) | rd;
}

A64Instr a64_SUBw_reg(int rd, int rn, int rm)
{
    return 0x4B000000 | (rm << 16) | (rn << 5) | rd;
}

A64Instr a64_ANDw_reg(int rd, int rn, int rm)
{
    return 0x0A000000 | (rm << 16) | (rn << 5) | rd;
}

A64Instr a64_ORRw_reg(int rd, int rn, int rm)
{
    return 0x2A000000 | (rm << 16) | (rn << 5) | rd;
}

A64Instr a64_EORw_reg(int rd, int rn, int rm)
{
    return 0x4A000000 | (rm << 16) | (rn << 5) | rd;
}

A64Instr a64_ADDSw_reg(int rd, int rn, int rm)
{
    return 0x2B000000 | (rm << 16) | (rn << 5) | rd;
}

A64Instr a64_SUBSw_reg(int rd, int rn, int rm)
{
    return 0x6B000000 | (rm << 16) | (rn << 5) | rd;
}

A64Instr a64_CMPw_reg(int rn, int rm)
{
    return a64_SUBSw_reg(31, rn, rm);
}

/* MOV (register) = ORR Xd, XZR, Xm */
A64Instr a64_MOV(int rd, int rn)
{
    return a64_ORR_reg(rd, 31, rn);
}

A64Instr a64_MOVw(int rd, int rn)
{
    return a64_ORRw_reg(rd, 31, rn);
}

/* Logical (register) with flag setting */
A64Instr a64_ANDSw_reg(int rd, int rn, int rm)
{
    return 0x6A000000 | (rm << 16) | (rn << 5) | rd;
}

A64Instr a64_TSTw_reg(int rn, int rm)
{
    return a64_ANDSw_reg(31, rn, rm); // WZR
}

/* Logical (immediate) with flag setting
 * N=0 for 32-bit, immr and imms encode a bitmask pattern.
 * E.g. for (1<<14) = 0x4000: immr=18, imms=0 */
A64Instr a64_ANDSw_imm(int rd, int rn, int immr, int imms)
{
    return 0x72000000 | (immr << 16) | (imms << 10) | (rn << 5) | rd;
}

A64Instr a64_TSTw_imm(int rn, int immr, int imms)
{
    return a64_ANDSw_imm(31, rn, immr, imms); // WZR
}

/* Multiply */
A64Instr a64_MADDw(int rd, int rn, int rm, int ra)
{
    return 0x1B000000 | (rm << 16) | (ra << 10) | (rn << 5) | rd;
}

A64Instr a64_MULw(int rd, int rn, int rm)
{
    return a64_MADDw(rd, rn, rm, 31); // WZR as accumulator
}

/* Negate */
A64Instr a64_NEGw(int rd, int rm)
{
    return a64_SUBw_reg(rd, 31, rm); // SUB Wd, WZR, Wm
}

/* Shifts (aliases for UBFM/SBFM) */
A64Instr a64_LSLw_imm(int rd, int rn, int shift)
{
    // LSL Wd, Wn, #shift = UBFM Wd, Wn, #(-shift MOD 32), #(31-shift)
    int immr = (-shift) & 31;
    int imms = 31 - shift;
    return 0x53000000 | (immr << 16) | (imms << 10) | (rn << 5) | rd;
}

A64Instr a64_LSRw_imm(int rd, int rn, int shift)
{
    // LSR Wd, Wn, #shift = UBFM Wd, Wn, #shift, #31
    return 0x53000000 | (shift << 16) | (31 << 10) | (rn << 5) | rd;
}

A64Instr a64_ASRw_imm(int rd, int rn, int shift)
{
    // ASR Wd, Wn, #shift = SBFM Wd, Wn, #shift, #31
    return 0x13000000 | (shift << 16) | (31 << 10) | (rn << 5) | rd;
}

/*
 *  Load/Store (unsigned offset)
 *  LDR Xt, [Xn, #offset]  -- offset must be 8-byte aligned, divided by 8
 *  LDR Wt, [Xn, #offset]  -- offset must be 4-byte aligned, divided by 4
 */
A64Instr a64_LDR(int rt, int rn, int offset)
{
    uint32 uoff = ((uint32)offset / 8) & 0xFFF;
    return 0xF9400000 | (uoff << 10) | (rn << 5) | rt;
}

A64Instr a64_STR(int rt, int rn, int offset)
{
    uint32 uoff = ((uint32)offset / 8) & 0xFFF;
    return 0xF9000000 | (uoff << 10) | (rn << 5) | rt;
}

A64Instr a64_LDRw(int rt, int rn, int offset)
{
    uint32 uoff = ((uint32)offset / 4) & 0xFFF;
    return 0xB9400000 | (uoff << 10) | (rn << 5) | rt;
}

A64Instr a64_STRw(int rt, int rn, int offset)
{
    uint32 uoff = ((uint32)offset / 4) & 0xFFF;
    return 0xB9000000 | (uoff << 10) | (rn << 5) | rt;
}

A64Instr a64_LDRBw(int rt, int rn, int offset)
{
    uint32 uoff = (uint32)offset & 0xFFF;
    return 0x39400000 | (uoff << 10) | (rn << 5) | rt;
}

A64Instr a64_STRBw(int rt, int rn, int offset)
{
    uint32 uoff = (uint32)offset & 0xFFF;
    return 0x39000000 | (uoff << 10) | (rn << 5) | rt;
}

A64Instr a64_LDRHw(int rt, int rn, int offset)
{
    uint32 uoff = ((uint32)offset / 2) & 0xFFF;
    return 0x79400000 | (uoff << 10) | (rn << 5) | rt;
}

A64Instr a64_STRHw(int rt, int rn, int offset)
{
    uint32 uoff = ((uint32)offset / 2) & 0xFFF;
    return 0x79000000 | (uoff << 10) | (rn << 5) | rt;
}

/* Load/Store pair (pre-index / post-index) */
A64Instr a64_STP_pre(int rt1, int rt2, int rn, int offset)
{
    sint32 simm7 = (offset / 8) & 0x7F;
    return 0xA9800000 | (simm7 << 15) | (rt2 << 10) | (rn << 5) | rt1;
}

A64Instr a64_LDP_post(int rt1, int rt2, int rn, int offset)
{
    sint32 simm7 = (offset / 8) & 0x7F;
    return 0xA8C00000 | (simm7 << 15) | (rt2 << 10) | (rn << 5) | rt1;
}

/*
 *  Branches
 *  B: PC-relative, +/-128MB (26-bit signed offset in instructions)
 *  BL: same but sets LR
 */
A64Instr a64_B(sint32 offset)
{
    sint32 imm26 = offset / 4;
    A64_ASSERT_RANGE(imm26, -0x2000000, 0x1FFFFFF, "B");
    return 0x14000000 | ((uint32)imm26 & 0x03FFFFFF);
}

A64Instr a64_BL(sint32 offset)
{
    sint32 imm26 = offset / 4;
    A64_ASSERT_RANGE(imm26, -0x2000000, 0x1FFFFFF, "BL");
    return 0x94000000 | ((uint32)imm26 & 0x03FFFFFF);
}

A64Instr a64_BR(int rn)
{
    return 0xD61F0000 | (rn << 5);
}

A64Instr a64_BLR(int rn)
{
    return 0xD63F0000 | (rn << 5);
}

A64Instr a64_RET(int rn)
{
    return 0xD65F0000 | (rn << 5);
}

/* Conditional branch */
A64Instr a64_Bcc(A64Cond cond, sint32 offset)
{
    sint32 imm19 = offset / 4;
    A64_ASSERT_RANGE(imm19, -0x40000, 0x3FFFF, "B.cc");
    return 0x54000000 | (((uint32)imm19 & 0x7FFFF) << 5) | (uint32)cond;
}

A64Instr a64_CBZ(int rt, sint32 offset)
{
    sint32 imm19 = offset / 4;
    A64_ASSERT_RANGE(imm19, -0x40000, 0x3FFFF, "CBZ");
    return 0xB4000000 | (((uint32)imm19 & 0x7FFFF) << 5) | rt;
}

A64Instr a64_CBNZ(int rt, sint32 offset)
{
    sint32 imm19 = offset / 4;
    A64_ASSERT_RANGE(imm19, -0x40000, 0x3FFFF, "CBNZ");
    return 0xB5000000 | (((uint32)imm19 & 0x7FFFF) << 5) | rt;
}

A64Instr a64_CBZw(int rt, sint32 offset)
{
    sint32 imm19 = offset / 4;
    A64_ASSERT_RANGE(imm19, -0x40000, 0x3FFFF, "CBZw");
    return 0x34000000 | (((uint32)imm19 & 0x7FFFF) << 5) | rt;
}

A64Instr a64_CBNZw(int rt, sint32 offset)
{
    sint32 imm19 = offset / 4;
    A64_ASSERT_RANGE(imm19, -0x40000, 0x3FFFF, "CBNZw");
    return 0x35000000 | (((uint32)imm19 & 0x7FFFF) << 5) | rt;
}

A64Instr a64_TBZ(int rt, int bit, sint32 offset)
{
    sint32 imm14 = offset / 4;
    A64_ASSERT_RANGE(imm14, -0x2000, 0x1FFF, "TBZ");
    uint32 b5 = (bit >> 5) & 1;
    uint32 b40 = bit & 0x1F;
    return 0x36000000 | (b5 << 31) | (b40 << 19) | (((uint32)imm14 & 0x3FFF) << 5) | rt;
}

A64Instr a64_TBNZ(int rt, int bit, sint32 offset)
{
    sint32 imm14 = offset / 4;
    A64_ASSERT_RANGE(imm14, -0x2000, 0x1FFF, "TBNZ");
    uint32 b5 = (bit >> 5) & 1;
    uint32 b40 = bit & 0x1F;
    return 0x37000000 | (b5 << 31) | (b40 << 19) | (((uint32)imm14 & 0x3FFF) << 5) | rt;
}

/* Misc */
A64Instr a64_NOP()
{
    return 0xD503201F;
}

A64Instr a64_REV(int rd, int rn)
{
    // REV Xd, Xn (64-bit byte reverse)
    return 0xDAC00C00 | (rn << 5) | rd;
}

A64Instr a64_REVw(int rd, int rn)
{
    // REV Wd, Wn (32-bit byte reverse)
    return 0x5AC00800 | (rn << 5) | rd;
}

/* PC-relative addressing */
A64Instr a64_ADRP(int rd, sint64 offset)
{
    // ADRP Xd, #offset (page-aligned, +/-4GB)
    sint32 imm = (sint32)(offset >> 12);
    uint32 immlo = imm & 0x3;
    uint32 immhi = (imm >> 2) & 0x7FFFF;
    return 0x90000000 | (immlo << 29) | (immhi << 5) | rd;
}

A64Instr a64_ADR(int rd, sint32 offset)
{
    // ADR Xd, #offset (PC-relative, +/-1MB)
    uint32 immlo = offset & 0x3;
    uint32 immhi = (offset >> 2) & 0x7FFFF;
    return 0x10000000 | (immlo << 29) | (immhi << 5) | rd;
}
