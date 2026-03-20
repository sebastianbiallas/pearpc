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

#define A64_ASSERT_REG(r, name) \
    do { if ((unsigned)(r) > 31) { \
        PPC_CPU_ERR("[A64] %s: register %d out of range [0, 31]\n", name, (int)(r)); \
    } } while(0)

#define A64_ASSERT_ALIGN(val, align, name) \
    do { if ((val) % (align) != 0) { \
        PPC_CPU_ERR("[A64] %s: offset %d not aligned to %d\n", name, (int)(val), (int)(align)); \
    } } while(0)

/*
 *  Move wide (immediate)
 *  MOVZ: Xd = imm16 << (hw*16), zero other bits
 *  MOVK: Xd[hw*16 +: 16] = imm16, keep other bits
 *  MOVN: Xd = ~(imm16 << (hw*16))
 */
A64Instr a64_MOVZ(int rd, uint16 imm16, int shift)
{
    A64_ASSERT_REG(rd, "MOVZ");
    A64_ASSERT_RANGE(shift, 0, 48, "MOVZ shift");
    A64_ASSERT_ALIGN(shift, 16, "MOVZ shift");
    int hw = shift / 16;
    return 0xD2800000 | (hw << 21) | ((uint32)imm16 << 5) | rd;
}

A64Instr a64_MOVK(int rd, uint16 imm16, int shift)
{
    A64_ASSERT_REG(rd, "MOVK");
    A64_ASSERT_RANGE(shift, 0, 48, "MOVK shift");
    A64_ASSERT_ALIGN(shift, 16, "MOVK shift");
    int hw = shift / 16;
    return 0xF2800000 | (hw << 21) | ((uint32)imm16 << 5) | rd;
}

A64Instr a64_MOVN(int rd, uint16 imm16, int shift)
{
    A64_ASSERT_REG(rd, "MOVN");
    A64_ASSERT_RANGE(shift, 0, 48, "MOVN shift");
    A64_ASSERT_ALIGN(shift, 16, "MOVN shift");
    int hw = shift / 16;
    return 0x92800000 | (hw << 21) | ((uint32)imm16 << 5) | rd;
}

/* 32-bit move wide */
A64Instr a64_MOVZw(int rd, uint16 imm16, int shift)
{
    A64_ASSERT_REG(rd, "MOVZw");
    A64_ASSERT_RANGE(shift, 0, 16, "MOVZw shift");
    A64_ASSERT_ALIGN(shift, 16, "MOVZw shift");
    int hw = shift / 16;
    return 0x52800000 | (hw << 21) | ((uint32)imm16 << 5) | rd;
}

A64Instr a64_MOVKw(int rd, uint16 imm16, int shift)
{
    A64_ASSERT_REG(rd, "MOVKw");
    A64_ASSERT_RANGE(shift, 0, 16, "MOVKw shift");
    A64_ASSERT_ALIGN(shift, 16, "MOVKw shift");
    int hw = shift / 16;
    return 0x72800000 | (hw << 21) | ((uint32)imm16 << 5) | rd;
}

A64Instr a64_MOVNw(int rd, uint16 imm16, int shift)
{
    A64_ASSERT_REG(rd, "MOVNw");
    A64_ASSERT_RANGE(shift, 0, 16, "MOVNw shift");
    A64_ASSERT_ALIGN(shift, 16, "MOVNw shift");
    int hw = shift / 16;
    return 0x12800000 | (hw << 21) | ((uint32)imm16 << 5) | rd;
}

/*
 *  Add/subtract (immediate)
 *  ADD Xd, Xn, #imm12
 */
A64Instr a64_ADD_imm(int rd, int rn, uint32 imm12)
{
    A64_ASSERT_REG(rd, "ADD_imm");
    A64_ASSERT_REG(rn, "ADD_imm");
    A64_ASSERT_RANGE(imm12, 0, 0xFFF, "ADD_imm imm12");
    return 0x91000000 | (imm12 << 10) | (rn << 5) | rd;
}

A64Instr a64_SUB_imm(int rd, int rn, uint32 imm12)
{
    A64_ASSERT_REG(rd, "SUB_imm");
    A64_ASSERT_REG(rn, "SUB_imm");
    A64_ASSERT_RANGE(imm12, 0, 0xFFF, "SUB_imm imm12");
    return 0xD1000000 | (imm12 << 10) | (rn << 5) | rd;
}

A64Instr a64_ADDS_imm(int rd, int rn, uint32 imm12)
{
    A64_ASSERT_REG(rd, "ADDS_imm");
    A64_ASSERT_REG(rn, "ADDS_imm");
    A64_ASSERT_RANGE(imm12, 0, 0xFFF, "ADDS_imm imm12");
    return 0xB1000000 | (imm12 << 10) | (rn << 5) | rd;
}

A64Instr a64_SUBS_imm(int rd, int rn, uint32 imm12)
{
    A64_ASSERT_REG(rd, "SUBS_imm");
    A64_ASSERT_REG(rn, "SUBS_imm");
    A64_ASSERT_RANGE(imm12, 0, 0xFFF, "SUBS_imm imm12");
    return 0xF1000000 | (imm12 << 10) | (rn << 5) | rd;
}

A64Instr a64_CMP_imm(int rn, uint32 imm12)
{
    return a64_SUBS_imm(31, rn, imm12); // XZR as destination
}

/* 32-bit add/subtract immediate */
A64Instr a64_ADDw_imm(int rd, int rn, uint32 imm12)
{
    A64_ASSERT_REG(rd, "ADDw_imm");
    A64_ASSERT_REG(rn, "ADDw_imm");
    A64_ASSERT_RANGE(imm12, 0, 0xFFF, "ADDw_imm imm12");
    return 0x11000000 | (imm12 << 10) | (rn << 5) | rd;
}

A64Instr a64_SUBw_imm(int rd, int rn, uint32 imm12)
{
    A64_ASSERT_REG(rd, "SUBw_imm");
    A64_ASSERT_REG(rn, "SUBw_imm");
    A64_ASSERT_RANGE(imm12, 0, 0xFFF, "SUBw_imm imm12");
    return 0x51000000 | (imm12 << 10) | (rn << 5) | rd;
}

A64Instr a64_ADDSw_imm(int rd, int rn, uint32 imm12)
{
    A64_ASSERT_REG(rd, "ADDSw_imm");
    A64_ASSERT_REG(rn, "ADDSw_imm");
    A64_ASSERT_RANGE(imm12, 0, 0xFFF, "ADDSw_imm imm12");
    return 0x31000000 | (imm12 << 10) | (rn << 5) | rd;
}

A64Instr a64_SUBSw_imm(int rd, int rn, uint32 imm12)
{
    A64_ASSERT_REG(rd, "SUBSw_imm");
    A64_ASSERT_REG(rn, "SUBSw_imm");
    A64_ASSERT_RANGE(imm12, 0, 0xFFF, "SUBSw_imm imm12");
    return 0x71000000 | (imm12 << 10) | (rn << 5) | rd;
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
    A64_ASSERT_REG(rd, "ADD_reg"); A64_ASSERT_REG(rn, "ADD_reg"); A64_ASSERT_REG(rm, "ADD_reg");
    return 0x8B000000 | (rm << 16) | (rn << 5) | rd;
}

A64Instr a64_SUB_reg(int rd, int rn, int rm)
{
    A64_ASSERT_REG(rd, "SUB_reg"); A64_ASSERT_REG(rn, "SUB_reg"); A64_ASSERT_REG(rm, "SUB_reg");
    return 0xCB000000 | (rm << 16) | (rn << 5) | rd;
}

A64Instr a64_AND_reg(int rd, int rn, int rm)
{
    A64_ASSERT_REG(rd, "AND_reg"); A64_ASSERT_REG(rn, "AND_reg"); A64_ASSERT_REG(rm, "AND_reg");
    return 0x8A000000 | (rm << 16) | (rn << 5) | rd;
}

A64Instr a64_ORR_reg(int rd, int rn, int rm)
{
    A64_ASSERT_REG(rd, "ORR_reg"); A64_ASSERT_REG(rn, "ORR_reg"); A64_ASSERT_REG(rm, "ORR_reg");
    return 0xAA000000 | (rm << 16) | (rn << 5) | rd;
}

A64Instr a64_EOR_reg(int rd, int rn, int rm)
{
    A64_ASSERT_REG(rd, "EOR_reg"); A64_ASSERT_REG(rn, "EOR_reg"); A64_ASSERT_REG(rm, "EOR_reg");
    return 0xCA000000 | (rm << 16) | (rn << 5) | rd;
}

A64Instr a64_ADDS_reg(int rd, int rn, int rm)
{
    A64_ASSERT_REG(rd, "ADDS_reg"); A64_ASSERT_REG(rn, "ADDS_reg"); A64_ASSERT_REG(rm, "ADDS_reg");
    return 0xAB000000 | (rm << 16) | (rn << 5) | rd;
}

A64Instr a64_SUBS_reg(int rd, int rn, int rm)
{
    A64_ASSERT_REG(rd, "SUBS_reg"); A64_ASSERT_REG(rn, "SUBS_reg"); A64_ASSERT_REG(rm, "SUBS_reg");
    return 0xEB000000 | (rm << 16) | (rn << 5) | rd;
}

/* 32-bit register ops */
A64Instr a64_ADDw_reg(int rd, int rn, int rm)
{
    A64_ASSERT_REG(rd, "ADDw_reg"); A64_ASSERT_REG(rn, "ADDw_reg"); A64_ASSERT_REG(rm, "ADDw_reg");
    return 0x0B000000 | (rm << 16) | (rn << 5) | rd;
}

A64Instr a64_ADDw_reg_lsr(int rd, int rn, int rm, int shift)
{
    A64_ASSERT_REG(rd, "ADDw_reg_lsr"); A64_ASSERT_REG(rn, "ADDw_reg_lsr"); A64_ASSERT_REG(rm, "ADDw_reg_lsr");
    A64_ASSERT_RANGE(shift, 0, 31, "ADDw_reg_lsr shift");
    return 0x0B400000 | (rm << 16) | (shift << 10) | (rn << 5) | rd;
}

A64Instr a64_CMNw_imm(int rn, uint32 imm12)
{
    // CMN Wn, #imm12 = ADDS WZR, Wn, #imm12
    A64_ASSERT_REG(rn, "CMNw_imm");
    A64_ASSERT_RANGE(imm12, 0, 0xFFF, "CMNw_imm imm12");
    return 0x31000000 | (imm12 << 10) | (rn << 5) | 0x1F;
}

A64Instr a64_SXTW(int rd, int rn)
{
    A64_ASSERT_REG(rd, "SXTW"); A64_ASSERT_REG(rn, "SXTW");
    // SXTW Xd, Wn = SBFM Xd, Xn, #0, #31
    return 0x93407C00 | (rn << 5) | rd;
}

A64Instr a64_SXTBw(int rd, int rn)
{
    A64_ASSERT_REG(rd, "SXTBw"); A64_ASSERT_REG(rn, "SXTBw");
    return 0x13001C00 | (rn << 5) | rd;
}

A64Instr a64_SXTHw(int rd, int rn)
{
    A64_ASSERT_REG(rd, "SXTHw"); A64_ASSERT_REG(rn, "SXTHw");
    return 0x13003C00 | (rn << 5) | rd;
}

A64Instr a64_AND_imm(int rd, int rn, int N, int immr, int imms)
{
    A64_ASSERT_REG(rd, "AND_imm"); A64_ASSERT_REG(rn, "AND_imm");
    A64_ASSERT_RANGE(N, 0, 1, "AND_imm N");
    A64_ASSERT_RANGE(immr, 0, 63, "AND_imm immr");
    A64_ASSERT_RANGE(imms, 0, 63, "AND_imm imms");
    return 0x92000000 | (N << 22) | (immr << 16) | (imms << 10) | (rn << 5) | rd;
}

A64Instr a64_ORR_imm(int rd, int rn, int N, int immr, int imms)
{
    A64_ASSERT_REG(rd, "ORR_imm"); A64_ASSERT_REG(rn, "ORR_imm");
    A64_ASSERT_RANGE(N, 0, 1, "ORR_imm N");
    A64_ASSERT_RANGE(immr, 0, 63, "ORR_imm immr");
    A64_ASSERT_RANGE(imms, 0, 63, "ORR_imm imms");
    return 0xB2000000 | (N << 22) | (immr << 16) | (imms << 10) | (rn << 5) | rd;
}

A64Instr a64_EOR_imm(int rd, int rn, int N, int immr, int imms)
{
    A64_ASSERT_REG(rd, "EOR_imm"); A64_ASSERT_REG(rn, "EOR_imm");
    A64_ASSERT_RANGE(N, 0, 1, "EOR_imm N");
    A64_ASSERT_RANGE(immr, 0, 63, "EOR_imm immr");
    A64_ASSERT_RANGE(imms, 0, 63, "EOR_imm imms");
    return 0xD2000000 | (N << 22) | (immr << 16) | (imms << 10) | (rn << 5) | rd;
}

A64Instr a64_SUBw_reg(int rd, int rn, int rm)
{
    A64_ASSERT_REG(rd, "SUBw_reg"); A64_ASSERT_REG(rn, "SUBw_reg"); A64_ASSERT_REG(rm, "SUBw_reg");
    return 0x4B000000 | (rm << 16) | (rn << 5) | rd;
}

A64Instr a64_ANDw_reg(int rd, int rn, int rm)
{
    A64_ASSERT_REG(rd, "ANDw_reg"); A64_ASSERT_REG(rn, "ANDw_reg"); A64_ASSERT_REG(rm, "ANDw_reg");
    return 0x0A000000 | (rm << 16) | (rn << 5) | rd;
}

A64Instr a64_ORRw_reg(int rd, int rn, int rm)
{
    A64_ASSERT_REG(rd, "ORRw_reg"); A64_ASSERT_REG(rn, "ORRw_reg"); A64_ASSERT_REG(rm, "ORRw_reg");
    return 0x2A000000 | (rm << 16) | (rn << 5) | rd;
}

A64Instr a64_EORw_reg(int rd, int rn, int rm)
{
    A64_ASSERT_REG(rd, "EORw_reg"); A64_ASSERT_REG(rn, "EORw_reg"); A64_ASSERT_REG(rm, "EORw_reg");
    return 0x4A000000 | (rm << 16) | (rn << 5) | rd;
}

A64Instr a64_ADDSw_reg(int rd, int rn, int rm)
{
    A64_ASSERT_REG(rd, "ADDSw_reg"); A64_ASSERT_REG(rn, "ADDSw_reg"); A64_ASSERT_REG(rm, "ADDSw_reg");
    return 0x2B000000 | (rm << 16) | (rn << 5) | rd;
}

A64Instr a64_SUBSw_reg(int rd, int rn, int rm)
{
    A64_ASSERT_REG(rd, "SUBSw_reg"); A64_ASSERT_REG(rn, "SUBSw_reg"); A64_ASSERT_REG(rm, "SUBSw_reg");
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
    A64_ASSERT_REG(rd, "ANDSw_reg"); A64_ASSERT_REG(rn, "ANDSw_reg"); A64_ASSERT_REG(rm, "ANDSw_reg");
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
    A64_ASSERT_REG(rd, "ANDSw_imm");
    A64_ASSERT_REG(rn, "ANDSw_imm");
    A64_ASSERT_RANGE(immr, 0, 31, "ANDSw_imm immr");
    A64_ASSERT_RANGE(imms, 0, 31, "ANDSw_imm imms");
    return 0x72000000 | (immr << 16) | (imms << 10) | (rn << 5) | rd;
}

A64Instr a64_TSTw_imm(int rn, int immr, int imms)
{
    return a64_ANDSw_imm(31, rn, immr, imms); // WZR
}

/* Multiply */
A64Instr a64_MADDw(int rd, int rn, int rm, int ra)
{
    A64_ASSERT_REG(rd, "MADDw"); A64_ASSERT_REG(rn, "MADDw"); A64_ASSERT_REG(rm, "MADDw"); A64_ASSERT_REG(ra, "MADDw");
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
    A64_ASSERT_REG(rd, "LSLw_imm");
    A64_ASSERT_REG(rn, "LSLw_imm");
    A64_ASSERT_RANGE(shift, 0, 31, "LSLw_imm shift");
    // LSL Wd, Wn, #shift = UBFM Wd, Wn, #(-shift MOD 32), #(31-shift)
    int immr = (-shift) & 31;
    int imms = 31 - shift;
    return 0x53000000 | (immr << 16) | (imms << 10) | (rn << 5) | rd;
}

A64Instr a64_LSRw_imm(int rd, int rn, int shift)
{
    A64_ASSERT_REG(rd, "LSRw_imm");
    A64_ASSERT_REG(rn, "LSRw_imm");
    A64_ASSERT_RANGE(shift, 0, 31, "LSRw_imm shift");
    // LSR Wd, Wn, #shift = UBFM Wd, Wn, #shift, #31
    return 0x53000000 | (shift << 16) | (31 << 10) | (rn << 5) | rd;
}

A64Instr a64_LSR_imm(int rd, int rn, int shift)
{
    A64_ASSERT_REG(rd, "LSR_imm");
    A64_ASSERT_REG(rn, "LSR_imm");
    A64_ASSERT_RANGE(shift, 0, 63, "LSR_imm shift");
    // LSR Xd, Xn, #shift = UBFM Xd, Xn, #shift, #63
    return 0xD340FC00 | (shift << 16) | (rn << 5) | rd;
}

A64Instr a64_ASRw_imm(int rd, int rn, int shift)
{
    A64_ASSERT_REG(rd, "ASRw_imm");
    A64_ASSERT_REG(rn, "ASRw_imm");
    A64_ASSERT_RANGE(shift, 0, 31, "ASRw_imm shift");
    // ASR Wd, Wn, #shift = SBFM Wd, Wn, #shift, #31
    return 0x13000000 | (shift << 16) | (31 << 10) | (rn << 5) | rd;
}

A64Instr a64_RORw_imm(int rd, int rn, int shift)
{
    A64_ASSERT_REG(rd, "RORw_imm");
    A64_ASSERT_REG(rn, "RORw_imm");
    A64_ASSERT_RANGE(shift, 0, 31, "RORw_imm shift");
    // ROR Wd, Wn, #shift = EXTR Wd, Wn, Wn, #shift
    return 0x13800000 | (rn << 16) | (shift << 10) | (rn << 5) | rd;
}

A64Instr a64_LSLw_reg(int rd, int rn, int rm)
{
    A64_ASSERT_REG(rd, "LSLw_reg");
    A64_ASSERT_REG(rn, "LSLw_reg");
    A64_ASSERT_REG(rm, "LSLw_reg");
    // LSLV Wd, Wn, Wm
    return 0x1AC02000 | (rm << 16) | (rn << 5) | rd;
}

A64Instr a64_LSRw_reg(int rd, int rn, int rm)
{
    A64_ASSERT_REG(rd, "LSRw_reg");
    A64_ASSERT_REG(rn, "LSRw_reg");
    A64_ASSERT_REG(rm, "LSRw_reg");
    // LSRV Wd, Wn, Wm
    return 0x1AC02400 | (rm << 16) | (rn << 5) | rd;
}

A64Instr a64_RORw_reg(int rd, int rn, int rm)
{
    A64_ASSERT_REG(rd, "RORw_reg");
    A64_ASSERT_REG(rn, "RORw_reg");
    A64_ASSERT_REG(rm, "RORw_reg");
    // RORV Wd, Wn, Wm
    return 0x1AC02C00 | (rm << 16) | (rn << 5) | rd;
}

/* Logical (immediate) — uses bitmask immediate encoding */
A64Instr a64_ORRw_imm(int rd, int rn, int immr, int imms)
{
    A64_ASSERT_REG(rd, "ORRw_imm");
    A64_ASSERT_REG(rn, "ORRw_imm");
    // ORR Wd, Wn, #bitmask  (N=0 for 32-bit)
    return 0x32000000 | (immr << 16) | (imms << 10) | (rn << 5) | rd;
}

A64Instr a64_ANDw_imm(int rd, int rn, int immr, int imms)
{
    A64_ASSERT_REG(rd, "ANDw_imm");
    A64_ASSERT_REG(rn, "ANDw_imm");
    // AND Wd, Wn, #bitmask  (N=0 for 32-bit)
    return 0x12000000 | (immr << 16) | (imms << 10) | (rn << 5) | rd;
}

A64Instr a64_EORw_imm(int rd, int rn, int immr, int imms)
{
    A64_ASSERT_REG(rd, "EORw_imm");
    A64_ASSERT_REG(rn, "EORw_imm");
    // EOR Wd, Wn, #bitmask  (N=0 for 32-bit)
    return 0x52000000 | (immr << 16) | (imms << 10) | (rn << 5) | rd;
}

/*
 *  Load/Store (unsigned offset)
 *  LDR Xt, [Xn, #offset]  -- offset must be 8-byte aligned, divided by 8
 *  LDR Wt, [Xn, #offset]  -- offset must be 4-byte aligned, divided by 4
 */
A64Instr a64_LDR(int rt, int rn, int offset)
{
    A64_ASSERT_REG(rt, "LDR");
    A64_ASSERT_REG(rn, "LDR");
    A64_ASSERT_ALIGN(offset, 8, "LDR");
    A64_ASSERT_RANGE(offset, 0, 32760, "LDR offset");
    uint32 uoff = (uint32)offset / 8;
    return 0xF9400000 | (uoff << 10) | (rn << 5) | rt;
}

A64Instr a64_STR(int rt, int rn, int offset)
{
    A64_ASSERT_REG(rt, "STR");
    A64_ASSERT_REG(rn, "STR");
    A64_ASSERT_ALIGN(offset, 8, "STR");
    A64_ASSERT_RANGE(offset, 0, 32760, "STR offset");
    uint32 uoff = (uint32)offset / 8;
    return 0xF9000000 | (uoff << 10) | (rn << 5) | rt;
}

A64Instr a64_LDRw(int rt, int rn, int offset)
{
    A64_ASSERT_REG(rt, "LDRw");
    A64_ASSERT_REG(rn, "LDRw");
    A64_ASSERT_ALIGN(offset, 4, "LDRw");
    A64_ASSERT_RANGE(offset, 0, 16380, "LDRw offset");
    uint32 uoff = (uint32)offset / 4;
    return 0xB9400000 | (uoff << 10) | (rn << 5) | rt;
}

A64Instr a64_STRw(int rt, int rn, int offset)
{
    A64_ASSERT_REG(rt, "STRw");
    A64_ASSERT_REG(rn, "STRw");
    A64_ASSERT_ALIGN(offset, 4, "STRw");
    A64_ASSERT_RANGE(offset, 0, 16380, "STRw offset");
    uint32 uoff = (uint32)offset / 4;
    return 0xB9000000 | (uoff << 10) | (rn << 5) | rt;
}

A64Instr a64_LDRBw(int rt, int rn, int offset)
{
    A64_ASSERT_REG(rt, "LDRBw");
    A64_ASSERT_REG(rn, "LDRBw");
    A64_ASSERT_RANGE(offset, 0, 4095, "LDRBw offset");
    uint32 uoff = (uint32)offset;
    return 0x39400000 | (uoff << 10) | (rn << 5) | rt;
}

A64Instr a64_STRBw(int rt, int rn, int offset)
{
    A64_ASSERT_REG(rt, "STRBw");
    A64_ASSERT_REG(rn, "STRBw");
    A64_ASSERT_RANGE(offset, 0, 4095, "STRBw offset");
    uint32 uoff = (uint32)offset;
    return 0x39000000 | (uoff << 10) | (rn << 5) | rt;
}

A64Instr a64_LDRHw(int rt, int rn, int offset)
{
    A64_ASSERT_REG(rt, "LDRHw");
    A64_ASSERT_REG(rn, "LDRHw");
    A64_ASSERT_ALIGN(offset, 2, "LDRHw");
    A64_ASSERT_RANGE(offset, 0, 8190, "LDRHw offset");
    uint32 uoff = (uint32)offset / 2;
    return 0x79400000 | (uoff << 10) | (rn << 5) | rt;
}

A64Instr a64_STRHw(int rt, int rn, int offset)
{
    A64_ASSERT_REG(rt, "STRHw");
    A64_ASSERT_REG(rn, "STRHw");
    A64_ASSERT_ALIGN(offset, 2, "STRHw");
    A64_ASSERT_RANGE(offset, 0, 8190, "STRHw offset");
    uint32 uoff = (uint32)offset / 2;
    return 0x79000000 | (uoff << 10) | (rn << 5) | rt;
}

/* Load/Store register-indexed */
A64Instr a64_LDRw_reg(int rt, int rn, int rm, bool shift)
{
    A64_ASSERT_REG(rt, "LDRw_reg");
    A64_ASSERT_REG(rn, "LDRw_reg");
    A64_ASSERT_REG(rm, "LDRw_reg");
    // LDR Wt, [Xn, Xm{, LSL #2}]
    // 10 111000 011 Rm option(011) S 10 Rn Rt
    uint32 S = shift ? 1 : 0;
    return 0xB8606800 | (S << 12) | (rm << 16) | (rn << 5) | rt;
}

A64Instr a64_LDR_reg(int rt, int rn, int rm, bool shift)
{
    A64_ASSERT_REG(rt, "LDR_reg");
    A64_ASSERT_REG(rn, "LDR_reg");
    A64_ASSERT_REG(rm, "LDR_reg");
    // LDR Xt, [Xn, Xm{, LSL #3}]
    // 11 111000 011 Rm option(011) S 10 Rn Rt
    uint32 S = shift ? 1 : 0;
    return 0xF8606800 | (S << 12) | (rm << 16) | (rn << 5) | rt;
}

/* Load/Store pair (pre-index / post-index) */
A64Instr a64_STP_pre(int rt1, int rt2, int rn, int offset)
{
    A64_ASSERT_REG(rt1, "STP_pre");
    A64_ASSERT_REG(rt2, "STP_pre");
    A64_ASSERT_REG(rn, "STP_pre");
    A64_ASSERT_ALIGN(offset, 8, "STP_pre");
    sint32 simm7 = offset / 8;
    A64_ASSERT_RANGE(simm7, -64, 63, "STP_pre imm7");
    return 0xA9800000 | (((uint32)simm7 & 0x7F) << 15) | (rt2 << 10) | (rn << 5) | rt1;
}

A64Instr a64_LDP_post(int rt1, int rt2, int rn, int offset)
{
    A64_ASSERT_REG(rt1, "LDP_post");
    A64_ASSERT_REG(rt2, "LDP_post");
    A64_ASSERT_REG(rn, "LDP_post");
    A64_ASSERT_ALIGN(offset, 8, "LDP_post");
    sint32 simm7 = offset / 8;
    A64_ASSERT_RANGE(simm7, -64, 63, "LDP_post imm7");
    return 0xA8C00000 | (((uint32)simm7 & 0x7F) << 15) | (rt2 << 10) | (rn << 5) | rt1;
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
    A64_ASSERT_REG(rn, "BR");
    return 0xD61F0000 | (rn << 5);
}

A64Instr a64_BLR(int rn)
{
    A64_ASSERT_REG(rn, "BLR");
    return 0xD63F0000 | (rn << 5);
}

A64Instr a64_RET(int rn)
{
    A64_ASSERT_REG(rn, "RET");
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
    A64_ASSERT_REG(rt, "CBZ");
    sint32 imm19 = offset / 4;
    A64_ASSERT_RANGE(imm19, -0x40000, 0x3FFFF, "CBZ");
    return 0xB4000000 | (((uint32)imm19 & 0x7FFFF) << 5) | rt;
}

A64Instr a64_CBNZ(int rt, sint32 offset)
{
    A64_ASSERT_REG(rt, "CBNZ");
    sint32 imm19 = offset / 4;
    A64_ASSERT_RANGE(imm19, -0x40000, 0x3FFFF, "CBNZ");
    return 0xB5000000 | (((uint32)imm19 & 0x7FFFF) << 5) | rt;
}

A64Instr a64_CBZw(int rt, sint32 offset)
{
    A64_ASSERT_REG(rt, "CBZw");
    sint32 imm19 = offset / 4;
    A64_ASSERT_RANGE(imm19, -0x40000, 0x3FFFF, "CBZw");
    return 0x34000000 | (((uint32)imm19 & 0x7FFFF) << 5) | rt;
}

A64Instr a64_CBNZw(int rt, sint32 offset)
{
    A64_ASSERT_REG(rt, "CBNZw");
    sint32 imm19 = offset / 4;
    A64_ASSERT_RANGE(imm19, -0x40000, 0x3FFFF, "CBNZw");
    return 0x35000000 | (((uint32)imm19 & 0x7FFFF) << 5) | rt;
}

A64Instr a64_TBZ(int rt, int bit, sint32 offset)
{
    A64_ASSERT_REG(rt, "TBZ");
    A64_ASSERT_RANGE(bit, 0, 63, "TBZ bit");
    sint32 imm14 = offset / 4;
    A64_ASSERT_RANGE(imm14, -0x2000, 0x1FFF, "TBZ");
    uint32 b5 = (bit >> 5) & 1;
    uint32 b40 = bit & 0x1F;
    return 0x36000000 | (b5 << 31) | (b40 << 19) | (((uint32)imm14 & 0x3FFF) << 5) | rt;
}

A64Instr a64_TBNZ(int rt, int bit, sint32 offset)
{
    A64_ASSERT_REG(rt, "TBNZ");
    A64_ASSERT_RANGE(bit, 0, 63, "TBNZ bit");
    sint32 imm14 = offset / 4;
    A64_ASSERT_RANGE(imm14, -0x2000, 0x1FFF, "TBNZ");
    uint32 b5 = (bit >> 5) & 1;
    uint32 b40 = bit & 0x1F;
    return 0x37000000 | (b5 << 31) | (b40 << 19) | (((uint32)imm14 & 0x3FFF) << 5) | rt;
}

/* Misc */
/* Conditional select */
A64Instr a64_CSELw(int rd, int rn, int rm, A64Cond cond)
{
    A64_ASSERT_REG(rd, "CSELw"); A64_ASSERT_REG(rn, "CSELw"); A64_ASSERT_REG(rm, "CSELw");
    return 0x1A800000 | (rm << 16) | ((int)cond << 12) | (rn << 5) | rd;
}

A64Instr a64_CSINCw(int rd, int rn, int rm, A64Cond cond)
{
    A64_ASSERT_REG(rd, "CSINCw"); A64_ASSERT_REG(rn, "CSINCw"); A64_ASSERT_REG(rm, "CSINCw");
    return 0x1A800400 | (rm << 16) | ((int)cond << 12) | (rn << 5) | rd;
}

/* Bit field operations */
A64Instr a64_BFIw(int rd, int rn, int lsb, int width)
{
    A64_ASSERT_REG(rd, "BFIw");
    A64_ASSERT_REG(rn, "BFIw");
    A64_ASSERT_RANGE(lsb, 0, 31, "BFIw lsb");
    A64_ASSERT_RANGE(width, 1, 32 - lsb, "BFIw width");
    // BFI Wd, Wn, #lsb, #width = BFM Wd, Wn, #(32-lsb)%32, #(width-1)
    int immr = (32 - lsb) & 31;
    int imms = width - 1;
    return 0x33000000 | (immr << 16) | (imms << 10) | (rn << 5) | rd;
}

/* Data processing (2 source) */
A64Instr a64_UDIVw(int rd, int rn, int rm)
{
    A64_ASSERT_REG(rd, "UDIVw"); A64_ASSERT_REG(rn, "UDIVw"); A64_ASSERT_REG(rm, "UDIVw");
    return 0x1AC00800 | (rm << 16) | (rn << 5) | rd;
}
A64Instr a64_SDIVw(int rd, int rn, int rm)
{
    A64_ASSERT_REG(rd, "SDIVw"); A64_ASSERT_REG(rn, "SDIVw"); A64_ASSERT_REG(rm, "SDIVw");
    return 0x1AC00C00 | (rm << 16) | (rn << 5) | rd;
}
A64Instr a64_LSLVw(int rd, int rn, int rm)
{
    A64_ASSERT_REG(rd, "LSLVw"); A64_ASSERT_REG(rn, "LSLVw"); A64_ASSERT_REG(rm, "LSLVw");
    return 0x1AC02000 | (rm << 16) | (rn << 5) | rd;
}
A64Instr a64_LSRVw(int rd, int rn, int rm)
{
    A64_ASSERT_REG(rd, "LSRVw"); A64_ASSERT_REG(rn, "LSRVw"); A64_ASSERT_REG(rm, "LSRVw");
    return 0x1AC02400 | (rm << 16) | (rn << 5) | rd;
}

/* 64-bit shift variable */
A64Instr a64_LSLV(int rd, int rn, int rm)
{
    A64_ASSERT_REG(rd, "LSLV"); A64_ASSERT_REG(rn, "LSLV"); A64_ASSERT_REG(rm, "LSLV");
    return 0x9AC02000 | (rm << 16) | (rn << 5) | rd;
}
A64Instr a64_LSRV(int rd, int rn, int rm)
{
    A64_ASSERT_REG(rd, "LSRV"); A64_ASSERT_REG(rn, "LSRV"); A64_ASSERT_REG(rm, "LSRV");
    return 0x9AC02400 | (rm << 16) | (rn << 5) | rd;
}
A64Instr a64_ASRV(int rd, int rn, int rm)
{
    A64_ASSERT_REG(rd, "ASRV"); A64_ASSERT_REG(rn, "ASRV"); A64_ASSERT_REG(rm, "ASRV");
    return 0x9AC02800 | (rm << 16) | (rn << 5) | rd;
}

/* Widening multiply */
A64Instr a64_UMULL(int rd, int rn, int rm)
{
    A64_ASSERT_REG(rd, "UMULL"); A64_ASSERT_REG(rn, "UMULL"); A64_ASSERT_REG(rm, "UMULL");
    return 0x9BA07C00 | (rm << 16) | (rn << 5) | rd;
}
A64Instr a64_SMULL(int rd, int rn, int rm)
{
    A64_ASSERT_REG(rd, "SMULL"); A64_ASSERT_REG(rn, "SMULL"); A64_ASSERT_REG(rm, "SMULL");
    return 0x9B207C00 | (rm << 16) | (rn << 5) | rd;
}

/* Logical with invert */
A64Instr a64_ORNw(int rd, int rn, int rm)
{
    A64_ASSERT_REG(rd, "ORNw"); A64_ASSERT_REG(rn, "ORNw"); A64_ASSERT_REG(rm, "ORNw");
    return 0x2A200000 | (rm << 16) | (rn << 5) | rd;
}
A64Instr a64_MVNw(int rd, int rm)
{
    A64_ASSERT_REG(rd, "MVNw"); A64_ASSERT_REG(rm, "MVNw");
    return 0x2A200000 | (rm << 16) | (31 << 5) | rd;
}

/* Count leading zeros */
A64Instr a64_CLZw(int rd, int rn)
{
    A64_ASSERT_REG(rd, "CLZw"); A64_ASSERT_REG(rn, "CLZw");
    return 0x5AC01000 | (rn << 5) | rd;
}

A64Instr a64_NOP()
{
    return 0xD503201F;
}

A64Instr a64_REV(int rd, int rn)
{
    A64_ASSERT_REG(rd, "REV");
    A64_ASSERT_REG(rn, "REV");
    // REV Xd, Xn (64-bit byte reverse)
    return 0xDAC00C00 | (rn << 5) | rd;
}

A64Instr a64_REV16w(int rd, int rn)
{
    A64_ASSERT_REG(rd, "REV16w");
    A64_ASSERT_REG(rn, "REV16w");
    return 0x5AC00400 | (rn << 5) | rd;
}

A64Instr a64_REVw(int rd, int rn)
{
    A64_ASSERT_REG(rd, "REVw");
    A64_ASSERT_REG(rn, "REVw");
    // REV Wd, Wn (32-bit byte reverse)
    return 0x5AC00800 | (rn << 5) | rd;
}

/*
 *  Floating-point load/store (D-register, unsigned offset)
 *  LDR Dt, [Xn, #uimm] — 11 111101 01 imm12 Rn Rt (scaled by 8)
 *  STR Dt, [Xn, #uimm] — 11 111101 00 imm12 Rn Rt (scaled by 8)
 */
A64Instr a64_LDR_D(int dt, int rn, int uimm)
{
    A64_ASSERT_REG(dt, "LDR_D");
    A64_ASSERT_REG(rn, "LDR_D");
    A64_ASSERT_ALIGN(uimm, 8, "LDR_D");
    A64_ASSERT_RANGE(uimm, 0, 32760, "LDR_D offset");
    uint32 uoff = (uint32)uimm / 8;
    return 0xFD400000 | (uoff << 10) | (rn << 5) | dt;
}

A64Instr a64_STR_D(int dt, int rn, int uimm)
{
    A64_ASSERT_REG(dt, "STR_D");
    A64_ASSERT_REG(rn, "STR_D");
    A64_ASSERT_ALIGN(uimm, 8, "STR_D");
    A64_ASSERT_RANGE(uimm, 0, 32760, "STR_D offset");
    uint32 uoff = (uint32)uimm / 8;
    return 0xFD000000 | (uoff << 10) | (rn << 5) | dt;
}

/*
 *  Floating-point arithmetic (double-precision)
 */
A64Instr a64_FADD_D(int dd, int dn, int dm)
{
    A64_ASSERT_REG(dd, "FADD_D"); A64_ASSERT_REG(dn, "FADD_D"); A64_ASSERT_REG(dm, "FADD_D");
    // 00011110 011 Rm 001010 Rn Rd
    return 0x1E602800 | (dm << 16) | (dn << 5) | dd;
}

A64Instr a64_FSUB_D(int dd, int dn, int dm)
{
    A64_ASSERT_REG(dd, "FSUB_D"); A64_ASSERT_REG(dn, "FSUB_D"); A64_ASSERT_REG(dm, "FSUB_D");
    // 00011110 011 Rm 001110 Rn Rd
    return 0x1E603800 | (dm << 16) | (dn << 5) | dd;
}

A64Instr a64_FMUL_D(int dd, int dn, int dm)
{
    A64_ASSERT_REG(dd, "FMUL_D"); A64_ASSERT_REG(dn, "FMUL_D"); A64_ASSERT_REG(dm, "FMUL_D");
    // 00011110 011 Rm 000010 Rn Rd
    return 0x1E600800 | (dm << 16) | (dn << 5) | dd;
}

A64Instr a64_FDIV_D(int dd, int dn, int dm)
{
    A64_ASSERT_REG(dd, "FDIV_D"); A64_ASSERT_REG(dn, "FDIV_D"); A64_ASSERT_REG(dm, "FDIV_D");
    // 00011110 011 Rm 000110 Rn Rd
    return 0x1E601800 | (dm << 16) | (dn << 5) | dd;
}

A64Instr a64_FNEG_D(int dd, int dn)
{
    A64_ASSERT_REG(dd, "FNEG_D"); A64_ASSERT_REG(dn, "FNEG_D");
    // 00011110 01 100001 010000 Rn Rd
    return 0x1E614000 | (dn << 5) | dd;
}

A64Instr a64_FABS_D(int dd, int dn)
{
    A64_ASSERT_REG(dd, "FABS_D"); A64_ASSERT_REG(dn, "FABS_D");
    // 00011110 01 100000 110000 Rn Rd
    return 0x1E60C000 | (dn << 5) | dd;
}

A64Instr a64_FSQRT_D(int dd, int dn)
{
    A64_ASSERT_REG(dd, "FSQRT_D"); A64_ASSERT_REG(dn, "FSQRT_D");
    // 00011110 01 100001 110000 Rn Rd
    return 0x1E61C000 | (dn << 5) | dd;
}

/*
 *  Floating-point fused multiply-add (double-precision)
 *  FMADD Dd, Dn, Dm, Da = Da + Dn*Dm
 */
A64Instr a64_FMADD_D(int dd, int dn, int dm, int da)
{
    A64_ASSERT_REG(dd, "FMADD_D"); A64_ASSERT_REG(dn, "FMADD_D");
    A64_ASSERT_REG(dm, "FMADD_D"); A64_ASSERT_REG(da, "FMADD_D");
    // 00011111 01 0 Rm 0 Ra Rn Rd
    return 0x1F400000 | (dm << 16) | (da << 10) | (dn << 5) | dd;
}

A64Instr a64_FMSUB_D(int dd, int dn, int dm, int da)
{
    A64_ASSERT_REG(dd, "FMSUB_D"); A64_ASSERT_REG(dn, "FMSUB_D");
    A64_ASSERT_REG(dm, "FMSUB_D"); A64_ASSERT_REG(da, "FMSUB_D");
    // 00011111 01 0 Rm 1 Ra Rn Rd
    return 0x1F408000 | (dm << 16) | (da << 10) | (dn << 5) | dd;
}

A64Instr a64_FNMADD_D(int dd, int dn, int dm, int da)
{
    A64_ASSERT_REG(dd, "FNMADD_D"); A64_ASSERT_REG(dn, "FNMADD_D");
    A64_ASSERT_REG(dm, "FNMADD_D"); A64_ASSERT_REG(da, "FNMADD_D");
    // 00011111 01 1 Rm 0 Ra Rn Rd
    return 0x1F600000 | (dm << 16) | (da << 10) | (dn << 5) | dd;
}

A64Instr a64_FNMSUB_D(int dd, int dn, int dm, int da)
{
    A64_ASSERT_REG(dd, "FNMSUB_D"); A64_ASSERT_REG(dn, "FNMSUB_D");
    A64_ASSERT_REG(dm, "FNMSUB_D"); A64_ASSERT_REG(da, "FNMSUB_D");
    // 00011111 01 1 Rm 1 Ra Rn Rd
    return 0x1F608000 | (dm << 16) | (da << 10) | (dn << 5) | dd;
}

/*
 *  Floating-point conversion
 */
A64Instr a64_FCVT_S_D(int sd, int dn)
{
    A64_ASSERT_REG(sd, "FCVT_S_D"); A64_ASSERT_REG(dn, "FCVT_S_D");
    // 00011110 01 100100 010000 Rn Rd  (type=01, opc=00 → single)
    return 0x1E624000 | (dn << 5) | sd;
}

A64Instr a64_FCVT_D_S(int dd, int sn)
{
    A64_ASSERT_REG(dd, "FCVT_D_S"); A64_ASSERT_REG(sn, "FCVT_D_S");
    // 00011110 00 100010 110000 Rn Rd  (type=00, opc=01 → double)
    return 0x1E22C000 | (sn << 5) | dd;
}

/*
 *  Floating-point compare
 */
A64Instr a64_FCMP_D(int dn, int dm)
{
    A64_ASSERT_REG(dn, "FCMP_D"); A64_ASSERT_REG(dm, "FCMP_D");
    // 00011110 011 Rm 001000 Rn 00000
    return 0x1E602000 | (dm << 16) | (dn << 5);
}

A64Instr a64_FCMP_D_zero(int dn)
{
    A64_ASSERT_REG(dn, "FCMP_D_zero");
    // 00011110 011 00000 001000 Rn 01000
    return 0x1E602008 | (dn << 5);
}

/*
 *  Float to integer conversion
 */
A64Instr a64_FCVTZS_W_D(int wd, int dn)
{
    A64_ASSERT_REG(wd, "FCVTZS_W_D"); A64_ASSERT_REG(dn, "FCVTZS_W_D");
    // 0 00 11110 01 111000 000000 Rn Rd
    return 0x1E780000 | (dn << 5) | wd;
}

/*
 *  Floating-point conditional select
 */
A64Instr a64_FCSEL_D(int dd, int dn, int dm, A64Cond cond)
{
    A64_ASSERT_REG(dd, "FCSEL_D"); A64_ASSERT_REG(dn, "FCSEL_D"); A64_ASSERT_REG(dm, "FCSEL_D");
    // 00011110 011 Rm cond 11 Rn Rd
    return 0x1E600C00 | (dm << 16) | ((int)cond << 12) | (dn << 5) | dd;
}

/*
 *  Floating-point move between GPR and FPR
 */
A64Instr a64_FMOV_D_X(int dd, int xn)
{
    A64_ASSERT_REG(dd, "FMOV_D_X"); A64_ASSERT_REG(xn, "FMOV_D_X");
    // 1 00 11110 01 1 00 111 000000 Rn Rd
    return 0x9E670000 | (xn << 5) | dd;
}

A64Instr a64_FMOV_X_D(int xd, int dn)
{
    A64_ASSERT_REG(xd, "FMOV_X_D"); A64_ASSERT_REG(dn, "FMOV_X_D");
    // 1 00 11110 01 1 00 110 000000 Rn Rd
    return 0x9E660000 | (dn << 5) | xd;
}

A64Instr a64_FMOV_S_W(int sd, int wn)
{
    A64_ASSERT_REG(sd, "FMOV_S_W"); A64_ASSERT_REG(wn, "FMOV_S_W");
    // 0 00 11110 00 1 00 111 000000 Rn Rd
    return 0x1E270000 | (wn << 5) | sd;
}

A64Instr a64_FMOV_W_S(int wd, int sn)
{
    A64_ASSERT_REG(wd, "FMOV_W_S"); A64_ASSERT_REG(sn, "FMOV_W_S");
    // 0 00 11110 00 1 00 110 000000 Rn Rd
    return 0x1E260000 | (sn << 5) | wd;
}

/* PC-relative addressing */
A64Instr a64_ADRP(int rd, sint64 offset)
{
    A64_ASSERT_REG(rd, "ADRP");
    // ADRP Xd, #offset (page-aligned, +/-4GB)
    sint32 imm = (sint32)(offset >> 12);
    A64_ASSERT_RANGE(imm, -0x100000, 0xFFFFF, "ADRP page offset");
    uint32 immlo = imm & 0x3;
    uint32 immhi = (imm >> 2) & 0x7FFFF;
    return 0x90000000 | (immlo << 29) | (immhi << 5) | rd;
}

A64Instr a64_ADR(int rd, sint32 offset)
{
    A64_ASSERT_REG(rd, "ADR");
    A64_ASSERT_RANGE(offset, -0x100000, 0xFFFFF, "ADR offset");
    // ADR Xd, #offset (PC-relative, +/-1MB)
    uint32 immlo = offset & 0x3;
    uint32 immhi = (offset >> 2) & 0x7FFFF;
    return 0x10000000 | (immlo << 29) | (immhi << 5) | rd;
}
