/*
 *  PearPC Test: Mid-block branch target liveness bug
 *
 *  Reproduces the CFG solver bug where a branch targets the middle
 *  of an already-discovered block. The solver uses the containing
 *  block's live_in, which says CR6 is dead (killed before the target),
 *  but jumping to the mid-block target skips the kill.
 *
 *  Pattern:
 *    cmpwi cr6, r5, 100   ; cr6=NE, flushed to CPU state
 *    cmpwi cr0, r5, 0     ; forces cr6 flush via clobberFlags
 *    cmpwi cr6, r6, 62    ; cr6=EQ (deferred, NOT yet in CPU state)
 *    beq cr6, MID          ; TAKEN. deadTarget=1 → cr6 NOT flushed!
 *    ...                   ; fall-through block kills cr6 before MID
 *    cmpwi cr6, r8, 200   ; kills cr6 in fall-through block
 *    ...
 *  MID:
 *    beq cr6, OK           ; reads cr6 from CPU state → stale NE → FAIL
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
     * Test 1: beq cr6 to mid-block target (taken path)
     *
     * cr6 is set to NE in CPU state first, then overwritten to EQ
     * via deferred flags. The optimization skips flushing EQ to CPU
     * state, so at the target cr6 reads stale NE.
     */
    PRINT   .Lstr1, (.Lstr1_end - .Lstr1)

    li      5, 0
    li      6, 62

    /* Set cr6 = NE in CPU state */
    cmpwi   6, 5, 100       /* cr6 = NE (0 != 100), deferred */
    cmpwi   0, 5, 0         /* clobberFlags → cr6 NE flushed to CPU state */

    /* Set cr6 = EQ via deferred flags (NZCV = EQ) */
    cmpwi   6, 6, 62        /* cr6 = EQ (62 == 62), deferred */

    /* Branch to mid-block target using cr6 */
    beq     6, .Lmid1       /* TAKEN. deadTarget=1 → cr6 NOT flushed! */

    /* Fall-through block (DFS discovers this BEFORE .Lmid1): */
    addi    8, 5, 1          /* harmless */
    cmpwi   6, 8, 200        /* KILLS cr6 (1 != 200) */
    nop
    nop

.Lmid1:
    /* Fresh entrypoint. beq cr6 reads cr6 from CPU state.
     * With BUG:  CPU state cr6 = NE (stale) → NOT taken → FAIL
     * With FIX:  CPU state cr6 = EQ (flushed) → taken → OK */
    beq     6, .Ltest1_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Ltest2
.Ltest1_ok:
    PRINT   .Lok, 3

    /*
     * Test 2: same pattern but branch NOT taken (fall-through path)
     *
     * cr6 is NE (5 != 62), so beq is not taken. Fall through
     * executes the block including cmpwi cr6 at r8=1 vs 200 → NE.
     * At .Lmid2, cr6 should be NE → beq NOT taken → OK.
     */
.Ltest2:
    PRINT   .Lstr2, (.Lstr2_end - .Lstr2)

    li      5, 0
    li      6, 5           /* r6 = 5, not 62 */

    cmpwi   6, 5, 100
    cmpwi   0, 5, 0

    cmpwi   6, 6, 62        /* cr6 = NE (5 != 62), deferred */
    beq     6, .Lmid2       /* NOT taken (cr6 != EQ) */

    addi    8, 5, 1
    cmpwi   6, 8, 200        /* kills cr6 */
    nop
    nop

.Lmid2:
    /* cr6 should be NE (from cmpwi cr6,r8,200 in fall-through) */
    beq     6, .Ltest2_fail
    PRINT   .Lok, 3
    b       .Ltest3
.Ltest2_fail:
    addi    30, 30, 1
    PRINT   .Lfail, 5

    /*
     * Test 3: bgt cr6 to mid-block target (taken path)
     *
     * Same pattern but with GT instead of EQ.
     */
.Ltest3:
    PRINT   .Lstr3, (.Lstr3_end - .Lstr3)

    li      5, 0
    li      6, 100          /* r6 = 100 */

    cmpwi   6, 5, 100        /* cr6 = LT (0 < 100) */
    cmpwi   0, 5, 0          /* flush cr6 LT to CPU state */

    cmpwi   6, 6, 62         /* cr6 = GT (100 > 62), deferred */
    bgt     6, .Lmid3        /* TAKEN. deadTarget=1 → cr6 NOT flushed! */

    addi    8, 5, 1
    cmpwi   6, 8, 200         /* kills cr6 */
    nop
    nop

.Lmid3:
    /* With BUG: CPU state cr6 = LT (stale) → bgt NOT taken → FAIL */
    bgt     6, .Ltest3_ok
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       .Ldone
.Ltest3_ok:
    PRINT   .Lok, 3

.Ldone:
    mr      3, 30
    .long   0x00333304  /* exit */

.Lstr1:
    .ascii "test 1 (mid-block beq cr6, taken): "
.Lstr1_end:
.Lstr2:
    .ascii "test 2 (mid-block beq cr6, fallthru): "
.Lstr2_end:
.Lstr3:
    .ascii "test 3 (mid-block bgt cr6, taken): "
.Lstr3_end:
.Lok:
    .ascii "OK\n"
.Lfail:
    .ascii "FAIL\n"
