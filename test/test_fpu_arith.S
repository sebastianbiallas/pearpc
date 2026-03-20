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
.Lsummary:
    .ascii  "\n=== FPU Arith Summary ===\n"
.Lsummarye:
.Lpass:
    .ascii  "All 34 tests passed!\n"
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
