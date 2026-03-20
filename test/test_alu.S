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
     * Test 23: mfmsr (read MSR — should have IR=1, DR=1 at minimum)
     */
    PRINT   .Ltest23, (.Ltest23_end - .Ltest23)
    mfmsr   3
    /* MSR should have IR (bit 5) and DR (bit 4) set since we run with translation on */
    andi.   4, 3, 0x30           /* mask IR|DR = bits 5+4 = 0x30 */
    cmpwi   0, 4, 0x30
    beq     .Lt23_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Lt23_end
.Lt23_ok:
    PRINT   .Lok, 3
.Lt23_end:

    /*
     * Test 24: mfmsr / mtmsr round-trip (set and read back EE bit)
     */
    PRINT   .Ltest24, (.Ltest24_end - .Ltest24)
    mfmsr   4                    /* save original MSR */
    ori     5, 4, 0x8000         /* set EE (bit 15) */
    mtmsr   5
    mfmsr   3
    andi.   6, 3, 0x8000         /* check EE is set */
    mtmsr   4                    /* restore original MSR */
    cmpwi   0, 6, 0
    bne     .Lt24_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Lt24_end
.Lt24_ok:
    PRINT   .Lok, 3
.Lt24_end:

    /*
     * Test 25: mfmsr confirms MSR_PR=0 (supervisor mode)
     */
    PRINT   .Ltest25, (.Ltest25_end - .Ltest25)
    mfmsr   3
    andi.   4, 3, 0x4000         /* MSR_PR = bit 14 */
    cmpwi   0, 4, 0
    beq     .Lt25_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Lt25_end
.Lt25_ok:
    PRINT   .Lok, 3
.Lt25_end:

    /*
     * Test 26: mulli (multiply low immediate)
     */
    PRINT   .Ltest26, (.Ltest26_end - .Ltest26)
    li      3, 7
    mulli   3, 3, 100
    CHECK   700

    /*
     * Test 27: mulhwu (multiply high word unsigned)
     * 0x80000000 * 4 = 0x200000000 → high word = 2
     */
    PRINT   .Ltest27, (.Ltest27_end - .Ltest27)
    lis     3, 0x8000        /* r3 = 0x80000000 */
    li      4, 4
    mulhwu  3, 3, 4
    CHECK   2

    /*
     * Test 28: rlwimi (rotate left word immediate then mask insert)
     * Insert bits 24-27 of rotated rS into rA
     * rS=0xFF, SH=24 → rotated=0xFF000000, MB=4, ME=7 → mask=0x0F000000
     * rA=0x12345678 → result = 0x1F345678
     */
    PRINT   .Ltest28, (.Ltest28_end - .Ltest28)
    lis     3, 0x1234
    ori     3, 3, 0x5678     /* r3 = 0x12345678 */
    li      4, 0xFF
    rlwimi  3, 4, 24, 4, 7   /* insert nibble */
    lis     11, 0x1F34
    ori     11, 11, 0x5678
    cmpw    0, 3, 11
    beq     88f
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       77f
88:
    PRINT   .Lok, 3
77:

    /*
     * Test 29: nor (NOR)
     * nor r3, r3, r3 = NOT r3
     */
    PRINT   .Ltest29, (.Ltest29_end - .Ltest29)
    li      3, 0
    nor     3, 3, 3           /* ~0 = 0xFFFFFFFF = -1 */
    CHECK   -1

    /*
     * Test 30: orc (OR with Complement)
     * orc r3, r3, r4 = r3 | ~r4
     */
    PRINT   .Ltest30, (.Ltest30_end - .Ltest30)
    li      3, 0x0F
    li      4, 0x0F
    orc     3, 3, 4          /* 0x0F | ~0x0F = 0x0F | 0xFFFFFFF0 = 0xFFFFFFFF = -1 */
    CHECK   -1

    /*
     * Test 31: cntlzw (count leading zeros)
     */
    PRINT   .Ltest31, (.Ltest31_end - .Ltest31)
    li      3, 1              /* leading zeros = 31 */
    cntlzw  3, 3
    CHECK   31

    /*
     * Test 32: subfic (subtract from immediate carrying)
     * subfic r3, r3, 100 → r3 = 100 - 10 = 90
     */
    PRINT   .Ltest32, (.Ltest32_end - .Ltest32)
    li      3, 10
    subfic  3, 3, 100
    CHECK   90

    /*
     * Test 33: lwbrx/stwbrx (byte-reversed load/store)
     * Store 0x12345678, load byte-reversed → 0x78563412
     */
    PRINT   .Ltest33, (.Ltest33_end - .Ltest33)
    lis     3, 0x1234
    ori     3, 3, 0x5678     /* r3 = 0x12345678 */
    stw     3, -16(1)        /* store normally */
    lwbrx   3, 0, 1          /* load byte-reversed from r1+0... */
    /* Actually lwbrx needs EA = (rA|0)+rB, use stack-relative */
    li      4, -16
    add     5, 1, 4          /* r5 = &stack[-16] */
    li      4, 0
    lwbrx   3, 4, 5          /* r3 = byte-reversed load */
    lis     11, 0x7856
    ori     11, 11, 0x3412
    cmpw    0, 3, 11
    beq     88f
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       77f
88:
    PRINT   .Lok, 3
77:

    /*
     * Test 34: stwbrx (store byte-reversed, then load normally)
     */
    PRINT   .Ltest34, (.Ltest34_end - .Ltest34)
    lis     3, 0x1234
    ori     3, 3, 0x5678     /* r3 = 0x12345678 */
    li      4, -16
    add     5, 1, 4
    li      4, 0
    stwbrx  3, 4, 5          /* store byte-reversed */
    lwz     3, -16(1)        /* load normally → should be 0x78563412 */
    lis     11, 0x7856
    ori     11, 11, 0x3412
    cmpw    0, 3, 11
    beq     88f
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       77f
88:
    PRINT   .Lok, 3
77:

    /*
     * Test 35: mulhwu edge case (large values)
     * 0xFFFFFFFF * 0xFFFFFFFF → high word = 0xFFFFFFFE
     */
    PRINT   .Ltest35, (.Ltest35_end - .Ltest35)
    li      3, -1             /* r3 = 0xFFFFFFFF */
    li      4, -1             /* r4 = 0xFFFFFFFF */
    mulhwu  3, 3, 4
    lis     11, 0xFFFF
    ori     11, 11, 0xFFFE
    cmpw    0, 3, 11
    beq     88f
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       77f
88:
    PRINT   .Lok, 3
77:

    /*
     * Test 36: srw (shift right word)
     */
    PRINT   .Ltest36, (.Ltest36_end - .Ltest36)
    lis     3, 0x8000         /* r3 = 0x80000000 */
    li      4, 16
    srw     3, 3, 4           /* 0x80000000 >> 16 = 0x8000 */
    CHECK32 0x00008000

    /*
     * Test 37: srw with shift >= 32 → must return 0
     */
    PRINT   .Ltest37, (.Ltest37_end - .Ltest37)
    li      3, -1             /* 0xFFFFFFFF */
    li      4, 32
    srw     3, 3, 4
    CHECK   0

    /*
     * Test 38: slw (shift left word)
     */
    PRINT   .Ltest38, (.Ltest38_end - .Ltest38)
    li      3, 1
    li      4, 20
    slw     3, 3, 4           /* 1 << 20 = 0x100000 */
    CHECK32 0x00100000

    /*
     * Test 39: slw with shift >= 32 → must return 0
     */
    PRINT   .Ltest39, (.Ltest39_end - .Ltest39)
    li      3, -1
    li      4, 33
    slw     3, 3, 4
    CHECK   0

    /*
     * Test 40: divwu (unsigned divide)
     */
    PRINT   .Ltest40, (.Ltest40_end - .Ltest40)
    li      3, 100
    li      4, 7
    divwu   3, 3, 4           /* 100 / 7 = 14 */
    CHECK   14

    /*
     * Test 41: addic (add immediate carrying)
     * 0xFFFFFFFF + 1 = 0, CA=1
     */
    PRINT   .Ltest41, (.Ltest41_end - .Ltest41)
    li      3, -1             /* 0xFFFFFFFF */
    addic   3, 3, 1           /* 0xFFFFFFFF + 1 = 0, carry out */
    CHECK   0

    /*
     * Test 42: subfic (subtract from immediate carrying)
     * subfic r3, r3, 50 → 50 - 30 = 20
     */
    PRINT   .Ltest42, (.Ltest42_end - .Ltest42)
    li      3, 30
    subfic  3, 3, 50
    CHECK   20

    /*
     * Test 43: adde (add extended with carry in)
     * Set CA=1 via addic overflow, then adde 5+3+1=9
     */
    PRINT   .Ltest43, (.Ltest43_end - .Ltest43)
    li      4, -1
    addic   4, 4, 1           /* sets CA=1 */
    li      3, 5
    li      4, 3
    adde    3, 3, 4           /* 5 + 3 + CA(1) = 9 */
    CHECK   9

    /*
     * Test 44: subfe (subtract from extended)
     * CA=1 from addic overflow, subfe r3,r4,r5 = ~r4+r5+CA
     */
    PRINT   .Ltest44, (.Ltest44_end - .Ltest44)
    li      4, -1
    addic   4, 4, 1           /* sets CA=1 */
    li      4, 3
    li      5, 10
    subfe   3, 4, 5           /* ~3 + 10 + 1 = 0xFFFFFFFC + 10 + 1 = 7 */
    CHECK   7

    /*
     * Test 45: srawi (shift right algebraic immediate)
     * -16 >> 2 = -4 (arithmetic shift)
     */
    PRINT   .Ltest45, (.Ltest45_end - .Ltest45)
    li      3, -16
    srawi   3, 3, 2
    CHECK   -4

    /*
     * Test 46: mfcr / mtcrf round-trip
     */
    PRINT   .Ltest46, (.Ltest46_end - .Ltest46)
    li      3, -4
    cmpwi   0, 3, -4          /* sets CR0 to EQ */
    mfcr    3
    /* CR0 EQ bit = bit 29 of CR = 0x20000000 */
    /* Check that EQ bit is set in CR0 */
    rlwinm  3, 3, 4, 28, 31   /* extract CR0 field */
    andi.   3, 3, 2            /* EQ bit = 2 */
    CHECK   2

    /*
     * Test 47: lwarx/stwcx. — successful reservation
     *
     * lwarx loads a word and sets a reservation.
     * stwcx. with matching reservation should store and set CR0.EQ.
     */
    PRINT   .Ltest47, (.Ltest47_end - .Ltest47)

    lis     8, .Lscratch@ha
    ori     8, 8, .Lscratch@l
    lis     0, 0xDEAD
    ori     0, 0, 0xBEEF
    stw     0, 0(8)                 /* mem = 0xDEADBEEF */
    lwarx   3, 0, 8                 /* r3 = 0xDEADBEEF, reservation set */
    lis     0, 0xCAFE
    ori     0, 0, 0xBABE
    stwcx.  0, 0, 8                 /* store 0xCAFEBABE (should succeed) */
    bne     .Lt47_fail
    lwz     3, 0(8)                 /* verify store happened */
    CHECK32 0xCAFEBABE
    b       .Lt47_done
.Lt47_fail:
    addi    30, 30, 1
    PRINT   .Lfail, 5
.Lt47_done:

    /*
     * Test 48: lwarx — correct load value
     *
     * Verify lwarx returns the word at the effective address.
     */
    PRINT   .Ltest48, (.Ltest48_end - .Ltest48)

    lis     8, .Lscratch@ha
    ori     8, 8, .Lscratch@l
    lis     0, 0x1234
    ori     0, 0, 0x5678
    stw     0, 0(8)
    lwarx   3, 0, 8
    CHECK32 0x12345678

    /*
     * Test 49: stwcx. without reservation — should NOT store, CR0.EQ=0
     *
     * After a successful stwcx., the reservation is cleared.
     * A second stwcx. without lwarx should fail (CR0.EQ=0).
     */
    PRINT   .Ltest49, (.Ltest49_end - .Ltest49)

    lis     8, .Lscratch@ha
    ori     8, 8, .Lscratch@l
    lis     0, 0xAAAA
    ori     0, 0, 0xBBBB
    stw     0, 0(8)                 /* mem = 0xAAAABBBB */
    lwarx   3, 0, 8                 /* reserve */
    stwcx.  0, 0, 8                 /* consume reservation (succeeds) */
    /* Now try again without lwarx — should fail */
    lis     0, 0x1111
    ori     0, 0, 0x2222
    stwcx.  0, 0, 8                 /* no reservation → should NOT store */
    beq     .Lt49_fail              /* if EQ set, stwcx wrongly succeeded */
    lwz     3, 0(8)                 /* mem should still be 0xAAAABBBB */
    CHECK32 0xAAAABBBB
    b       .Lt49_done
.Lt49_fail:
    addi    30, 30, 1
    PRINT   .Lfail, 5
.Lt49_done:

    /*
     * Test 50: stwcx. with modified memory — should NOT store
     *
     * lwarx reads value V, another store changes memory to V2,
     * stwcx. re-reads and compares with reserve (V). Since V != V2,
     * stwcx. should fail (CR0.EQ=0) and NOT write.
     */
    PRINT   .Ltest50, (.Ltest50_end - .Ltest50)

    lis     8, .Lscratch@ha
    ori     8, 8, .Lscratch@l
    lis     0, 0x1111
    ori     0, 0, 0x1111
    stw     0, 0(8)                 /* mem = 0x11111111 */
    lwarx   3, 0, 8                 /* r3 = 0x11111111, reserve = 0x11111111 */
    /* Intervening store changes memory */
    lis     0, 0x2222
    ori     0, 0, 0x2222
    stw     0, 0(8)                 /* mem = 0x22222222 (reserve still 0x11111111) */
    lis     0, 0x3333
    ori     0, 0, 0x3333
    stwcx.  0, 0, 8                 /* re-reads 0x22222222 != reserve 0x11111111 → fail */
    beq     .Lt50_fail              /* if EQ set, stwcx wrongly succeeded */
    lwz     3, 0(8)                 /* mem should still be 0x22222222 */
    CHECK32 0x22222222
    b       .Lt50_done
.Lt50_fail:
    addi    30, 30, 1
    PRINT   .Lfail, 5
.Lt50_done:

    /*
     * Test 51: lwarx with rA+rB addressing
     */
    PRINT   .Ltest51, (.Ltest51_end - .Ltest51)

    lis     8, .Lscratch@ha
    ori     8, 8, .Lscratch@l
    li      7, 16                   /* offset */
    lis     0, 0xFEED
    ori     0, 0, 0xFACE
    stw     0, 16(8)                /* mem[scratch+16] = 0xFEEDFACE */
    lwarx   3, 8, 7                 /* EA = r8 + r7 = scratch+16 */
    CHECK32 0xFEEDFACE

    /*
     * Test 52: addze (rA=5, CA=1 → rD=6, CA=0)
     */
    PRINT   .Ltest52, (.Ltest52_end - .Ltest52)
    li      4, 5
    /* Set CA by doing addic with overflow: 0xFFFFFFFF + 1 = 0, CA=1 */
    li      5, -1
    addic   5, 5, 1             /* CA = 1 */
    addze   3, 4                /* r3 = 5 + 1 = 6 */
    CHECK   6

    /*
     * Test 53: addze (rA=5, CA=0 → rD=5, CA=0)
     */
    PRINT   .Ltest53, (.Ltest53_end - .Ltest53)
    li      4, 5
    /* Clear CA: addic 0+0 = 0, CA=0 */
    li      5, 0
    addic   5, 5, 0             /* CA = 0 */
    addze   3, 4                /* r3 = 5 + 0 = 5 */
    CHECK   5

    /*
     * Test 54: addze CA output (0xFFFFFFFF + CA=1 → 0, CA=1)
     */
    PRINT   .Ltest54, (.Ltest54_end - .Ltest54)
    li      5, -1
    addic   5, 5, 1             /* CA = 1 */
    li      4, -1               /* rA = 0xFFFFFFFF */
    addze   3, 4                /* r3 = 0xFFFFFFFF + 1 = 0, CA=1 */
    CHECK   0
    /* Verify CA is set by chaining another addze */
    li      4, 0
    addze   3, 4                /* r3 = 0 + CA(1) = 1 */
    CHECK   1

    /*
     * Test 55: addme (rA=5, CA=1 → rD=5, CA=1)
     */
    PRINT   .Ltest55, (.Ltest55_end - .Ltest55)
    li      5, -1
    addic   5, 5, 1             /* CA = 1 */
    li      4, 5
    addme   3, 4                /* r3 = 5 + 1 + (-1) = 5, CA=1 (rA||CA) */
    CHECK   5

    /*
     * Test 56: addme (rA=0, CA=0 → rD=-1, CA=0)
     */
    PRINT   .Ltest56, (.Ltest56_end - .Ltest56)
    li      5, 0
    addic   5, 5, 0             /* CA = 0 */
    li      4, 0
    addme   3, 4                /* r3 = 0 + 0 + (-1) = -1, CA=0 */
    CHECK   -1

    /*
     * Test 57: mcrf (copy CR field)
     */
    PRINT   .Ltest57, (.Ltest57_end - .Ltest57)
    /* Set CR0 to GT by comparing 1 > 0 */
    li      4, 1
    cmpwi   0, 4, 0             /* CR0 = GT */
    /* Set CR1 to LT by comparing -1 < 0 */
    li      4, -1
    cmpwi   1, 4, 0             /* CR1 = LT */
    /* Copy CR0 → CR2 */
    mcrf    2, 0
    /* Now CR2 should be GT. Branch on CR2 GT. */
    bgt     2, .Lt57_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Lt57_done
.Lt57_ok:
    PRINT   .Lok, 3
.Lt57_done:

    /*
     * Test 58: mcrf different direction (CR1=LT → CR3)
     */
    PRINT   .Ltest58, (.Ltest58_end - .Ltest58)
    li      4, -1
    cmpwi   1, 4, 0             /* CR1 = LT */
    li      4, 1
    cmpwi   3, 4, 0             /* CR3 = GT */
    mcrf    3, 1                /* CR3 = CR1 = LT */
    blt     3, .Lt58_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Lt58_done
.Lt58_ok:
    PRINT   .Lok, 3
.Lt58_done:

    /*
     * Test 59: mulhw (positive * positive)
     * 0x10000 * 0x10000 = 0x100000000 → high word = 1
     */
    PRINT   .Ltest59, (.Ltest59_end - .Ltest59)
    lis     4, 1                /* r4 = 0x10000 */
    lis     5, 1                /* r5 = 0x10000 */
    mulhw   3, 4, 5
    CHECK   1

    /*
     * Test 60: mulhw (negative * positive)
     * -1 * 0x10000 = -0x10000 = 0xFFFFFFFF_FFFF0000 → high word = 0xFFFFFFFF
     */
    PRINT   .Ltest60, (.Ltest60_end - .Ltest60)
    li      4, -1               /* r4 = 0xFFFFFFFF */
    lis     5, 1                /* r5 = 0x10000 */
    mulhw   3, 4, 5
    CHECK   -1

    /*
     * Test 61: sraw by 1 (positive → logical shift)
     * 0x40 >> 1 = 0x20, CA = 0
     */
    PRINT   .Ltest61, (.Ltest61_end - .Ltest61)
    li      4, 0x40
    li      5, 1
    /* Clear CA first */
    li      6, 0
    addic   6, 6, 0
    sraw    3, 4, 5
    CHECK   0x20
    /* Verify CA = 0 by checking addze */
    li      4, 0
    addze   3, 4
    CHECK   0

    /*
     * Test 62: sraw by 1 (negative, no shifted-out bit)
     * 0xFFFFFFFE >> 1 = 0xFFFFFFFF, CA = 0
     */
    PRINT   .Ltest62, (.Ltest62_end - .Ltest62)
    li      4, -2               /* 0xFFFFFFFE */
    li      5, 1
    li      6, 0
    addic   6, 6, 0
    sraw    3, 4, 5
    CHECK   -1
    /* CA should be 0 (shifted-out bit was 0) */
    li      4, 0
    addze   3, 4
    CHECK   0

    /*
     * Test 63: sraw by 1 (negative, shifted-out bit set → CA=1)
     * 0xFFFFFFFF >> 1 = 0xFFFFFFFF, CA = 1
     */
    PRINT   .Ltest63, (.Ltest63_end - .Ltest63)
    li      4, -1               /* 0xFFFFFFFF */
    li      5, 1
    li      6, 0
    addic   6, 6, 0
    sraw    3, 4, 5
    CHECK   -1
    /* CA should be 1 */
    li      4, 0
    addze   3, 4
    CHECK   1

    /*
     * Test 64: sraw by 32 (negative → all 1s, CA=1 if any bit set)
     * 0x80000001 >> 32 = 0xFFFFFFFF, CA = 1
     */
    PRINT   .Ltest64, (.Ltest64_end - .Ltest64)
    lis     4, 0x8000           /* r4 = 0x80000000 */
    ori     4, 4, 1             /* r4 = 0x80000001 */
    li      5, 32
    li      6, 0
    addic   6, 6, 0
    sraw    3, 4, 5
    CHECK   -1
    /* CA should be 1 */
    li      4, 0
    addze   3, 4
    CHECK   1

    /*
     * Test 65: sraw by 0 (no shift, CA=0)
     */
    PRINT   .Ltest65, (.Ltest65_end - .Ltest65)
    li      4, 42
    li      5, 0
    li      6, 0
    addic   6, 6, 0
    sraw    3, 4, 5
    CHECK   42

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
.Ltest23:
    .ascii  "test 23 mfmsr IR|DR: "
.Ltest23_end:
.Ltest24:
    .ascii  "test 24 mtmsr/mfmsr: "
.Ltest24_end:
.Ltest25:
    .ascii  "test 25 mfmsr !PR:   "
.Ltest25_end:
.Ltest26:
    .ascii  "test 26 mulli:       "
.Ltest26_end:
.Ltest27:
    .ascii  "test 27 mulhwu:      "
.Ltest27_end:
.Ltest28:
    .ascii  "test 28 rlwimi:      "
.Ltest28_end:
.Ltest29:
    .ascii  "test 29 nor:         "
.Ltest29_end:
.Ltest30:
    .ascii  "test 30 orc:         "
.Ltest30_end:
.Ltest31:
    .ascii  "test 31 cntlzw:      "
.Ltest31_end:
.Ltest32:
    .ascii  "test 32 subfic:      "
.Ltest32_end:
.Ltest33:
    .ascii  "test 33 lwbrx:       "
.Ltest33_end:
.Ltest34:
    .ascii  "test 34 stwbrx:      "
.Ltest34_end:
.Ltest35:
    .ascii  "test 35 mulhwu big:  "
.Ltest35_end:
.Ltest36:
    .ascii  "test 36 srw:         "
.Ltest36_end:
.Ltest37:
    .ascii  "test 37 srw >=32:    "
.Ltest37_end:
.Ltest38:
    .ascii  "test 38 slw:         "
.Ltest38_end:
.Ltest39:
    .ascii  "test 39 slw >=32:    "
.Ltest39_end:
.Ltest40:
    .ascii  "test 40 divwu:       "
.Ltest40_end:
.Ltest41:
    .ascii  "test 41 addic:       "
.Ltest41_end:
.Ltest42:
    .ascii  "test 42 subfic:      "
.Ltest42_end:
.Ltest43:
    .ascii  "test 43 adde:        "
.Ltest43_end:
.Ltest44:
    .ascii  "test 44 subfe:       "
.Ltest44_end:
.Ltest45:
    .ascii  "test 45 srawi:       "
.Ltest45_end:
.Ltest46:
    .ascii  "test 46 mfcr:        "
.Ltest46_end:
.Ltest47:
    .ascii  "test 47 lwarx+stwcx: "
.Ltest47_end:
.Ltest48:
    .ascii  "test 48 lwarx val:   "
.Ltest48_end:
.Ltest49:
    .ascii  "test 49 stwcx no res:"
.Ltest49_end:
.Ltest50:
    .ascii  "test 50 stwcx stale: "
.Ltest50_end:
.Ltest51:
    .ascii  "test 51 lwarx rA+rB: "
.Ltest51_end:
.Ltest52:
    .ascii  "test 52 addze:       "
.Ltest52_end:
.Ltest53:
    .ascii  "test 53 addze no CA: "
.Ltest53_end:
.Ltest54:
    .ascii  "test 54 addze CA out:"
.Ltest54_end:
.Ltest55:
    .ascii  "test 55 addme CA=1:  "
.Ltest55_end:
.Ltest56:
    .ascii  "test 56 addme CA=0:  "
.Ltest56_end:
.Ltest57:
    .ascii  "test 57 mcrf:        "
.Ltest57_end:
.Ltest58:
    .ascii  "test 58 mcrf LT:     "
.Ltest58_end:
.Ltest59:
    .ascii  "test 59 mulhw:       "
.Ltest59_end:
.Ltest60:
    .ascii  "test 60 mulhw neg:   "
.Ltest60_end:
.Ltest61:
    .ascii  "test 61 sraw pos:    "
.Ltest61_end:
.Ltest62:
    .ascii  "test 62 sraw neg:    "
.Ltest62_end:
.Ltest63:
    .ascii  "test 63 sraw CA:     "
.Ltest63_end:
.Ltest64:
    .ascii  "test 64 sraw 32:     "
.Ltest64_end:
.Ltest65:
    .ascii  "test 65 sraw 0:      "
.Ltest65_end:
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

    .data
    .p2align 4
.Lscratch:
    .space  256
