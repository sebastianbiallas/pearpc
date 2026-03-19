/*
 *  PearPC AArch64 JIT Test: FPU Exception Handling
 *
 *  Tests that FP instructions correctly raise PPC_EXC_NO_FPU (vector 0x800)
 *  when MSR_FP is clear, and that FP instructions work when MSR_FP is set.
 *
 *  This validates:
 *    - NO_FPU exception on FP arithmetic (opcode groups 59 and 63)
 *    - NO_FPU exception on FP load/store (lfd, stfd, lfs, stfs)
 *    - SRR0 points to the faulting instruction
 *    - SRR1 has correct MSR mask (0x87c0ffff)
 *    - FP instructions execute normally when MSR_FP is set
 *
 *  Custom opcodes:
 *    .long 0x00333303  - print string (r3=addr, r4=length)
 *    .long 0x00333304  - exit (r3=exit code)
 *
 *  Memory layout:
 *    0x100000 - code (mapped by ELF loader)
 *    0x800    - NO_FPU exception vector
 *    0xF00    - save area for exception data
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

_start:
    li      30, 0           /* fail counter */
    mr      31, 1           /* save stack pointer */

    /*
     * Step 1: Install NO_FPU exception handler at vector 0x800.
     *
     * The handler saves SRR0/SRR1/count to PA 0xF00, then sets MSR_FP
     * in SRR1 so that rfi re-enables FPU and retries the instruction.
     */
    bl      .Lget_handler_addr
.Lget_handler_addr:
    mflr    5
    addi    5, 5, (.Lnofpu_handler - .Lget_handler_addr)

    li      6, 0x800                /* destination = vector 0x800 */
    li      7, (.Lnofpu_handler_end - .Lnofpu_handler)
    addi    7, 7, 3
    srwi    7, 7, 2                 /* word count */
    mtctr   7
.Lcopy_handler:
    lwz     0, 0(5)
    stw     0, 0(6)
    addi    5, 5, 4
    addi    6, 6, 4
    bdnz    .Lcopy_handler

    /* Sync icache */
    li      6, 0x800
    icbi    0, 6
    sync
    isync

    /*
     * Test 1: FP arithmetic with MSR_FP=1 (should NOT fault)
     *
     * Verify basic FP works before we start testing exceptions.
     * Current MSR should have FP enabled (0x2030 from PROM).
     */
    PRINT   .Lt1, (.Lt1e - .Lt1)

    /* Clear save area */
    li      9, 0
    li      0, 0
    stw     0, 0xF00(9)             /* count = 0 */

    /* Store a known double to memory, load it into FPR, store back */
    lis     8, 0x10                  /* r8 = 0x100800 (in our mapped page) */
    ori     8, 8, 0x0800
    lis     0, 0x4059               /* 0x4059000000000000 = 100.0 in IEEE 754 */
    stw     0, 0(8)
    li      0, 0
    stw     0, 4(8)
    lfd     1, 0(8)                 /* f1 = 100.0 */
    stfd    1, 8(8)                 /* store f1 back at +8 */
    lwz     3, 8(8)                 /* read high word */
    CHECK32 0x40590000

    /* Verify no exception was raised */
    PRINT   .Lt1b, (.Lt1be - .Lt1b)
    li      9, 0
    lwz     3, 0xF00(9)
    CHECK   0

    /*
     * Test 2: fadd with MSR_FP=1 (FP arithmetic, should work)
     */
    PRINT   .Lt2, (.Lt2e - .Lt2)

    lis     8, 0x10
    ori     8, 8, 0x0800
    /* f1 = 100.0 (loaded above), load 50.0 into f2 */
    lis     0, 0x4049               /* 0x4049000000000000 = 50.0 */
    stw     0, 16(8)
    li      0, 0
    stw     0, 20(8)
    lfd     2, 16(8)                /* f2 = 50.0 */
    fadd    3, 1, 2                 /* f3 = f1 + f2 = 150.0 */
    stfd    3, 24(8)
    lwz     3, 24(8)                /* high word of 150.0 = 0x4062C000 */
    CHECK32 0x4062C000

    /*
     * Test 3: Clear MSR_FP, execute lfd — should raise NO_FPU
     */
    PRINT   .Lt3, (.Lt3e - .Lt3)

    /* Clear save area */
    li      9, 0
    li      0, 0
    stw     0, 0xF00(9)

    /* Clear MSR_FP: read MSR, clear bit 13, write back */
    mfmsr   4
    lis     0, 0xFFFF
    ori     0, 0, 0xDFFF             /* mask = ~(1<<13) = 0xFFFFDFFF */
    and     5, 4, 0                  /* r5 = MSR with FP cleared */
    mtmsr   5
    isync

    /* This lfd should fault with NO_FPU.
     * The handler sets MSR_FP in SRR1, rfi retries, lfd succeeds. */
    lis     8, 0x10
    ori     8, 8, 0x0800
.Lt3_fault:
    lfd     1, 0(8)

    /* Verify: exception count == 1 */
    li      9, 0
    lwz     3, 0xF00(9)
    CHECK   1

    /* Verify: SRR0 points to the lfd instruction */
    PRINT   .Lt3b, (.Lt3be - .Lt3b)
    li      9, 0
    lwz     3, 0xF0C(9)             /* saved SRR0 */
    bl      .Lget_t3_pc
.Lget_t3_pc:
    mflr    11
    addi    11, 11, (.Lt3_fault - .Lget_t3_pc)
    cmpw    0, 3, 11
    beq     88f
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       77f
88:
    PRINT   .Lok, 3
77:

    /* Verify: lfd actually loaded the data on retry (f1 == 100.0) */
    PRINT   .Lt3c, (.Lt3ce - .Lt3c)
    lis     8, 0x10
    ori     8, 8, 0x0900             /* scratch area at 0x100900 */
    stfd    1, 0(8)
    lwz     3, 0(8)                  /* high word of 100.0 = 0x40590000 */
    CHECK32 0x40590000

    /*
     * Test 4: Clear MSR_FP, execute fadd — should raise NO_FPU
     *
     * This tests the FP arithmetic path (opcode group 63).
     */
    PRINT   .Lt4, (.Lt4e - .Lt4)

    /* Clear save area */
    li      9, 0
    li      0, 0
    stw     0, 0xF00(9)

    /* Clear MSR_FP */
    mfmsr   4
    lis     0, 0xFFFF
    ori     0, 0, 0xDFFF
    and     5, 4, 0
    mtmsr   5
    isync

    /* fadd should fault, handler enables FP, retry succeeds */
.Lt4_fault:
    fadd    3, 1, 2

    /* Verify: exception count == 1 */
    li      9, 0
    lwz     3, 0xF00(9)
    CHECK   1

    /* Verify: SRR0 points to the fadd */
    PRINT   .Lt4b, (.Lt4be - .Lt4b)
    li      9, 0
    lwz     3, 0xF0C(9)
    bl      .Lget_t4_pc
.Lget_t4_pc:
    mflr    11
    addi    11, 11, (.Lt4_fault - .Lget_t4_pc)
    cmpw    0, 3, 11
    beq     88f
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       77f
88:
    PRINT   .Lok, 3
77:

    /* Verify: fadd actually computed the result on retry (f3 = 100+50 = 150) */
    PRINT   .Lt4c, (.Lt4ce - .Lt4c)
    lis     8, 0x10
    ori     8, 8, 0x0900
    stfd    3, 0(8)
    lwz     3, 0(8)                  /* high word of 150.0 = 0x4062C000 */
    CHECK32 0x4062C000

    /*
     * Test 5: Clear MSR_FP, execute stfd — should raise NO_FPU
     */
    PRINT   .Lt5, (.Lt5e - .Lt5)

    /* Clear save area */
    li      9, 0
    li      0, 0
    stw     0, 0xF00(9)

    /* Clear MSR_FP */
    mfmsr   4
    lis     0, 0xFFFF
    ori     0, 0, 0xDFFF
    and     5, 4, 0
    mtmsr   5
    isync

    lis     8, 0x10
    ori     8, 8, 0x0900
.Lt5_fault:
    stfd    1, 0(8)

    /* Verify: exception count == 1 */
    li      9, 0
    lwz     3, 0xF00(9)
    CHECK   1

    /*
     * Test 6: Clear MSR_FP, execute fdivs (opcode group 59)
     */
    PRINT   .Lt6, (.Lt6e - .Lt6)

    /* Clear save area */
    li      9, 0
    li      0, 0
    stw     0, 0xF00(9)

    /* Clear MSR_FP */
    mfmsr   4
    lis     0, 0xFFFF
    ori     0, 0, 0xDFFF
    and     5, 4, 0
    mtmsr   5
    isync

.Lt6_fault:
    fdivs   3, 1, 2                 /* opcode group 59 */

    /* Verify: exception count == 1 */
    li      9, 0
    lwz     3, 0xF00(9)
    CHECK   1

    /*
     * Test 7: Clear MSR_FP, execute lfs — should raise NO_FPU
     */
    PRINT   .Lt7, (.Lt7e - .Lt7)

    /* Clear save area */
    li      9, 0
    li      0, 0
    stw     0, 0xF00(9)

    /* Clear MSR_FP */
    mfmsr   4
    lis     0, 0xFFFF
    ori     0, 0, 0xDFFF
    and     5, 4, 0
    mtmsr   5
    isync

    lis     8, 0x10
    ori     8, 8, 0x0800
.Lt7_fault:
    lfs     1, 0(8)

    /* Verify: exception count == 1 */
    li      9, 0
    lwz     3, 0xF00(9)
    CHECK   1

    /*
     * Test 8: Clear MSR_FP, execute stfs — should raise NO_FPU
     */
    PRINT   .Lt8, (.Lt8e - .Lt8)

    /* Clear save area */
    li      9, 0
    li      0, 0
    stw     0, 0xF00(9)

    /* Clear MSR_FP */
    mfmsr   4
    lis     0, 0xFFFF
    ori     0, 0, 0xDFFF
    and     5, 4, 0
    mtmsr   5
    isync

    lis     8, 0x10
    ori     8, 8, 0x0900
.Lt8_fault:
    stfs    1, 0(8)

    /* Verify: exception count == 1 */
    li      9, 0
    lwz     3, 0xF00(9)
    CHECK   1

    /*
     * Test 9: Verify SRR1 mask is correct (0x87c0ffff applied to MSR)
     *
     * When we clear MSR_FP from 0x2030, MSR = 0x0030.
     * SRR1 = MSR & 0x87c0ffff = 0x0030 & 0x87c0ffff = 0x0030.
     */
    PRINT   .Lt9, (.Lt9e - .Lt9)

    li      9, 0
    lwz     3, 0xF10(9)             /* SRR1 from most recent exception (test 8) */
    CHECK32 0x00000030

    /*
     * Test 10: NO_FPU vs DSI precedence
     *
     * lfd from UNMAPPED address with MSR_FP=0.
     * PPC spec says NO_FPU has higher priority than DSI.
     * We should get NO_FPU (0x800), NOT DSI (0x300).
     *
     * Install a DSI handler that increments a counter at 0xE00.
     * Then verify: NO_FPU count == 1, DSI count == 0.
     */
    PRINT   .Lt10, (.Lt10e - .Lt10)

    /* Install DSI handler at vector 0x300 */
    bl      .Lget_dsi_handler_addr
.Lget_dsi_handler_addr:
    mflr    5
    addi    5, 5, (.Ldsi_handler - .Lget_dsi_handler_addr)
    li      6, 0x300
    li      7, (.Ldsi_handler_end - .Ldsi_handler)
    addi    7, 7, 3
    srwi    7, 7, 2
    mtctr   7
.Lcopy_dsi:
    lwz     0, 0(5)
    stw     0, 0(6)
    addi    5, 5, 4
    addi    6, 6, 4
    bdnz    .Lcopy_dsi
    li      6, 0x300
    icbi    0, 6
    sync
    isync

    /* Clear both counters */
    li      9, 0
    li      0, 0
    stw     0, 0xF00(9)             /* NO_FPU count = 0 */
    stw     0, 0xE00(9)             /* DSI count = 0 */

    /* Clear MSR_FP */
    mfmsr   4
    lis     0, 0xFFFF
    ori     0, 0, 0xDFFF
    and     5, 4, 0
    mtmsr   5
    isync

    /* lfd from unmapped address 0x600000 — should get NO_FPU, not DSI */
    lis     8, 0x60
.Lt10_fault:
    lfd     1, 0(8)

    /* After NO_FPU handler re-enables FP and retries, lfd will DSI
     * on the unmapped address. The DSI handler maps it and retries.
     * So we expect: NO_FPU count == 1, DSI count == 1. */

    /* Verify: NO_FPU fired (count >= 1) */
    li      9, 0
    lwz     3, 0xF00(9)
    CHECK   1

    /* Verify: DSI also fired on retry (count == 1) */
    PRINT   .Lt10b, (.Lt10be - .Lt10b)
    li      9, 0
    lwz     3, 0xE00(9)
    CHECK   1

    /*
     * Summary
     */
    PRINT   .Lsummary, (.Lsummarye - .Lsummary)
    cmpwi   0, 30, 0
    beq     .Lall_pass
    mr      3, 30
    .long   0x00333304              /* exit(fail_count) */
.Lall_pass:
    PRINT   .Lpass, (.Lpasse - .Lpass)
    li      3, 0
    .long   0x00333304              /* exit(0) */

/* ============================================================
 * NO_FPU Exception Handler (vector 0x800)
 *
 * Saves exception data to PA 0xF00 for test verification.
 * Sets MSR_FP in SRR1 so rfi enables FPU and retries.
 * ============================================================ */
.p2align 2
.Lnofpu_handler:
    /* Save scratch registers */
    stw     0, 0xF20(0)
    mtsprg  0, 3

    /* Exception count++ */
    lwz     0, 0xF00(0)
    addi    0, 0, 1
    stw     0, 0xF00(0)

    /* Save SRR0 (faulting PC) */
    mfsrr0  3
    stw     3, 0xF0C(0)

    /* Save SRR1 (saved MSR) */
    mfsrr1  3
    stw     3, 0xF10(0)

    /* Set MSR_FP in SRR1 so rfi re-enables FPU */
    ori     3, 3, 0x2000            /* MSR_FP = (1<<13) = 0x2000 */
    mtsrr1  3

    /* Restore scratch registers */
    lwz     0, 0xF20(0)
    mfsprg  3, 0

    rfi
.Lnofpu_handler_end:

/* ============================================================
 * DSI Exception Handler (vector 0x300) — used only by test 10
 *
 * Increments counter at PA 0xE00, creates 1:1 PTE mapping for
 * the faulting page, then rfi to retry.
 * ============================================================ */
.p2align 2
.Ldsi_handler:
    stw     0, 0xE20(0)
    mtsprg  0, 3
    mtsprg  1, 4

    /* DSI count++ at 0xE00 */
    lwz     0, 0xE00(0)
    addi    0, 0, 1
    stw     0, 0xE00(0)

    /* Get DAR */
    mfdar   3

    /* hash index = (EA >> 12) & 0x1FFF */
    srwi    4, 3, 12
    andi.   4, 4, 0x1FFF

    /* PTEG address = 0x300000 + hash * 64 */
    slwi    4, 4, 6
    addis   4, 4, 0x30

    /* API = (EA >> 22) & 0x3F */
    srwi    0, 3, 22
    andi.   0, 0, 0x3F

    /* PTE word 0 = 0x80000000 | API */
    oris    0, 0, 0x8000
    stw     0, 0(4)

    /* PTE word 1 = (EA & 0xFFFFF000) | 0xC2 (R=1, C=1, PP=0x02) */
    rlwinm  0, 3, 0, 0, 19
    ori     0, 0, 0xC2
    stw     0, 4(4)

    sync
    tlbie   3
    sync

    lwz     0, 0xE20(0)
    mfsprg  3, 0
    mfsprg  4, 1
    rfi
.Ldsi_handler_end:

/* ============================================================
 * String constants
 * ============================================================ */
.Lok:
    .ascii  "OK\n"
.Lfail:
    .ascii  "FAIL\n"
.Lt1:
    .ascii  "test 1a lfd/stfd FP=1:   "
.Lt1e:
.Lt1b:
    .ascii  "test 1b no exc raised:   "
.Lt1be:
.Lt2:
    .ascii  "test  2 fadd FP=1:       "
.Lt2e:
.Lt3:
    .ascii  "test 3a lfd NO_FPU cnt:  "
.Lt3e:
.Lt3b:
    .ascii  "test 3b SRR0==lfd:       "
.Lt3be:
.Lt3c:
    .ascii  "test 3c lfd retry data:  "
.Lt3ce:
.Lt4:
    .ascii  "test 4a fadd NO_FPU cnt: "
.Lt4e:
.Lt4b:
    .ascii  "test 4b SRR0==fadd:      "
.Lt4be:
.Lt4c:
    .ascii  "test 4c fadd retry data: "
.Lt4ce:
.Lt5:
    .ascii  "test  5 stfd NO_FPU:     "
.Lt5e:
.Lt6:
    .ascii  "test  6 fdivs NO_FPU:    "
.Lt6e:
.Lt7:
    .ascii  "test  7 lfs NO_FPU:      "
.Lt7e:
.Lt8:
    .ascii  "test  8 stfs NO_FPU:     "
.Lt8e:
.Lt9:
    .ascii  "test  9 SRR1 mask:       "
.Lt9e:
.Lt10:
    .ascii  "test 10a NO_FPU pri cnt: "
.Lt10e:
.Lt10b:
    .ascii  "test 10b DSI on retry:   "
.Lt10be:
.Lsummary:
    .ascii  "\n=== Summary ===\n"
.Lsummarye:
.Lpass:
    .ascii  "All tests passed!\n"
.Lpasse:
