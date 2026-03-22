/*
 *  PearPC AArch64 JIT Test: DSI Exception Handling
 *
 *  Tests that load/store to unmapped memory correctly raises DSI (0x300)
 *  and that the exception handler can map the page and retry.
 *
 *  Also tests lwarx/stwcx. with DSI on first access.
 *
 *  Custom opcodes:
 *    .long 0x00333303  - print string (r3=addr, r4=length)
 *    .long 0x00333304  - exit (r3=exit code)
 *
 *  Memory layout:
 *    0x100000 - code (mapped by ELF loader)
 *    0x500000 - unmapped region (will fault on access)
 *    Stack around 0x200000 (mapped by ELF loader)
 *
 *  Strategy:
 *    Install a DSI handler at vector 0x300.
 *    The handler creates a page table entry for the faulting address
 *    by writing a PTE into the hash table, then returns (rfi).
 *    The faulting instruction re-executes and succeeds.
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

    /* Save stack pointer */
    mr      31, 1

    /*
     * Step 1: Install DSI exception handler at vector 0x300.
     *
     * The PROM sets up the page table at PA 0x300000 with SDR1 = 0x300003.
     * The hash table is 256KB (2^18 bytes) at PA 0x300000.
     *
     * Our DSI handler will:
     *   - Read DAR (the faulting address)
     *   - Create a simple 1:1 PTE mapping (EA = PA)
     *   - Insert it into the hash table
     *   - rfi to retry
     *
     * For simplicity, we use a direct PTE insert at a known hash slot.
     * The test address 0x500000 will hash to a specific PTEG.
     */

    /* Install handler: copy handler code to PA 0x300 (vector address).
     * Since MMU is on with 1:1 mapping at low addresses, EA 0x300 = PA 0x300.
     * The PROM maps low memory, so we can write there. */

    bl      .Lget_handler_addr
.Lget_handler_addr:
    mflr    5                       /* r5 = address of this label */
    addi    5, 5, (.Ldsi_handler - .Lget_handler_addr)

    li      6, 0x300                /* r6 = destination (vector 0x300) */
    li      7, (.Ldsi_handler_end - .Ldsi_handler)
    addi    7, 7, 3
    srwi    7, 7, 2                 /* r7 = word count (rounded up) */
    mtctr   7
.Lcopy_handler:
    lwz     0, 0(5)
    stw     0, 0(6)
    addi    5, 5, 4
    addi    6, 6, 4
    bdnz    .Lcopy_handler

    /* Sync instruction cache for the vector area */
    li      6, 0x300
    icbi    0, 6
    sync
    isync

    /*
     * Test 1: lwz from unmapped address — verify DSI exception data
     */
    PRINT   .Lt1, (.Lt1e - .Lt1)

    /* Clear the save area at EA 0xF00 (1:1 mapped by PROM) */
    li      9, 0
    li      0, 0
    stw     0, 0xF00(9)             /* count = 0 */
    stw     0, 0xF04(9)             /* DAR = 0 */
    stw     0, 0xF08(9)             /* DSISR = 0 */
    stw     0, 0xF0C(9)             /* SRR0 = 0 */
    stw     0, 0xF10(9)             /* SRR1 = 0 */

    lis     8, 0x50                 /* r8 = 0x500000 */
.Lt1_fault:
    lwz     3, 0(8)                 /* FAULT → DSI → handler maps → retry → success */

    /* Verify: DSI count == 1 */
    li      9, 0
    lwz     3, 0xF00(9)
    CHECK   1

    /* Verify: DAR == 0x500000 */
    PRINT   .Lt1b, (.Lt1be - .Lt1b)
    li      9, 0
    lwz     3, 0xF04(9)
    CHECK32 0x00500000

    /* Verify: DSISR has "page not found" bit (bit 1 = 0x40000000) */
    PRINT   .Lt1c, (.Lt1ce - .Lt1c)
    li      9, 0
    lwz     3, 0xF08(9)
    lis     11, 0x4000
    and     3, 3, 11
    CHECK32 0x40000000

    /* Verify: SRR1 == saved MSR (should be 0x2030 = IR|DR|ME) */
    PRINT   .Lt1d, (.Lt1de - .Lt1d)
    li      9, 0
    lwz     3, 0xF10(9)
    CHECK32 0x00002030

    /*
     * Test 2: stw to the now-mapped address (should NOT trigger DSI)
     */
    PRINT   .Lt2, (.Lt2e - .Lt2)

    lis     8, 0x50
    lis     0, 0xCAFE
    ori     0, 0, 0xBABE            /* r0 = 0xCAFEBABE */
    stw     0, 0(8)                 /* store to 0x500000 (now mapped) */
    lwz     3, 0(8)                 /* load it back */
    CHECK32 0xCAFEBABE

    /*
     * Test 3: sth/lhz through mapped address
     */
    PRINT   .Lt3, (.Lt3e - .Lt3)

    lis     8, 0x50
    li      0, 0x1234
    sth     0, 4(8)                 /* store halfword at 0x500004 */
    li      3, 0
    lhz     3, 4(8)                 /* load it back */
    CHECK   0x1234

    /*
     * Test 4: stb/lbz through mapped address
     */
    PRINT   .Lt4, (.Lt4e - .Lt4)

    lis     8, 0x50
    li      0, 0xAB
    stb     0, 8(8)                 /* store byte at 0x500008 */
    li      3, 0
    lbz     3, 8(8)                 /* load it back */
    CHECK   0xAB

    /*
     * Test 5: stw to UNMAPPED page (write DSI) — verify DSISR indicates write
     */
    PRINT   .Lt5, (.Lt5e - .Lt5)

    /* Clear save area */
    li      9, 0
    li      0, 0
    stw     0, 0xF00(9)
    stw     0, 0xF08(9)

    lis     8, 0x50
    ori     8, 8, 0x2000            /* r8 = 0x502000 */
    lis     0, 0xDEAD
    ori     0, 0, 0xC0DE
    stw     0, 0(8)                 /* WRITE FAULT → DSI → map → retry */
    lwz     3, 0(8)
    CHECK32 0xDEADC0DE

    /* Verify: DSISR has page-not-found (0x40000000) + store (0x02000000) */
    PRINT   .Lt5b, (.Lt5be - .Lt5b)
    li      9, 0
    lwz     3, 0xF08(9)
    lis     11, 0x4200              /* expect 0x42000000: page not found + store */
    and     3, 3, 11
    CHECK32 0x42000000

    /*
     * Test 6: sth to unmapped page (halfword write DSI)
     */
    PRINT   .Lt6, (.Lt6e - .Lt6)

    lis     8, 0x50
    ori     8, 8, 0x3000            /* r8 = 0x503000 */
    li      0, 0x5678
    sth     0, 0(8)                 /* WRITE FAULT → DSI → map → retry */
    li      3, 0
    lhz     3, 0(8)
    CHECK   0x5678

    /*
     * Test 7: stb to unmapped page (byte write DSI)
     */
    PRINT   .Lt7, (.Lt7e - .Lt7)

    lis     8, 0x50
    ori     8, 8, 0x4000            /* r8 = 0x504000 */
    li      0, 0xAA
    stb     0, 0(8)                 /* WRITE FAULT → DSI → map → retry */
    li      3, 0
    lbz     3, 0(8)
    CHECK   0xAA

    /*
     * Test 8: lhz from unmapped page (halfword read DSI)
     */
    PRINT   .Lt8, (.Lt8e - .Lt8)

    lis     8, 0x50
    ori     8, 8, 0x5000            /* r8 = 0x505000 */
    lhz     3, 0(8)                 /* READ FAULT → DSI → map → retry */
    /* Value will be 0 (uninitialized memory), just verify no crash */
    PRINT   .Lok, 3

    /*
     * Test 9: lbz from unmapped page (byte read DSI)
     */
    PRINT   .Lt9, (.Lt9e - .Lt9)

    lis     8, 0x50
    ori     8, 8, 0x6000            /* r8 = 0x506000 */
    lbz     3, 0(8)                 /* READ FAULT → DSI → map → retry */
    PRINT   .Lok, 3

    /*
     * Test 10: lwz from second unmapped page (sequential DSI)
     */
    PRINT   .Lt10, (.Lt10e - .Lt10)

    lis     8, 0x50
    ori     8, 8, 0x7000            /* r8 = 0x507000 */
    lwz     3, 0(8)                 /* FAULT → DSI → map → retry */
    PRINT   .Lok, 3

    /*
     * Test 11: stw then lwz on same DSI-mapped page — verify data survives
     */
    PRINT   .Lt11, (.Lt11e - .Lt11)

    lis     8, 0x50
    ori     8, 8, 0x8000            /* r8 = 0x508000 (unmapped) */
    lis     0, 0x1234
    ori     0, 0, 0x5678
    stw     0, 0(8)                 /* WRITE FAULT → map → retry → store 0x12345678 */
    li      0, 0
    stw     0, 4(8)                 /* store 0 at +4 (no fault, already mapped) */
    lwz     3, 0(8)                 /* load back from +0 */
    CHECK32 0x12345678

    /*
     * Test 12: lwarx from unmapped page (atomic read DSI)
     */
    PRINT   .Lt12, (.Lt12e - .Lt12)

    lis     8, 0x50
    ori     8, 8, 0x9000            /* r8 = 0x509000 (unmapped) */
    lwarx   3, 0, 8                 /* FAULT → DSI → map → retry → reserve */
    /* Now store a value via stwcx. */
    lis     0, 0xBEEF
    ori     0, 0, 0xCAFE
    stwcx.  0, 0, 8                 /* conditional store (should succeed) */
    bne     .Lt12_fail
    lwz     3, 0(8)                 /* verify */
    CHECK32 0xBEEFCAFE
    b       .Lt12_done
.Lt12_fail:
    addi    30, 30, 1
    PRINT   .Lfail, 5
.Lt12_done:

    /*
     * Test 13: stwcx. to unmapped page (atomic write DSI)
     * lwarx on mapped page, stwcx. on unmapped page.
     */
    PRINT   .Lt13, (.Lt13e - .Lt13)

    /* lwarx on an already-mapped page (0x509000 from test 12) */
    lis     8, 0x50
    ori     8, 8, 0x9000
    lwarx   3, 0, 8                 /* reserve on mapped page */

    /* stwcx. re-reads the word to compare with reserve, so it DOES access
     * memory and CAN trigger DSI even if the reservation won't match.
     * Test with lwarx + stwcx. both to same unmapped page: */
    lis     8, 0x50
    ori     8, 8, 0xA000            /* r8 = 0x50A000 (unmapped) */
    lwarx   3, 0, 8                 /* FAULT → DSI → map → retry → reserve */
    lis     0, 0xFACE
    ori     0, 0, 0xFEED
    stwcx.  0, 0, 8                 /* conditional store (should succeed, page now mapped) */
    bne     .Lt13_fail
    lwz     3, 0(8)
    CHECK32 0xFACEFEED
    b       .Lt13_done
.Lt13_fail:
    addi    30, 30, 1
    PRINT   .Lfail, 5
.Lt13_done:

    /*
     * Test 14: Multiple stores to different offsets on DSI-mapped page
     */
    PRINT   .Lt14, (.Lt14e - .Lt14)

    lis     8, 0x50
    ori     8, 8, 0xB000            /* r8 = 0x50B000 (unmapped) */
    li      0, 0x11
    stb     0, 0(8)                 /* WRITE FAULT → map → retry */
    li      0, 0x22
    stb     0, 1(8)                 /* no fault (mapped) */
    li      0, 0x33
    stb     0, 2(8)
    li      0, 0x44
    stb     0, 3(8)
    lwz     3, 0(8)                 /* read full word: should be 0x11223344 */
    CHECK32 0x11223344

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
 * DSI Exception Handler
 *
 * Installed at vector 0x300. When a load/store faults:
 *   SRR0 = faulting PC
 *   SRR1 = saved MSR
 *   DAR  = faulting address
 *   DSISR = fault type bits
 *
 * This handler creates a 1:1 PTE (EA = PA) for the faulting page
 * by writing into the hash table. Then rfi to retry.
 *
 * PTE format (32-bit PPC):
 *   PTE word 0: V(1) | VSID(24) | H(1) | API(6)
 *   PTE word 1: RPN(20) | R(1) | C(1) | WIMG(4) | X(1) | PP(2)
 *
 * For 1:1 mapping with VSID=0 (default for the PROM setup):
 *   VSID = 0, API = (EA >> 22) & 0x3F
 *   RPN = PA page number
 *   V=1, H=0, R=1, C=1, PP=0x02 (read/write)
 *
 * Hash table at PA 0x300000, 256KB, mask = 0x1FFF (8192 PTEGs)
 * Primary hash = (VSID ^ (EA>>12)) & mask
 * Each PTEG = 8 PTEs × 8 bytes = 64 bytes
 * ============================================================ */
.p2align 2
.Ldsi_handler:
    /* Save r0, r3, r4 — we need all three as scratch.
     * r0 may be part of the faulting instruction (e.g. stw r0, ...).
     * Use SPRG0 for r3, SPRG1 for r4, and PA 0xF20 for r0. */
    stw     0, 0xF20(0)             /* save r0 to PA 0xF20 (real mode) */
    mtsprg  0, 3
    mtsprg  1, 4

    /* Save exception data to PA 0xF00 for test verification. */

    /* DSI count++ */
    lwz     0, 0xF00(0)
    addi    0, 0, 1
    stw     0, 0xF00(0)             /* *(0xF00) = count */

    /* Save DAR */
    mfdar   3
    stw     3, 0xF04(0)             /* *(0xF04) = DAR */

    /* Save DSISR */
    mfspr   0, 18                   /* SPR 18 = DSISR */
    stw     0, 0xF08(0)             /* *(0xF08) = DSISR */

    /* Save SRR0 */
    mfsrr0  0
    stw     0, 0xF0C(0)             /* *(0xF0C) = SRR0 */

    /* Save SRR1 */
    mfsrr1  0
    stw     0, 0xF10(0)             /* *(0xF10) = SRR1 */

    /* Compute PTE for 1:1 mapping.
     * VSID = 0 (set by PROM), so:
     *   hash = (0 ^ (EA >> 12)) & 0x1FFF
     *   API = (EA >> 22) & 0x3F
     *   PTE0 = 0x80000000 | (API)    — V=1, VSID=0, H=0
     *   PTE1 = (EA & 0xFFFFF000) | 0xC2  — RPN=EA page, R=1, C=1, PP=0x02
     */

    /* r4 = hash index = (EA >> 12) & 0x1FFF */
    srwi    4, 3, 12
    andi.   4, 4, 0x1FFF

    /* PTEG address = 0x300000 + hash * 64 */
    slwi    4, 4, 6                 /* hash * 64 */
    addis   4, 4, 0x30              /* + 0x300000 */

    /* API = (EA >> 22) & 0x3F */
    srwi    0, 3, 22
    andi.   0, 0, 0x3F

    /* PTE word 0 = 0x80000000 | API */
    oris    0, 0, 0x8000            /* V=1 */

    /* Store PTE word 0 at first slot of PTEG */
    stw     0, 0(4)

    /* PTE word 1 = (EA & 0xFFFFF000) | 0xC2 */
    rlwinm  0, 3, 0, 0, 19         /* r0 = EA & 0xFFFFF000 (RPN) */
    ori     0, 0, 0xC2              /* R=1, C=1, PP=0x02 */
    stw     0, 4(4)

    /* Sync and invalidate TLB for the faulting address */
    sync
    tlbie   3
    sync

    /* Restore registers */
    lwz     0, 0xF20(0)             /* restore r0 from PA 0xF20 */
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
    .ascii  "test 1a lwz DSI count:   "
.Lt1e:
.Lt1b:
    .ascii  "test 1b DAR==0x500000:   "
.Lt1be:
.Lt1c:
    .ascii  "test 1c DSISR page flt:  "
.Lt1ce:
.Lt1d:
    .ascii  "test 1d SRR1==MSR:       "
.Lt1de:
.Lt2:
    .ascii  "test  2 stw mapped:     "
.Lt2e:
.Lt3:
    .ascii  "test  3 sth/lhz mapped: "
.Lt3e:
.Lt4:
    .ascii  "test  4 stb/lbz mapped: "
.Lt4e:
.Lt5:
    .ascii  "test 5a stw DSI write:   "
.Lt5e:
.Lt5b:
    .ascii  "test 5b DSISR store bit: "
.Lt5be:
.Lt6:
    .ascii  "test  6 sth DSI write:   "
.Lt6e:
.Lt7:
    .ascii  "test  7 stb DSI write:   "
.Lt7e:
.Lt8:
    .ascii  "test  8 lhz DSI read:    "
.Lt8e:
.Lt9:
    .ascii  "test  9 lbz DSI read:    "
.Lt9e:
.Lt10:
    .ascii  "test 10 lwz DSI seq:     "
.Lt10e:
.Lt11:
    .ascii  "test 11 stw+lwz verify:  "
.Lt11e:
.Lt12:
    .ascii  "test 12 lwarx DSI read:  "
.Lt12e:
.Lt13:
    .ascii  "test 13 lwarx+stwcx DSI: "
.Lt13e:
.Lt14:
    .ascii  "test 14 stb x4 DSI:      "
.Lt14e:
.Lsummary:
    .ascii  "\n=== Summary ===\n"
.Lsummarye:
.Lpass:
    .ascii  "All tests passed!\n"
.Lpasse:
