/*
 *  PearPC AArch64 JIT Test: ALU and Memory Operations
 *
 *  Tests integer ALU, load/store, compare/branch, and CR operations.
 *  Uses custom opcodes for output:
 *    .long 0x00333303  - print string (r3=addr, r4=length)
 *    .long 0x00333304  - exit (r3=exit code)
 *
 *  Each test prints "OK" or "FAIL" and the program exits with
 *  the number of failures as exit code.
 */

    .text
    .globl _start

/* Macro: print a string literal via PC-relative addressing */
.macro PRINT label, len
    bl      99f
99:
    mflr    10
    addi    3, 10, (\label - 99b)
    li      4, \len
    .long   0x00333303
.endm

/* Macro: test result - if r3 == expected, print OK, else FAIL */
.macro CHECK expected
    cmpwi   0, 3, \expected
    beq     88f
    /* FAIL */
    addi    30, 30, 1       /* increment fail counter */
    PRINT   .Lfail, 5
    b       77f
88:
    PRINT   .Lok, 3
77:
.endm

_start:
    /* r30 = fail counter */
    li      30, 0
    /* r1 = stack pointer, already set by the ELF loader (mapped_load_elf) */

    /*
     * Test 1: addi / li
     */
    PRINT   .Ltest1, (.Ltest1_end - .Ltest1)
    li      3, 42
    CHECK   42

    /*
     * Test 2: addis / lis
     */
    PRINT   .Ltest2, (.Ltest2_end - .Ltest2)
    lis     3, 1            /* r3 = 0x10000 */
    srwi    3, 3, 16        /* r3 = 1 */
    CHECK   1

    /*
     * Test 3: add
     */
    PRINT   .Ltest3, (.Ltest3_end - .Ltest3)
    li      4, 17
    li      5, 25
    add     3, 4, 5
    CHECK   42

    /*
     * Test 4: subf (subtract from)
     */
    PRINT   .Ltest4, (.Ltest4_end - .Ltest4)
    li      4, 10
    li      5, 52
    subf    3, 4, 5         /* r3 = r5 - r4 = 52 - 10 = 42 */
    CHECK   42

    /*
     * Test 5: ori / andi
     */
    PRINT   .Ltest5, (.Ltest5_end - .Ltest5)
    li      3, 0x20
    ori     3, 3, 0x0A      /* r3 = 0x2A = 42 */
    CHECK   42

    /*
     * Test 6: and
     */
    PRINT   .Ltest6, (.Ltest6_end - .Ltest6)
    li      4, 0xFF
    li      5, 0x2A
    and     3, 4, 5          /* 0xFF & 0x2A = 0x2A = 42 */
    CHECK   42

    /*
     * Test 7: xor
     */
    PRINT   .Ltest7, (.Ltest7_end - .Ltest7)
    li      4, 0x55
    li      5, 0x7F
    xor     3, 4, 5          /* 0x55 ^ 0x7F = 0x2A = 42 */
    CHECK   42

    /*
     * Test 8: slw (shift left)
     */
    PRINT   .Ltest8, (.Ltest8_end - .Ltest8)
    li      4, 21
    li      5, 1
    slw     3, 4, 5          /* 21 << 1 = 42 */
    CHECK   42

    /*
     * Test 9: srw (shift right)
     */
    PRINT   .Ltest9, (.Ltest9_end - .Ltest9)
    li      4, 168
    li      5, 2
    srw     3, 4, 5          /* 168 >> 2 = 42 */
    CHECK   42

    /*
     * Test 10: neg
     */
    PRINT   .Ltest10, (.Ltest10_end - .Ltest10)
    li      4, -42
    neg     3, 4             /* r3 = -(-42) = 42 */
    CHECK   42

    /*
     * Test 11: mullw (multiply)
     */
    PRINT   .Ltest11, (.Ltest11_end - .Ltest11)
    li      4, 6
    li      5, 7
    mullw   3, 4, 5          /* 6 * 7 = 42 */
    CHECK   42

    /*
     * Test 12: oris (OR immediate shifted)
     */
    PRINT   .Ltest12, (.Ltest12_end - .Ltest12)
    li      3, 0
    oris    3, 3, 1           /* r3 = 0x00010000 */
    srwi    3, 3, 16          /* r3 = 1 */
    CHECK   1

    /*
     * Test 13: xori
     */
    PRINT   .Ltest13, (.Ltest13_end - .Ltest13)
    li      4, 0x55
    xori    3, 4, 0x7F        /* 0x55 ^ 0x7F = 0x2A = 42 */
    CHECK   42

    /*
     * Test 14: xoris
     */
    PRINT   .Ltest14, (.Ltest14_end - .Ltest14)
    lis     4, 0x0055         /* r4 = 0x00550000 */
    xoris   3, 4, 0x007F      /* 0x00550000 ^ 0x007F0000 = 0x002A0000 */
    srwi    3, 3, 16          /* r3 = 0x002A = 42 */
    CHECK   42

    /*
     * Test 15: or (register)
     */
    PRINT   .Ltest15, (.Ltest15_end - .Ltest15)
    li      4, 0x22
    li      5, 0x08
    or      3, 4, 5           /* 0x22 | 0x08 = 0x2A = 42 */
    CHECK   42

    /*
     * Test 16: stw / lwz (store and load word)
     */
    PRINT   .Ltest16, (.Ltest16_end - .Ltest16)
    li      4, 42
    stw     4, -16(1)        /* store 42 to stack */
    li      3, 0             /* clear r3 */
    lwz     3, -16(1)        /* load it back */
    CHECK   42

    /*
     * Test 17: stb / lbz (store and load byte)
     */
    PRINT   .Ltest17, (.Ltest17_end - .Ltest17)
    li      4, 42
    stb     4, -20(1)
    li      3, 0
    lbz     3, -20(1)
    CHECK   42

    /*
     * Test 18: sth / lhz (store and load halfword)
     */
    PRINT   .Ltest18, (.Ltest18_end - .Ltest18)
    li      4, 4200
    sth     4, -24(1)
    li      3, 0
    lhz     3, -24(1)
    CHECK   4200

    /*
     * Test 19: rlwinm (rotate and mask - used for bit extraction)
     */
    PRINT   .Ltest19, (.Ltest19_end - .Ltest19)
    lis     3, 0x002A        /* r3 = 0x002A0000 */
    rlwinm  3, 3, 16, 16, 31 /* rotate left 16, mask low 16 bits = 0x002A = 42 */
    CHECK   42

    /*
     * Test 20: cmp / bgt / blt / beq
     */
    PRINT   .Ltest20, (.Ltest20_end - .Ltest20)
    li      3, 0
    li      4, 10
    li      5, 20
    cmpw    0, 4, 5
    bge     1f
    addi    3, 3, 1          /* should execute: 10 < 20 */
1:
    cmpw    0, 5, 4
    ble     2f
    addi    3, 3, 1          /* should execute: 20 > 10 */
2:
    cmpw    0, 4, 4
    bne     3f
    addi    3, 3, 1          /* should execute: 10 == 10 */
3:
    /* r3 should be 3 (all three branches taken correctly) */
    cmpwi   0, 3, 3
    beq     4f
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       5f
4:
    PRINT   .Lok, 3
5:

    /*
     * Test 21: mfspr / mtspr (link register)
     */
    PRINT   .Ltest21, (.Ltest21_end - .Ltest21)
    li      4, 42
    mtlr    4
    mflr    3
    CHECK   42

    /*
     * Test 22: mfcr / cmpwi with CR field
     */
    PRINT   .Ltest22, (.Ltest22_end - .Ltest22)
    li      4, 5
    cmpwi   0, 4, 5          /* CR0 = EQ */
    mfcr    3
    rlwinm  3, 3, 4, 28, 31  /* extract CR0 (bits 0-3) */
    andi.   3, 3, 2           /* check EQ bit */
    cmpwi   0, 3, 2
    beq     6f
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       7f
6:
    PRINT   .Lok, 3
7:

    /*
     * Summary
     */
    PRINT   .Lsummary, (.Lsummary_end - .Lsummary)

    /* Print fail count */
    cmpwi   0, 30, 0
    bne     .Lprint_fails

    PRINT   .Lall_pass, (.Lall_pass_end - .Lall_pass)
    li      3, 0
    .long   0x00333304       /* exit(0) */

.Lprint_fails:
    addi    3, 30, 0x30      /* ASCII digit */
    stb     3, -1(1)
    addi    3, 1, -1
    li      4, 1
    .long   0x00333303
    PRINT   .Lfails_suffix, (.Lfails_suffix_end - .Lfails_suffix)
    mr      3, 30
    .long   0x00333304       /* exit(fail_count) */

/* String data */
.Ltest1:
    .ascii  "test  1 addi/li:     "
.Ltest1_end:
.Ltest2:
    .ascii  "test  2 addis/lis:   "
.Ltest2_end:
.Ltest3:
    .ascii  "test  3 add:         "
.Ltest3_end:
.Ltest4:
    .ascii  "test  4 subf:        "
.Ltest4_end:
.Ltest5:
    .ascii  "test  5 ori:         "
.Ltest5_end:
.Ltest6:
    .ascii  "test  6 and:         "
.Ltest6_end:
.Ltest7:
    .ascii  "test  7 xor:         "
.Ltest7_end:
.Ltest8:
    .ascii  "test  8 slw:         "
.Ltest8_end:
.Ltest9:
    .ascii  "test  9 srw:         "
.Ltest9_end:
.Ltest10:
    .ascii  "test 10 neg:         "
.Ltest10_end:
.Ltest11:
    .ascii  "test 11 mullw:       "
.Ltest11_end:
.Ltest12:
    .ascii  "test 12 oris:        "
.Ltest12_end:
.Ltest13:
    .ascii  "test 13 xori:        "
.Ltest13_end:
.Ltest14:
    .ascii  "test 14 xoris:       "
.Ltest14_end:
.Ltest15:
    .ascii  "test 15 or:          "
.Ltest15_end:
.Ltest16:
    .ascii  "test 16 stw/lwz:     "
.Ltest16_end:
.Ltest17:
    .ascii  "test 17 stb/lbz:     "
.Ltest17_end:
.Ltest18:
    .ascii  "test 18 sth/lhz:     "
.Ltest18_end:
.Ltest19:
    .ascii  "test 19 rlwinm:      "
.Ltest19_end:
.Ltest20:
    .ascii  "test 20 cmp/branch:  "
.Ltest20_end:
.Ltest21:
    .ascii  "test 21 mtspr/mfspr: "
.Ltest21_end:
.Ltest22:
    .ascii  "test 22 mfcr/cmpwi:  "
.Ltest22_end:
.Lsummary:
    .ascii  "\n=== Summary ===\n"
.Lsummary_end:
.Lall_pass:
    .ascii  "All tests passed!\n"
.Lall_pass_end:
.Lfails_suffix:
    .ascii  " test(s) FAILED\n"
.Lfails_suffix_end:
.Lok:
    .ascii  "OK\n"
.Lfail:
    .ascii  "FAIL\n"
