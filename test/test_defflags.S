/*
 *  PearPC Test: Deferred CR Flags
 *
 *  Tests that compare+branch patterns work correctly when CR
 *  materialization is deferred (NZCV flags used directly).
 *
 *  Each test uses a specific compare+branch pattern and verifies
 *  the branch is taken/not-taken as expected.
 *
 *  Uses custom opcodes for output:
 *    .long 0x00333303  - print string (r3=addr, r4=length)
 *    .long 0x00333304  - exit (r3=exit code)
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

_start:
    li      30, 0       /* fail counter */

    /*
     * Test 1: cmpi + beq (equal, taken)
     */
    PRINT   .Lstr1, (.Lstr1_end - .Lstr1)
    li      3, 0
    cmpwi   0, 3, 0
    beq     .Ltest1_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Ltest2
.Ltest1_ok:
    PRINT   .Lok, 3

    /*
     * Test 2: cmpi + beq (not equal, not taken)
     */
.Ltest2:
    PRINT   .Lstr2, (.Lstr2_end - .Lstr2)
    li      3, 1
    cmpwi   0, 3, 0
    beq     .Ltest2_fail
    PRINT   .Lok, 3
    b       .Ltest3
.Ltest2_fail:
    addi    30, 30, 1
    PRINT   .Lfail, 5

    /*
     * Test 3: cmpi + blt (less than, taken)
     */
.Ltest3:
    PRINT   .Lstr3, (.Lstr3_end - .Lstr3)
    li      3, -5
    cmpwi   0, 3, 0
    blt     .Ltest3_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Ltest4
.Ltest3_ok:
    PRINT   .Lok, 3

    /*
     * Test 4: cmpi + bgt (greater, taken)
     */
.Ltest4:
    PRINT   .Lstr4, (.Lstr4_end - .Lstr4)
    li      3, 42
    cmpwi   0, 3, 10
    bgt     .Ltest4_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Ltest5
.Ltest4_ok:
    PRINT   .Lok, 3

    /*
     * Test 5: cmpi + ble (equal case of LE, taken)
     */
.Ltest5:
    PRINT   .Lstr5, (.Lstr5_end - .Lstr5)
    li      3, 7
    cmpwi   0, 3, 7
    ble     .Ltest5_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Ltest6
.Ltest5_ok:
    PRINT   .Lok, 3

    /*
     * Test 6: cmpi + ble (less case of LE, taken)
     */
.Ltest6:
    PRINT   .Lstr6, (.Lstr6_end - .Lstr6)
    li      3, 5
    cmpwi   0, 3, 7
    ble     .Ltest6_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Ltest7
.Ltest6_ok:
    PRINT   .Lok, 3

    /*
     * Test 7: cmpi + ble (greater, not taken)
     */
.Ltest7:
    PRINT   .Lstr7, (.Lstr7_end - .Lstr7)
    li      3, 10
    cmpwi   0, 3, 7
    ble     .Ltest7_fail
    PRINT   .Lok, 3
    b       .Ltest8
.Ltest7_fail:
    addi    30, 30, 1
    PRINT   .Lfail, 5

    /*
     * Test 8: cmpwi on CR2 + beq cr2 (different CR field)
     */
.Ltest8:
    PRINT   .Lstr8, (.Lstr8_end - .Lstr8)
    li      3, 99
    cmpwi   2, 3, 99
    beq     2, .Ltest8_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Ltest9
.Ltest8_ok:
    PRINT   .Lok, 3

    /*
     * Test 9: cmpwi CR0 then cmpwi CR2, branch on CR0
     * CR0 must have been flushed before CR2 clobbers NZCV
     */
.Ltest9:
    PRINT   .Lstr9, (.Lstr9_end - .Lstr9)
    li      3, 0
    cmpwi   0, 3, 0         /* CR0 = EQ */
    li      4, 5
    cmpwi   2, 4, 10        /* CR2 = LT — clobbers NZCV */
    beq     0, .Ltest9_ok   /* should branch on CR0.EQ, not NZCV */
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Ltest10
.Ltest9_ok:
    PRINT   .Lok, 3

    /*
     * Test 10: cmplwi (unsigned) + blt (unsigned less)
     */
.Ltest10:
    PRINT   .Lstr10, (.Lstr10_end - .Lstr10)
    li      3, 5
    cmplwi  0, 3, 10
    blt     .Ltest10_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Ltest11
.Ltest10_ok:
    PRINT   .Lok, 3

    /*
     * Test 11: cmp register + bne
     */
.Ltest11:
    PRINT   .Lstr11, (.Lstr11_end - .Lstr11)
    li      3, 100
    li      4, 200
    cmpw    0, 3, 4
    bne     .Ltest11_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Ltest12
.Ltest11_ok:
    PRINT   .Lok, 3

    /*
     * Test 12: counted loop — cmpwi + ble backward branch
     * This is the key pattern for the optimization.
     * Sum 1+2+...+10 = 55
     */
.Ltest12:
    PRINT   .Lstr12, (.Lstr12_end - .Lstr12)
    li      5, 0            /* sum = 0 */
    li      3, 1            /* i = 1 */
.Lloop12:
    add     5, 5, 3         /* sum += i */
    addi    3, 3, 1         /* i++ */
    cmpwi   0, 3, 10        /* i <= 10? (ble: branch if r3 <= 10) */
    ble     .Lloop12        /* back edge */
    cmpwi   0, 5, 55
    beq     .Ltest12_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Ltest13
.Ltest12_ok:
    PRINT   .Lok, 3

    /*
     * Test 13: add. (Rc=1) + beq — CR0 from ALU result
     */
.Ltest13:
    PRINT   .Lstr13, (.Lstr13_end - .Lstr13)
    li      3, 5
    li      4, -5
    add.    5, 3, 4         /* 5 + (-5) = 0, CR0.EQ */
    beq     .Ltest13_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Ltest14
.Ltest13_ok:
    PRINT   .Lok, 3

    /*
     * Test 14: add. (Rc=1) + blt — CR0.LT from negative result
     */
.Ltest14:
    PRINT   .Lstr14, (.Lstr14_end - .Lstr14)
    li      3, 5
    li      4, -10
    add.    5, 3, 4         /* 5 + (-10) = -5, CR0.LT */
    blt     .Ltest14_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Ltest15
.Ltest14_ok:
    PRINT   .Lok, 3

    /*
     * Test 15: andi. + bne — andi. always sets CR0
     */
.Ltest15:
    PRINT   .Lstr15, (.Lstr15_end - .Lstr15)
    li      3, 0xFF
    andi.   4, 3, 0x0F      /* r4 = 0x0F, CR0 = GT (nonzero positive) */
    bne     .Ltest15_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Ltest16
.Ltest15_ok:
    PRINT   .Lok, 3

    /*
     * Test 16: intervening ALU between cmpwi and branch
     * The addi should NOT affect the CR0 compare result.
     */
.Ltest16:
    PRINT   .Lstr16, (.Lstr16_end - .Lstr16)
    li      3, 42
    cmpwi   0, 3, 42        /* CR0 = EQ */
    addi    4, 3, 1          /* intervening: r4 = 43, no Rc */
    beq     .Ltest16_ok      /* should still see CR0.EQ */
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Ltest17
.Ltest16_ok:
    PRINT   .Lok, 3

    /*
     * Test 17: mfcr after cmpwi — CR must be materialized
     */
.Ltest17:
    PRINT   .Lstr17, (.Lstr17_end - .Lstr17)
    li      3, -1
    cmpwi   0, 3, 0          /* CR0 = LT */
    mfcr    5                 /* must see CR0.LT in r5 */
    rlwinm  5, 5, 1, 31, 31  /* extract bit 0 (LT) */
    cmpwi   0, 5, 1
    beq     .Ltest17_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Ltest18
.Ltest17_ok:
    PRINT   .Lok, 3

    /*
     * Test 18: bdnz loop (CTR-only, no CR)
     * Sum 1+2+...+5 = 15 using bdnz
     */
.Ltest18:
    PRINT   .Lstr18, (.Lstr18_end - .Lstr18)
    li      5, 0
    li      3, 1
    li      4, 5
    mtctr   4
.Lloop18:
    add     5, 5, 3
    addi    3, 3, 1
    bdnz    .Lloop18
    cmpwi   0, 5, 15
    beq     .Ltest18_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Ldone
.Ltest18_ok:
    PRINT   .Lok, 3

    /* ---- done ---- */
.Ldone:
    PRINT   .Lsummary, (.Lsummary_end - .Lsummary)
    mr      3, 30
    .long   0x00333304       /* exit(r3) */

    /* Strings in .text to stay within 16-bit PC-relative range */
.Lok:
    .ascii  "OK\n"
.Lfail:
    .ascii  "FAIL\n"
.Lstr1:
    .ascii  "test 1  cmpi+beq taken:   "
.Lstr1_end:
.Lstr2:
    .ascii  "test 2  cmpi+beq !taken:  "
.Lstr2_end:
.Lstr3:
    .ascii  "test 3  cmpi+blt taken:   "
.Lstr3_end:
.Lstr4:
    .ascii  "test 4  cmpi+bgt taken:   "
.Lstr4_end:
.Lstr5:
    .ascii  "test 5  cmpi+ble eq:      "
.Lstr5_end:
.Lstr6:
    .ascii  "test 6  cmpi+ble lt:      "
.Lstr6_end:
.Lstr7:
    .ascii  "test 7  cmpi+ble !taken:  "
.Lstr7_end:
.Lstr8:
    .ascii  "test 8  cmpi cr2+beq cr2: "
.Lstr8_end:
.Lstr9:
    .ascii  "test 9  cr0 then cr2:     "
.Lstr9_end:
.Lstr10:
    .ascii  "test 10 cmplwi+blt:       "
.Lstr10_end:
.Lstr11:
    .ascii  "test 11 cmpw+bne:         "
.Lstr11_end:
.Lstr12:
    .ascii  "test 12 loop cmpwi+ble:   "
.Lstr12_end:
.Lstr13:
    .ascii  "test 13 add.+beq:         "
.Lstr13_end:
.Lstr14:
    .ascii  "test 14 add.+blt:         "
.Lstr14_end:
.Lstr15:
    .ascii  "test 15 andi.+bne:        "
.Lstr15_end:
.Lstr16:
    .ascii  "test 16 cmp+alu+branch:   "
.Lstr16_end:
.Lstr17:
    .ascii  "test 17 cmp+mfcr:         "
.Lstr17_end:
.Lstr18:
    .ascii  "test 18 bdnz loop:        "
.Lstr18_end:
.Lsummary:
    .ascii  "\n=== Deferred Flags Summary ===\n"
.Lsummary_end:
