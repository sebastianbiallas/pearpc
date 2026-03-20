/*
 *  PearPC AArch64 JIT Test: Branch loop patterns
 *
 *  Tests same-page conditional branch loops with bl calls inside,
 *  matching the pattern that caused the softirq init stall:
 *
 *    loop_top:
 *      mr r3, r30        ; pass arg
 *      addi r31, r31, 1  ; increment counter
 *      bl callee         ; call function (dispatch boundary)
 *      addi r30, r30, 20 ; update pointer
 *      cmpwi r31, N      ; check counter
 *      ble loop_top      ; same-page branch back
 *
 *  The key pattern is: ble branches to a same-page target that
 *  includes a bl (which is flowEndBlockUnreachable). The ble's
 *  return from the bl must re-enter the block at the addi and
 *  properly continue the loop with updated registers.
 *
 *  Uses custom opcodes for output:
 *    .long 0x00333303  - print string (r3=addr, r4=length)
 *    .long 0x00333304  - exit (r3=exit code)
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

_start:
    li      30, 0           /* r30 = fail counter */

    /*
     * Test 1: Simple counted loop with bl inside
     * This is the exact pattern from the kernel's softirq init.
     *
     * r28 = base pointer (incremented by 20 each iteration)
     * r29 = loop counter (0 to 7)
     * r27 = callee address
     *
     * Expected: after loop, r29 = 8, r28 = base + 8*20 = base + 160
     */
    PRINT   .Ltest1_name, (.Ltest1_name_end - .Ltest1_name)

    li      29, 0           /* r29 = counter */
    li      28, 1000        /* r28 = base (arbitrary start) */

    /* Get address of callee via bl/mflr */
    bl      .Lget_callee_addr
.Lget_callee_addr:
    mflr    27
    addi    27, 27, (.Lcallee - .Lget_callee_addr)

.Lloop1_top:
    mr      3, 28           /* arg0 = pointer */
    mr      5, 29           /* arg2 = counter */
    mr      4, 27           /* arg1 = callee (unused, just testing mr) */
    addi    29, 29, 1       /* counter++ */
    /* Call callee - this creates a dispatch boundary */
    bl      .Lcallee
    /* After return: update pointer, check loop */
    addi    28, 28, 20      /* pointer += 20 */
    cmpwi   0, 29, 8        /* compare counter with 8 */
    ble     .Lloop1_top     /* if counter <= 8, loop */

    /* Verify r29 == 9 (loop ran 9 times: 0,1,2,...,8 → exit when 9 > 8 is false, actually when 9 > 8 ble not taken) */
    /* Wait: ble loops while r29 <= 8. After addi r29 goes 1,2,...,9.
       When r29=9: cmpwi 9,8 → GT → ble not taken → exit.
       So r29 = 9 after loop. */
    cmpwi   0, 29, 9
    beq     .Ltest1_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Ltest1_done
.Ltest1_ok:
    /* Also verify r28 = 1000 + 9*20 = 1180 */
    cmpwi   0, 28, 1180
    beq     .Ltest1_r28_ok
    addi    30, 30, 1
    PRINT   .Lfail_r28, (.Lfail_r28_end - .Lfail_r28)
    b       .Ltest1_done
.Ltest1_r28_ok:
    PRINT   .Lok, 3
.Ltest1_done:

    /*
     * Test 2: Nested function call inside loop (deeper call chain)
     * r25 = accumulator, incremented by callee
     * r26 = loop counter
     */
    PRINT   .Ltest2_name, (.Ltest2_name_end - .Ltest2_name)

    li      25, 0           /* accumulator */
    li      26, 0           /* counter */

.Lloop2_top:
    mr      3, 25           /* pass accumulator */
    addi    26, 26, 1       /* counter++ */
    bl      .Lcallee_add    /* accumulator += 5 (returned in r3) */
    mr      25, 3           /* save result */
    cmpwi   0, 26, 10
    ble     .Lloop2_top

    /* r26 should be 11, r25 should be 11*5 = 55 */
    cmpwi   0, 26, 11
    bne     .Ltest2_fail
    cmpwi   0, 25, 55
    bne     .Ltest2_fail
    PRINT   .Lok, 3
    b       .Ltest2_done
.Ltest2_fail:
    addi    30, 30, 1
    PRINT   .Lfail, 5
.Ltest2_done:

    /*
     * Test 3: bdnz loop (CTR-based countdown)
     * Exercises the bdnz fast path in gen_bcx
     */
    PRINT   .Ltest3_name, (.Ltest3_name_end - .Ltest3_name)

    li      25, 0           /* accumulator */
    li      0, 100          /* loop count */
    mtctr   0

.Lloop3_top:
    addi    25, 25, 1
    bdnz    .Lloop3_top

    cmpwi   0, 25, 100
    beq     .Ltest3_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Ltest3_done
.Ltest3_ok:
    PRINT   .Lok, 3
.Ltest3_done:

    /*
     * Test 4: Loop with bl where target is on same page but
     * the bl creates a new entrypoint. This tests that the
     * same-page dispatch correctly finds or creates the
     * entrypoint for the loop body after the bl return.
     */
    PRINT   .Ltest4_name, (.Ltest4_name_end - .Ltest4_name)

    li      25, 0           /* accumulator */
    li      26, 0           /* counter */
    li      28, 100         /* base value */

.Lloop4_top:
    /* Several instructions before the bl */
    mr      3, 28
    addi    4, 26, 0x30     /* ASCII digit */
    add     25, 25, 28      /* accumulate */
    addi    26, 26, 1       /* counter++ */
    bl      .Lcallee        /* call (noop callee) */
    /* Return here - same page dispatch */
    addi    28, 28, 1       /* base++ */
    cmpwi   0, 26, 5
    ble     .Lloop4_top

    /* r26 should be 6, r28 should be 106, r25 should be 100+101+102+103+104+105 = 615 */
    cmpwi   0, 26, 6
    bne     .Ltest4_fail
    cmpwi   0, 28, 106
    bne     .Ltest4_fail
    /* Can't cmpwi with 615, use subtraction */
    li      3, 615
    cmpw    0, 25, 3
    bne     .Ltest4_fail
    PRINT   .Lok, 3
    b       .Ltest4_done
.Ltest4_fail:
    addi    30, 30, 1
    PRINT   .Lfail, 5
.Ltest4_done:

    /*
     * Test 5: blt (branch if less than)
     * r25 = 5, compare with 10 → LT → should branch
     * r25 = 10, compare with 5 → GT → should NOT branch
     */
    PRINT   .Ltest5_name, (.Ltest5_name_end - .Ltest5_name)

    li      25, 0           /* result: 0 = both paths correct */
    li      3, 5
    cmpwi   0, 3, 10
    blt     .Ltest5_lt_ok
    addi    25, 25, 1       /* should not reach here */
.Ltest5_lt_ok:
    li      3, 10
    cmpwi   0, 3, 5
    blt     .Ltest5_lt_bad  /* should NOT branch */
    b       .Ltest5_check
.Ltest5_lt_bad:
    addi    25, 25, 1
.Ltest5_check:
    cmpwi   0, 25, 0
    beq     .Ltest5_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Ltest5_done
.Ltest5_ok:
    PRINT   .Lok, 3
.Ltest5_done:

    /*
     * Test 6: bgt (branch if greater than)
     */
    PRINT   .Ltest6_name, (.Ltest6_name_end - .Ltest6_name)

    li      25, 0
    li      3, 10
    cmpwi   0, 3, 5
    bgt     .Ltest6_gt_ok
    addi    25, 25, 1
.Ltest6_gt_ok:
    li      3, 5
    cmpwi   0, 3, 10
    bgt     .Ltest6_gt_bad
    b       .Ltest6_check
.Ltest6_gt_bad:
    addi    25, 25, 1
.Ltest6_check:
    cmpwi   0, 25, 0
    beq     .Ltest6_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Ltest6_done
.Ltest6_ok:
    PRINT   .Lok, 3
.Ltest6_done:

    /*
     * Test 7: bge (branch if greater or equal)
     * Tests both GT and EQ cases should branch, LT should not.
     */
    PRINT   .Ltest7_name, (.Ltest7_name_end - .Ltest7_name)

    li      25, 0
    /* GT case: 10 >= 5 → should branch */
    li      3, 10
    cmpwi   0, 3, 5
    bge     .Ltest7_ge1_ok
    addi    25, 25, 1
.Ltest7_ge1_ok:
    /* EQ case: 5 >= 5 → should branch */
    li      3, 5
    cmpwi   0, 3, 5
    bge     .Ltest7_ge2_ok
    addi    25, 25, 1
.Ltest7_ge2_ok:
    /* LT case: 3 >= 5 → should NOT branch */
    li      3, 3
    cmpwi   0, 3, 5
    bge     .Ltest7_ge_bad
    b       .Ltest7_check
.Ltest7_ge_bad:
    addi    25, 25, 1
.Ltest7_check:
    cmpwi   0, 25, 0
    beq     .Ltest7_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Ltest7_done
.Ltest7_ok:
    PRINT   .Lok, 3
.Ltest7_done:

    /*
     * Test 8: forward beq — branch over a store, verify skipped
     * r25 = 42. Set r25 = 99, then beq over a "li r25, 0".
     * If branch works, r25 stays 99.
     */
    PRINT   .Ltest8_name, (.Ltest8_name_end - .Ltest8_name)

    li      25, 99
    li      3, 7
    cmpwi   0, 3, 7         /* EQ → should branch forward */
    beq     .Ltest8_skip
    li      25, 0           /* should be skipped */
.Ltest8_skip:
    cmpwi   0, 25, 99
    beq     .Ltest8_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Ltest8_done
.Ltest8_ok:
    PRINT   .Lok, 3
.Ltest8_done:

    /*
     * Test 9: backward bne loop — decrement r25 from 10, exit when 0
     */
    PRINT   .Ltest9_name, (.Ltest9_name_end - .Ltest9_name)

    li      25, 10
    li      26, 0           /* iteration count */
.Lloop9_top:
    addi    26, 26, 1
    addi    25, 25, -1
    cmpwi   0, 25, 0
    bne     .Lloop9_top

    /* r25 should be 0, r26 should be 10 */
    cmpwi   0, 25, 0
    bne     .Ltest9_fail
    cmpwi   0, 26, 10
    bne     .Ltest9_fail
    PRINT   .Lok, 3
    b       .Ltest9_done
.Ltest9_fail:
    addi    30, 30, 1
    PRINT   .Lfail, 5
.Ltest9_done:

    /*
     * Test 10: bdz — CTR countdown, branch when reaches zero
     * Load CTR=5, loop with bdz. The bdz branches when CTR
     * reaches 0 after decrement. So we accumulate in r25.
     */
    PRINT   .Ltest10_name, (.Ltest10_name_end - .Ltest10_name)

    li      25, 0
    li      0, 5
    mtctr   0
.Lloop10_top:
    addi    25, 25, 1
    bdz     .Lloop10_done   /* branch when CTR becomes 0 */
    b       .Lloop10_top    /* CTR != 0, keep looping */
.Lloop10_done:
    /* bdz decrements CTR then branches if CTR==0.
       CTR: 5→4→3→2→1→0. So 5 iterations, r25 = 5 */
    cmpwi   0, 25, 5
    beq     .Ltest10_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Ltest10_done
.Ltest10_ok:
    PRINT   .Lok, 3
.Ltest10_done:

    /*
     * Test 11: bctr — indirect jump via CTR
     * Load address of .Lbctr_target into CTR, jump to it.
     * Target sets r3 = 99.
     */
    PRINT   .Ltest11_name, (.Ltest11_name_end - .Ltest11_name)
    bl      1f
1:  mflr    11
    addi    11, 11, (.Lbctr_target - 1b)
    mtctr   11
    li      3, 0
    bctr
    /* should not reach here */
    li      3, -1
.Lbctr_resume:
    cmpwi   0, 3, 99
    beq     .Ltest11_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Ltest11_done
.Ltest11_ok:
    PRINT   .Lok, 3
.Ltest11_done:

    /*
     * Test 12: bctrl — indirect call via CTR, sets LR
     * Load address of .Lcallee_ret42 into CTR, call via bctrl.
     * Callee sets r3 = 42 and returns via blr.
     * After return, verify r3=42 and LR was set correctly.
     */
    PRINT   .Ltest12_name, (.Ltest12_name_end - .Ltest12_name)
    bl      1f
1:  mflr    11
    addi    11, 11, (.Lcallee_ret42 - 1b)
    mtctr   11
    li      3, 0
    bctrl
    /* should return here with r3=42 */
    cmpwi   0, 3, 42
    beq     .Ltest12_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Ltest12_done
.Ltest12_ok:
    PRINT   .Lok, 3
.Ltest12_done:

    /*
     * Test 13: blrl — call to LR, update LR
     * Set LR = address of .Lcallee_ret77, then blrl jumps there
     * and sets LR = return address. Callee returns via blr.
     */
    PRINT   .Ltest13_name, (.Ltest13_name_end - .Ltest13_name)
    bl      1f
1:  mflr    11
    addi    11, 11, (.Lcallee_ret77 - 1b)
    mtlr    11
    li      3, 0
    blrl
    /* should return here with r3=77 */
    cmpwi   0, 3, 77
    beq     .Ltest13_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Ltest13_done
.Ltest13_ok:
    PRINT   .Lok, 3
.Ltest13_done:

    /*
     * Test 14: bctr loop — use CTR as function pointer in a loop
     * Call .Lcallee_add via bctrl 3 times, accumulating r3 += 5 each time.
     */
    PRINT   .Ltest14_name, (.Ltest14_name_end - .Ltest14_name)
    bl      1f
1:  mflr    11
    addi    11, 11, (.Lcallee_add - 1b)
    mtctr   11
    li      3, 0
    bctrl                   /* r3 = 0 + 5 = 5 */
    /* CTR is clobbered by bctrl? No — bctrl doesn't decrement CTR */
    mtctr   11              /* restore CTR */
    bctrl                   /* r3 = 5 + 5 = 10 */
    mtctr   11
    bctrl                   /* r3 = 10 + 5 = 15 */
    cmpwi   0, 3, 15
    beq     .Ltest14_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Ltest14_done
.Ltest14_ok:
    PRINT   .Lok, 3
.Ltest14_done:

    /* Summary */
    cmpwi   0, 30, 0
    bne     .Lsummary_fail
    PRINT   .Lpass, (.Lpass_end - .Lpass)
    li      3, 0
    .long   0x00333304      /* exit 0 */

.Lsummary_fail:
    PRINT   .Lfailsum, (.Lfailsum_end - .Lfailsum)
    mr      3, 30
    .long   0x00333304      /* exit with fail count */

/*
 * Callee functions (must be on the same page as the loop for
 * the same-page dispatch test to be meaningful).
 */

/* Simple no-op callee: just returns */
.Lcallee:
    blr

/* Callee that adds 5 to r3 and returns */
.Lcallee_add:
    addi    3, 3, 5
    blr

/* Callee that sets r3 = 42 and returns */
.Lcallee_ret42:
    li      3, 42
    blr

/* Callee that sets r3 = 77 and returns */
.Lcallee_ret77:
    li      3, 77
    blr

/* Target for bctr test — sets r3 = 99, then jumps back */
.Lbctr_target:
    li      3, 99
    bl      1f
1:  mflr    11
    addi    11, 11, (.Lbctr_resume - 1b)
    mtctr   11
    bctr

/* String data */
.Ltest1_name:
    .ascii  "Test 1 (counted loop + bl): "
.Ltest1_name_end:

.Ltest2_name:
    .ascii  "Test 2 (accumulator loop + bl): "
.Ltest2_name_end:

.Ltest3_name:
    .ascii  "Test 3 (bdnz loop): "
.Ltest3_name_end:

.Ltest4_name:
    .ascii  "Test 4 (loop + bl + multi-reg): "
.Ltest4_name_end:

.Ltest5_name:
    .ascii  "Test 5 (blt): "
.Ltest5_name_end:

.Ltest6_name:
    .ascii  "Test 6 (bgt): "
.Ltest6_name_end:

.Ltest7_name:
    .ascii  "Test 7 (bge): "
.Ltest7_name_end:

.Ltest8_name:
    .ascii  "Test 8 (forward beq skip): "
.Ltest8_name_end:

.Ltest9_name:
    .ascii  "Test 9 (backward bne loop): "
.Ltest9_name_end:

.Ltest10_name:
    .ascii  "Test 10 (bdz): "
.Ltest10_name_end:

.Ltest11_name:
    .ascii  "Test 11 (bctr): "
.Ltest11_name_end:

.Ltest12_name:
    .ascii  "Test 12 (bctrl): "
.Ltest12_name_end:

.Ltest13_name:
    .ascii  "Test 13 (blrl): "
.Ltest13_name_end:

.Ltest14_name:
    .ascii  "Test 14 (bctrl loop): "
.Ltest14_name_end:

.Lok:
    .ascii  "OK\n"
.Lfail:
    .ascii  "FAIL\n"
.Lfail_r28:
    .ascii  "FAIL (r28 wrong)\n"
.Lfail_r28_end:
.Lpass:
    .ascii  "All 14 branch tests PASSED\n"
.Lpass_end:
.Lfailsum:
    .ascii  "FAILED - see above\n"
.Lfailsum_end:
