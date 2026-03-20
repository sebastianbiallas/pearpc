/*
 *  PearPC AArch64 JIT Test: FPU Arithmetic
 *
 *  Comprehensive tests for FPU arithmetic instructions.
 *  Tests all native-codegen FPU ops: fabs, fnabs, fadd, fsub, fmul, fdiv,
 *  fmadd, fmsub, fnmadd, fnmsub, fadds, fsubs, fmuls, fdivs,
 *  fmadds, fmsubs, fsqrt, frsp, fctiwz, fsel, fcmpu.
 *
 *  Custom opcodes:
 *    .long 0x00333303  - print string (r3=addr, r4=length)
 *    .long 0x00333304  - exit (r3=exit code)
 *
 *  Memory layout:
 *    0x100000 - code (mapped by ELF loader)
 *    0x200000 - data (.Lscratch)
 */

    .text
    .globl _start

.macro PRINT label, len
    bl      99f
99:
    mflr    10
    addi    3, 10, (\label - 99b)
    li      4, \len
    .long   0x00333303
.endm

.macro CHECK expected
    cmpwi   0, 3, \expected
    beq     88f
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       77f
88:
    PRINT   .Lok, 3
77:
.endm

.macro CHECK32 expected
    lis     11, \expected >> 16
    ori     11, 11, \expected & 0xFFFF
    cmpw    0, 3, 11
    beq     88f
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       77f
88:
    PRINT   .Lok, 3
77:
.endm

/*
 * LOAD_DBL freg, hi, lo — load a 64-bit IEEE double into freg
 * Uses r8 as scratch pointer (must point to .Lscratch)
 */
.macro LOAD_DBL freg, hi, lo
    lis     0, \hi >> 16
    ori     0, 0, \hi & 0xFFFF
    stw     0, 0(8)
    lis     0, \lo >> 16
    ori     0, 0, \lo & 0xFFFF
    stw     0, 4(8)
    lfd     \freg, 0(8)
.endm

/*
 * CHECK_DBL freg, exp_hi, exp_lo — store freg to scratch, check both words
 */
.macro CHECK_DBL freg, exp_hi, exp_lo
    stfd    \freg, 0(8)
    lwz     3, 0(8)
    CHECK32 \exp_hi
    lwz     3, 4(8)
    CHECK32 \exp_lo
.endm

_start:
    li      30, 0           /* fail counter */

    /* Set up scratch pointer */
    lis     8, .Lscratch@ha
    ori     8, 8, .Lscratch@l

    /* ====== Test 1: fabs(-100.0) = 100.0 ====== */
    PRINT   .Lt01, (.Lt01e - .Lt01)
    LOAD_DBL 1, 0xC0590000, 0x00000000  /* f1 = -100.0 */
    fabs    2, 1
    CHECK_DBL 2, 0x40590000, 0x00000000 /* 100.0 */

    /* ====== Test 2: fabs(+100.0) = 100.0 ====== */
    PRINT   .Lt02, (.Lt02e - .Lt02)
    LOAD_DBL 1, 0x40590000, 0x00000000  /* f1 = +100.0 */
    fabs    2, 1
    CHECK_DBL 2, 0x40590000, 0x00000000

    /* ====== Test 3: fnabs(+100.0) = -100.0 ====== */
    PRINT   .Lt03, (.Lt03e - .Lt03)
    LOAD_DBL 1, 0x40590000, 0x00000000
    fnabs   2, 1
    CHECK_DBL 2, 0xC0590000, 0x00000000

    /* ====== Test 4: fadd 100.0 + 50.0 = 150.0 ====== */
    PRINT   .Lt04, (.Lt04e - .Lt04)
    LOAD_DBL 1, 0x40590000, 0x00000000  /* 100.0 */
    LOAD_DBL 2, 0x40490000, 0x00000000  /* 50.0 */
    fadd    3, 1, 2
    CHECK_DBL 3, 0x4062C000, 0x00000000

    /* ====== Test 5: fadd 100.0 + (-50.0) = 50.0 ====== */
    PRINT   .Lt05, (.Lt05e - .Lt05)
    LOAD_DBL 1, 0x40590000, 0x00000000
    LOAD_DBL 2, 0xC0490000, 0x00000000  /* -50.0 */
    fadd    3, 1, 2
    CHECK_DBL 3, 0x40490000, 0x00000000

    /* ====== Test 6: fadd (-100.0) + (-50.0) = -150.0 ====== */
    PRINT   .Lt06, (.Lt06e - .Lt06)
    LOAD_DBL 1, 0xC0590000, 0x00000000
    LOAD_DBL 2, 0xC0490000, 0x00000000
    fadd    3, 1, 2
    CHECK_DBL 3, 0xC062C000, 0x00000000

    /* ====== Test 7: fadd 100.0 + 0.0 = 100.0 ====== */
    PRINT   .Lt07, (.Lt07e - .Lt07)
    LOAD_DBL 1, 0x40590000, 0x00000000
    LOAD_DBL 2, 0x00000000, 0x00000000  /* 0.0 */
    fadd    3, 1, 2
    CHECK_DBL 3, 0x40590000, 0x00000000

    /* ====== Test 8: fsub 100.0 - 50.0 = 50.0 ====== */
    PRINT   .Lt08, (.Lt08e - .Lt08)
    LOAD_DBL 1, 0x40590000, 0x00000000
    LOAD_DBL 2, 0x40490000, 0x00000000
    fsub    3, 1, 2
    CHECK_DBL 3, 0x40490000, 0x00000000

    /* ====== Test 9: fsub 50.0 - 100.0 = -50.0 ====== */
    PRINT   .Lt09, (.Lt09e - .Lt09)
    LOAD_DBL 1, 0x40490000, 0x00000000
    LOAD_DBL 2, 0x40590000, 0x00000000
    fsub    3, 1, 2
    CHECK_DBL 3, 0xC0490000, 0x00000000

    /* ====== Test 10: fsub 100.0 - 100.0 = 0.0 ====== */
    PRINT   .Lt10, (.Lt10e - .Lt10)
    LOAD_DBL 1, 0x40590000, 0x00000000
    LOAD_DBL 2, 0x40590000, 0x00000000
    fsub    3, 1, 2
    CHECK_DBL 3, 0x00000000, 0x00000000

    /* ====== Test 11: fmul 100.0 * 50.0 = 5000.0 ====== */
    PRINT   .Lt11, (.Lt11e - .Lt11)
    LOAD_DBL 1, 0x40590000, 0x00000000
    LOAD_DBL 2, 0x40490000, 0x00000000
    fmul    3, 1, 2
    CHECK_DBL 3, 0x40B38800, 0x00000000

    /* ====== Test 12: fmul (-100.0) * 50.0 = -5000.0 ====== */
    PRINT   .Lt12, (.Lt12e - .Lt12)
    LOAD_DBL 1, 0xC0590000, 0x00000000
    LOAD_DBL 2, 0x40490000, 0x00000000
    fmul    3, 1, 2
    CHECK_DBL 3, 0xC0B38800, 0x00000000

    /* ====== Test 13: fmul 0.5 * 2.0 = 1.0 ====== */
    PRINT   .Lt13, (.Lt13e - .Lt13)
    LOAD_DBL 1, 0x3FE00000, 0x00000000  /* 0.5 */
    LOAD_DBL 2, 0x40000000, 0x00000000  /* 2.0 */
    fmul    3, 1, 2
    CHECK_DBL 3, 0x3FF00000, 0x00000000

    /* ====== Test 14: fdiv 100.0 / 50.0 = 2.0 ====== */
    PRINT   .Lt14, (.Lt14e - .Lt14)
    LOAD_DBL 1, 0x40590000, 0x00000000
    LOAD_DBL 2, 0x40490000, 0x00000000
    fdiv    3, 1, 2
    CHECK_DBL 3, 0x40000000, 0x00000000

    /* ====== Test 15: fdiv 50.0 / 100.0 = 0.5 ====== */
    PRINT   .Lt15, (.Lt15e - .Lt15)
    LOAD_DBL 1, 0x40490000, 0x00000000
    LOAD_DBL 2, 0x40590000, 0x00000000
    fdiv    3, 1, 2
    CHECK_DBL 3, 0x3FE00000, 0x00000000

    /* ====== Test 16: fdiv 100.0 / (-50.0) = -2.0 ====== */
    PRINT   .Lt16, (.Lt16e - .Lt16)
    LOAD_DBL 1, 0x40590000, 0x00000000
    LOAD_DBL 2, 0xC0490000, 0x00000000
    fdiv    3, 1, 2
    CHECK_DBL 3, 0xC0000000, 0x00000000

    /* ====== Test 17: fmadd 100*50+2 = 5002.0 ====== */
    PRINT   .Lt17, (.Lt17e - .Lt17)
    LOAD_DBL 1, 0x40590000, 0x00000000  /* frA = 100.0 */
    LOAD_DBL 2, 0x40000000, 0x00000000  /* frB = 2.0 */
    LOAD_DBL 3, 0x40490000, 0x00000000  /* frC = 50.0 */
    fmadd   4, 1, 3, 2                  /* f4 = f1*f3 + f2 = 100*50+2 = 5002 */
    CHECK_DBL 4, 0x40B38A00, 0x00000000

    /* ====== Test 18: fmsub 100*50-2 = 4998.0 ====== */
    PRINT   .Lt18, (.Lt18e - .Lt18)
    LOAD_DBL 1, 0x40590000, 0x00000000  /* frA = 100.0 */
    LOAD_DBL 2, 0x40000000, 0x00000000  /* frB = 2.0 */
    LOAD_DBL 3, 0x40490000, 0x00000000  /* frC = 50.0 */
    fmsub   4, 1, 3, 2                  /* f4 = f1*f3 - f2 = 100*50-2 = 4998 */
    CHECK_DBL 4, 0x40B38600, 0x00000000

    /* ====== Test 19: fnmadd -(100*50+2) = -5002.0 ====== */
    PRINT   .Lt19, (.Lt19e - .Lt19)
    LOAD_DBL 1, 0x40590000, 0x00000000
    LOAD_DBL 2, 0x40000000, 0x00000000
    LOAD_DBL 3, 0x40490000, 0x00000000
    fnmadd  4, 1, 3, 2
    CHECK_DBL 4, 0xC0B38A00, 0x00000000

    /* ====== Test 20: fnmsub -(100*50-2) = -4998.0 ====== */
    PRINT   .Lt20, (.Lt20e - .Lt20)
    LOAD_DBL 1, 0x40590000, 0x00000000
    LOAD_DBL 2, 0x40000000, 0x00000000
    LOAD_DBL 3, 0x40490000, 0x00000000
    fnmsub  4, 1, 3, 2
    CHECK_DBL 4, 0xC0B38600, 0x00000000

    /* ====== Test 21: fadds 1.0 + 2^-24 rounds to 1.0 in single ====== */
    PRINT   .Lt21, (.Lt21e - .Lt21)
    LOAD_DBL 1, 0x3FF00000, 0x00000000  /* 1.0 */
    LOAD_DBL 2, 0x3E700000, 0x00000000  /* 2^-24 = 5.96e-8 */
    fadds   3, 1, 2
    CHECK_DBL 3, 0x3FF00000, 0x00000000 /* rounds to 1.0 */

    /* ====== Test 22: fadd 1.0 + 2^-24 keeps precision ====== */
    PRINT   .Lt22, (.Lt22e - .Lt22)
    LOAD_DBL 1, 0x3FF00000, 0x00000000
    LOAD_DBL 2, 0x3E700000, 0x00000000
    fadd    3, 1, 2
    CHECK_DBL 3, 0x3FF00000, 0x10000000

    /* ====== Test 23: fmuls 3.0 * 1.0 = 3.0 ====== */
    PRINT   .Lt23, (.Lt23e - .Lt23)
    LOAD_DBL 1, 0x40080000, 0x00000000  /* 3.0 */
    LOAD_DBL 2, 0x3FF00000, 0x00000000  /* 1.0 */
    fmuls   3, 1, 2
    CHECK_DBL 3, 0x40080000, 0x00000000

    /* ====== Test 24: fdivs 100.0 / 50.0 = 2.0 ====== */
    PRINT   .Lt24, (.Lt24e - .Lt24)
    LOAD_DBL 1, 0x40590000, 0x00000000
    LOAD_DBL 2, 0x40490000, 0x00000000
    fdivs   3, 1, 2
    CHECK_DBL 3, 0x40000000, 0x00000000

    /* ====== Test 25: fsqrt(4.0) = 2.0 ====== */
    PRINT   .Lt25, (.Lt25e - .Lt25)
    LOAD_DBL 1, 0x40100000, 0x00000000  /* 4.0 */
    fsqrt   2, 1
    CHECK_DBL 2, 0x40000000, 0x00000000

    /* ====== Test 26: fcmpu 100.0 vs 50.0 → GT (CR0 bit 2 = 0x4) ====== */
    PRINT   .Lt26, (.Lt26e - .Lt26)
    LOAD_DBL 1, 0x40590000, 0x00000000
    LOAD_DBL 2, 0x40490000, 0x00000000
    fcmpu   0, 1, 2
    mfcr    3
    srwi    3, 3, 28        /* shift CR0 to low 4 bits */
    CHECK   0x4             /* GT */

    /* ====== Test 27: fcmpu 50.0 vs 100.0 → LT (CR0 bit 3 = 0x8) ====== */
    PRINT   .Lt27, (.Lt27e - .Lt27)
    LOAD_DBL 1, 0x40490000, 0x00000000
    LOAD_DBL 2, 0x40590000, 0x00000000
    fcmpu   0, 1, 2
    mfcr    3
    srwi    3, 3, 28
    CHECK   0x8             /* LT */

    /* ====== Test 28: fcmpu 100.0 vs 100.0 → EQ (CR0 bit 1 = 0x2) ====== */
    PRINT   .Lt28, (.Lt28e - .Lt28)
    LOAD_DBL 1, 0x40590000, 0x00000000
    LOAD_DBL 2, 0x40590000, 0x00000000
    fcmpu   0, 1, 2
    mfcr    3
    srwi    3, 3, 28
    CHECK   0x2             /* EQ */

    /* ====== Test 29: frsp rounds double to single ====== */
    PRINT   .Lt29, (.Lt29e - .Lt29)
    /* Load 1.0 + 2^-24 as double */
    LOAD_DBL 1, 0x3FF00000, 0x10000000
    frsp    2, 1
    CHECK_DBL 2, 0x3FF00000, 0x00000000 /* rounds to 1.0 */

    /* ====== Test 30: fctiwz(3.7) = 3 ====== */
    PRINT   .Lt30, (.Lt30e - .Lt30)
    LOAD_DBL 1, 0x400D9999, 0x9999999A  /* 3.7 */
    fctiwz  2, 1
    stfd    2, 0(8)
    lwz     3, 4(8)         /* low word = integer result */
    CHECK   3

    /* ====== Test 31: fsel(+1, 100, 50) → 100 (a>=0 picks frC) ====== */
    PRINT   .Lt31, (.Lt31e - .Lt31)
    LOAD_DBL 1, 0x3FF00000, 0x00000000  /* frA = +1.0 */
    LOAD_DBL 2, 0x40490000, 0x00000000  /* frB = 50.0 */
    LOAD_DBL 3, 0x40590000, 0x00000000  /* frC = 100.0 */
    fsel    4, 1, 3, 2      /* f4 = (f1>=0) ? f3 : f2 */
    CHECK_DBL 4, 0x40590000, 0x00000000

    /* ====== Test 32: fsel(-1, 100, 50) → 50 (a<0 picks frB) ====== */
    PRINT   .Lt32, (.Lt32e - .Lt32)
    LOAD_DBL 1, 0xBFF00000, 0x00000000  /* frA = -1.0 */
    LOAD_DBL 2, 0x40490000, 0x00000000  /* frB = 50.0 */
    LOAD_DBL 3, 0x40590000, 0x00000000  /* frC = 100.0 */
    fsel    4, 1, 3, 2
    CHECK_DBL 4, 0x40490000, 0x00000000

    /* ====== Test 33: fmadds 100*50+2 = 5002.0 (single) ====== */
    PRINT   .Lt33, (.Lt33e - .Lt33)
    LOAD_DBL 1, 0x40590000, 0x00000000  /* 100.0 */
    LOAD_DBL 2, 0x40000000, 0x00000000  /* 2.0 */
    LOAD_DBL 3, 0x40490000, 0x00000000  /* 50.0 */
    fmadds  4, 1, 3, 2
    CHECK_DBL 4, 0x40B38A00, 0x00000000

    /* ====== Test 34: fmsubs 100*50-2 = 4998.0 (single) ====== */
    PRINT   .Lt34, (.Lt34e - .Lt34)
    LOAD_DBL 1, 0x40590000, 0x00000000
    LOAD_DBL 2, 0x40000000, 0x00000000
    LOAD_DBL 3, 0x40490000, 0x00000000
    fmsubs  4, 1, 3, 2
    CHECK_DBL 4, 0x40B38600, 0x00000000

    /* ====== Test 35: lfs loads single 100.0, converts to double ====== */
    PRINT   .Lt35, (.Lt35e - .Lt35)
    /* Store IEEE single 100.0 = 0x42C80000 to scratch */
    lis     0, 0x42C8
    stw     0, 0(8)
    lfs     1, 0(8)                 /* f1 = single_to_double(100.0) */
    CHECK_DBL 1, 0x40590000, 0x00000000  /* double 100.0 */

    /* ====== Test 36: stfs stores double as single ====== */
    PRINT   .Lt36, (.Lt36e - .Lt36)
    LOAD_DBL 1, 0x40590000, 0x00000000  /* f1 = double 100.0 */
    stfs    1, 0(8)                 /* store as single */
    lwz     3, 0(8)                 /* read back the 32-bit single */
    CHECK32 0x42C80000              /* IEEE single 100.0 */

    /* ====== Test 37: lfs + fadd round-trip ====== */
    PRINT   .Lt37, (.Lt37e - .Lt37)
    /* Store single 1.5 = 0x3FC00000 */
    lis     0, 0x3FC0
    stw     0, 0(8)
    lfs     1, 0(8)                 /* f1 = 1.5 */
    /* Store single 2.5 = 0x40200000 */
    lis     0, 0x4020
    stw     0, 0(8)
    lfs     2, 0(8)                 /* f2 = 2.5 */
    fadd    3, 1, 2                 /* f3 = 1.5 + 2.5 = 4.0 */
    CHECK_DBL 3, 0x40100000, 0x00000000  /* double 4.0 */

    /* ====== Test 38: stfs negative value ====== */
    PRINT   .Lt38, (.Lt38e - .Lt38)
    LOAD_DBL 1, 0xC0490000, 0x00000000  /* f1 = -50.0 */
    stfs    1, 0(8)
    lwz     3, 0(8)
    CHECK32 0xC2480000              /* IEEE single -50.0 */

    /* ====== Test 39: lfsu updates rA ====== */
    PRINT   .Lt39, (.Lt39e - .Lt39)
    lis     0, 0x42C8               /* single 100.0 */
    stw     0, 16(8)                /* store at scratch+16 */
    mr      9, 8                    /* r9 = scratch base */
    lfsu    1, 16(9)                /* f1 = lfs(r9+16), r9 = r9+16 */
    CHECK_DBL 1, 0x40590000, 0x00000000
    /* Check r9 was updated */
    PRINT   .Lt39b, (.Lt39be - .Lt39b)
    subf    3, 8, 9                 /* r3 = r9 - scratch_base = 16 */
    CHECK   16

    /* ====== Test 40: stfsu updates rA ====== */
    PRINT   .Lt40, (.Lt40e - .Lt40)
    LOAD_DBL 1, 0x40590000, 0x00000000  /* 100.0 */
    mr      9, 8
    stfsu   1, 32(9)                /* store single at scratch+32, r9 = r9+32 */
    lwz     3, 0(9)                 /* read from updated r9 (=scratch+32) */
    CHECK32 0x42C80000
    /* Check r9 was updated */
    PRINT   .Lt40b, (.Lt40be - .Lt40b)
    subf    3, 8, 9
    CHECK   32

    /*
     * ====== FPSCR rounding mode tests ======
     *
     * These tests verify:
     * - mffs reads FPSCR correctly
     * - mtfsfi changes rounding mode
     * - fdiv 10/3 gives different results under different rounding modes
     * - Restoring RN=0 gives the default (round-to-nearest) result
     *
     * RN values: 0=nearest, 1=zero, 2=+inf, 3=-inf
     *
     * fdiv 10.0/3.0:
     *   RN_NEAR (0): 0x400AAAAA_AAAAAAAB (rounds up)
     *   RN_ZERO (1): 0x400AAAAA_AAAAAAAA (truncates)
     *   RN_PINF (2): 0x400AAAAA_AAAAAAAB (rounds toward +inf = up for positive)
     *   RN_MINF (3): 0x400AAAAA_AAAAAAAA (rounds toward -inf = down for positive)
     */

    /* ====== Test 41: mffs reads FPSCR ====== */
    PRINT   .Lt41, (.Lt41e - .Lt41)
    /* FPSCR should have RN=0 (default) and no exception flags */
    mffs    0                       /* f0 = FPSCR (in low 32 bits of double) */
    stfd    0, 0(8)
    lwz     3, 4(8)                 /* low word = FPSCR value */
    andi.   3, 3, 3                 /* mask RN bits */
    CHECK   0                       /* RN should be 0 (round-to-nearest) */

    /* ====== Test 42: fdiv 10/3 with RN=0 (nearest) ====== */
    PRINT   .Lt42, (.Lt42e - .Lt42)
    LOAD_DBL 1, 0x40240000, 0x00000000  /* 10.0 */
    LOAD_DBL 2, 0x40080000, 0x00000000  /* 3.0 */
    fdiv    3, 1, 2
    /* Check low word — this is the distinguishing word */
    stfd    3, 0(8)
    lwz     3, 0(8)
    CHECK32 0x400AAAAA
    PRINT   .Lt42b, (.Lt42be - .Lt42b)
    lwz     3, 4(8)
    CHECK32 0xAAAAAAAB             /* rounds UP in round-to-nearest */

    /* ====== Test 43: mtfsfi sets RN=1 (round-to-zero) ====== */
    PRINT   .Lt43, (.Lt43e - .Lt43)
    mtfsfi  7, 1                    /* set FPSCR field 7 = 0001 → RN=1 */
    mffs    0
    stfd    0, 0(8)
    lwz     3, 4(8)
    andi.   3, 3, 3
    CHECK   1                       /* RN should now be 1 */

    /* ====== Test 44: fdiv 10/3 with RN=1 (toward zero) ====== */
    PRINT   .Lt44, (.Lt44e - .Lt44)
    LOAD_DBL 1, 0x40240000, 0x00000000  /* 10.0 */
    LOAD_DBL 2, 0x40080000, 0x00000000  /* 3.0 */
    fdiv    3, 1, 2
    stfd    3, 0(8)
    lwz     3, 0(8)
    CHECK32 0x400AAAAA
    PRINT   .Lt44b, (.Lt44be - .Lt44b)
    lwz     3, 4(8)
    CHECK32 0xAAAAAAAA             /* truncates in round-to-zero */

    /* ====== Test 45: mtfsfi sets RN=2 (toward +inf) ====== */
    PRINT   .Lt45, (.Lt45e - .Lt45)
    mtfsfi  7, 2                    /* RN=2 */
    LOAD_DBL 1, 0x40240000, 0x00000000
    LOAD_DBL 2, 0x40080000, 0x00000000
    fdiv    3, 1, 2
    stfd    3, 0(8)
    lwz     3, 4(8)
    CHECK32 0xAAAAAAAB             /* positive → rounds UP toward +inf */

    /* ====== Test 46: RN=3 (toward -inf) with positive value ====== */
    PRINT   .Lt46, (.Lt46e - .Lt46)
    mtfsfi  7, 3                    /* RN=3 */
    LOAD_DBL 1, 0x40240000, 0x00000000
    LOAD_DBL 2, 0x40080000, 0x00000000
    fdiv    3, 1, 2
    stfd    3, 0(8)
    lwz     3, 4(8)
    CHECK32 0xAAAAAAAA             /* positive → rounds DOWN toward -inf */

    /* ====== Test 47: RN=3 (toward -inf) with negative value ====== */
    PRINT   .Lt47, (.Lt47e - .Lt47)
    /* -10.0 / 3.0 = -3.333... should round AWAY from zero toward -inf */
    LOAD_DBL 1, 0xC0240000, 0x00000000  /* -10.0 */
    LOAD_DBL 2, 0x40080000, 0x00000000  /* 3.0 */
    fdiv    3, 1, 2
    stfd    3, 0(8)
    lwz     3, 0(8)
    CHECK32 0xC00AAAAA             /* sign bit set */
    PRINT   .Lt47b, (.Lt47be - .Lt47b)
    lwz     3, 4(8)
    CHECK32 0xAAAAAAAB             /* negative → rounds AWAY from zero toward -inf */

    /* ====== Test 48: restore RN=0, verify fdiv gives nearest result again ====== */
    PRINT   .Lt48, (.Lt48e - .Lt48)
    mtfsfi  7, 0                    /* restore RN=0 */
    LOAD_DBL 1, 0x40240000, 0x00000000
    LOAD_DBL 2, 0x40080000, 0x00000000
    fdiv    3, 1, 2
    stfd    3, 0(8)
    lwz     3, 4(8)
    CHECK32 0xAAAAAAAB             /* back to round-to-nearest */

    /* ====== Test 49: fcmpu on cr1 (GT) ====== */
    PRINT   .Lt49, (.Lt49e - .Lt49)
    LOAD_DBL 1, 0x40590000, 0x00000000  /* 100.0 */
    LOAD_DBL 2, 0x40490000, 0x00000000  /* 50.0 */
    fcmpu   1, 1, 2                     /* compare on CR1 */
    mfcr    3
    rlwinm  3, 3, 8, 28, 31            /* extract CR1 (bits 24-27) */
    CHECK   0x4                         /* GT */

    /* ====== Test 50: fcmpu on cr7 (LT) ====== */
    PRINT   .Lt50, (.Lt50e - .Lt50)
    LOAD_DBL 1, 0x40490000, 0x00000000  /* 50.0 */
    LOAD_DBL 2, 0x40590000, 0x00000000  /* 100.0 */
    fcmpu   7, 1, 2                     /* compare on CR7 */
    mfcr    3
    andi.   3, 3, 0xF                   /* extract CR7 (bits 0-3) */
    CHECK   0x8                         /* LT */

    /* ====== Test 51: fcmpo (ordered compare, GT) ====== */
    PRINT   .Lt51, (.Lt51e - .Lt51)
    LOAD_DBL 1, 0x40590000, 0x00000000  /* 100.0 */
    LOAD_DBL 2, 0x40490000, 0x00000000  /* 50.0 */
    fcmpo   0, 1, 2
    mfcr    3
    srwi    3, 3, 28
    CHECK   0x4                         /* GT */

    /* ====== Test 52: fcmpo (ordered compare, EQ) ====== */
    PRINT   .Lt52, (.Lt52e - .Lt52)
    LOAD_DBL 1, 0x40590000, 0x00000000  /* 100.0 */
    LOAD_DBL 2, 0x40590000, 0x00000000  /* 100.0 */
    fcmpo   0, 1, 2
    mfcr    3
    srwi    3, 3, 28
    CHECK   0x2                         /* EQ */

    /* ====== Summary ====== */
    PRINT   .Lsummary, (.Lsummarye - .Lsummary)
    cmpwi   0, 30, 0
    beq     .Lall_pass
    mr      3, 30
    PRINT   .Lfailcount, (.Lfailcounte - .Lfailcount)
    li      3, 1
    .long   0x00333304      /* exit(1) */

.Lall_pass:
    PRINT   .Lpass, (.Lpasse - .Lpass)
    li      3, 0
    .long   0x00333304      /* exit(0) */

/* ============================================================
 * String constants
 * ============================================================ */
    .p2align 2
.Lok:
    .ascii  "ok\n"
.Lfail:
    .ascii  "FAIL\n"
.Lt01:
    .ascii  "test 01 fabs(-100):      "
.Lt01e:
.Lt02:
    .ascii  "test 02 fabs(+100):      "
.Lt02e:
.Lt03:
    .ascii  "test 03 fnabs(+100):     "
.Lt03e:
.Lt04:
    .ascii  "test 04 fadd 100+50:     "
.Lt04e:
.Lt05:
    .ascii  "test 05 fadd 100+(-50):  "
.Lt05e:
.Lt06:
    .ascii  "test 06 fadd -100+(-50): "
.Lt06e:
.Lt07:
    .ascii  "test 07 fadd 100+0:      "
.Lt07e:
.Lt08:
    .ascii  "test 08 fsub 100-50:     "
.Lt08e:
.Lt09:
    .ascii  "test 09 fsub 50-100:     "
.Lt09e:
.Lt10:
    .ascii  "test 10 fsub 100-100:    "
.Lt10e:
.Lt11:
    .ascii  "test 11 fmul 100*50:     "
.Lt11e:
.Lt12:
    .ascii  "test 12 fmul -100*50:    "
.Lt12e:
.Lt13:
    .ascii  "test 13 fmul 0.5*2:      "
.Lt13e:
.Lt14:
    .ascii  "test 14 fdiv 100/50:     "
.Lt14e:
.Lt15:
    .ascii  "test 15 fdiv 50/100:     "
.Lt15e:
.Lt16:
    .ascii  "test 16 fdiv 100/(-50):  "
.Lt16e:
.Lt17:
    .ascii  "test 17 fmadd 100*50+2:  "
.Lt17e:
.Lt18:
    .ascii  "test 18 fmsub 100*50-2:  "
.Lt18e:
.Lt19:
    .ascii  "test 19 fnmadd:          "
.Lt19e:
.Lt20:
    .ascii  "test 20 fnmsub:          "
.Lt20e:
.Lt21:
    .ascii  "test 21 fadds round:     "
.Lt21e:
.Lt22:
    .ascii  "test 22 fadd precision:  "
.Lt22e:
.Lt23:
    .ascii  "test 23 fmuls 3*1:       "
.Lt23e:
.Lt24:
    .ascii  "test 24 fdivs 100/50:    "
.Lt24e:
.Lt25:
    .ascii  "test 25 fsqrt(4):        "
.Lt25e:
.Lt26:
    .ascii  "test 26 fcmpu GT:        "
.Lt26e:
.Lt27:
    .ascii  "test 27 fcmpu LT:        "
.Lt27e:
.Lt28:
    .ascii  "test 28 fcmpu EQ:        "
.Lt28e:
.Lt29:
    .ascii  "test 29 frsp:            "
.Lt29e:
.Lt30:
    .ascii  "test 30 fctiwz(3.7):     "
.Lt30e:
.Lt31:
    .ascii  "test 31 fsel a>=0:       "
.Lt31e:
.Lt32:
    .ascii  "test 32 fsel a<0:        "
.Lt32e:
.Lt33:
    .ascii  "test 33 fmadds:          "
.Lt33e:
.Lt34:
    .ascii  "test 34 fmsubs:          "
.Lt34e:
.Lt35:
    .ascii  "test 35 lfs 100.0:       "
.Lt35e:
.Lt36:
    .ascii  "test 36 stfs 100.0:      "
.Lt36e:
.Lt37:
    .ascii  "test 37 lfs+fadd:        "
.Lt37e:
.Lt38:
    .ascii  "test 38 stfs -50.0:      "
.Lt38e:
.Lt39:
    .ascii  "test 39a lfsu val:       "
.Lt39e:
.Lt39b:
    .ascii  "test 39b lfsu rA:        "
.Lt39be:
.Lt40:
    .ascii  "test 40a stfsu val:      "
.Lt40e:
.Lt40b:
    .ascii  "test 40b stfsu rA:       "
.Lt40be:
.Lt41:
    .ascii  "test 41 mffs RN=0:       "
.Lt41e:
.Lt42:
    .ascii  "test 42a fdiv RN=0 hi:   "
.Lt42e:
.Lt42b:
    .ascii  "test 42b fdiv RN=0 lo:   "
.Lt42be:
.Lt43:
    .ascii  "test 43 mtfsfi RN=1:     "
.Lt43e:
.Lt44:
    .ascii  "test 44a fdiv RN=1 hi:   "
.Lt44e:
.Lt44b:
    .ascii  "test 44b fdiv RN=1 lo:   "
.Lt44be:
.Lt45:
    .ascii  "test 45 fdiv RN=2 lo:    "
.Lt45e:
.Lt46:
    .ascii  "test 46 fdiv RN=3+ lo:   "
.Lt46e:
.Lt47:
    .ascii  "test 47a fdiv RN=3- hi:  "
.Lt47e:
.Lt47b:
    .ascii  "test 47b fdiv RN=3- lo:  "
.Lt47be:
.Lt48:
    .ascii  "test 48 fdiv RN=0 again: "
.Lt48e:
.Lt49:
    .ascii  "test 49 fcmpu cr1 GT:    "
.Lt49e:
.Lt50:
    .ascii  "test 50 fcmpu cr7 LT:    "
.Lt50e:
.Lt51:
    .ascii  "test 51 fcmpo GT:        "
.Lt51e:
.Lt52:
    .ascii  "test 52 fcmpo EQ:        "
.Lt52e:
.Lsummary:
    .ascii  "\n=== FPU Arith Summary ===\n"
.Lsummarye:
.Lpass:
    .ascii  "All 52 tests passed!\n"
.Lpasse:
.Lfailcount:
    .ascii  " test(s) failed\n"
.Lfailcounte:

/* ============================================================
 * Scratch data area — on a separate page (0x200000) so writes
 * here do not trigger JIT code page invalidation.
 * ============================================================ */
    .data
    .p2align 4
.Lscratch:
    .space  256
