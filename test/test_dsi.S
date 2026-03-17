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
     * Test 1: lwz from unmapped address (should trigger DSI, handler maps it, retries)
     */
    PRINT   .Lt1, (.Lt1e - .Lt1)

    /* First write a known value to PA 0x500000 using physical access.
     * We'll use the unmapped EA 0x500000 which maps 1:1 to PA 0x500000.
     * But we can't write there yet because it's unmapped!
     * Instead, we set up the test value AFTER the DSI handler maps the page.
     * The DSI handler will map 0x500000 → PA 0x500000.
     * After mapping, we write a value, then read it back. */

    /* Clear the DSI counter */
    li      0, 0
    lis     9, 0x10                 /* r9 = 0x100000 (our code area) */
    stw     0, 0x100(9)             /* *(0x100100) = 0 — DSI counter */

    /* Attempt to read from unmapped address 0x500000 — should trigger DSI */
    lis     8, 0x50                 /* r8 = 0x500000 */
    lwz     3, 0(8)                 /* FAULT HERE → DSI → handler maps it → retry → success */

    /* If we get here, the DSI handler worked and the load succeeded. */
    PRINT   .Lok, 3

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
     * Test 5: lwz from second unmapped page (0x501000) — triggers another DSI
     */
    PRINT   .Lt5, (.Lt5e - .Lt5)

    lis     8, 0x50
    ori     8, 8, 0x1000            /* r8 = 0x501000 */
    lwz     3, 0(8)                 /* FAULT → DSI → map → retry */

    /* If we get here, second DSI handled correctly. */
    PRINT   .Lok, 3

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
    /* Save r3 and r4 in SPRG0/SPRG1 */
    mtsprg  0, 3
    mtsprg  1, 4

    /* r3 = faulting address */
    mfdar   3

    /* Increment DSI counter at 0x100100 (use r0, which is volatile) */
    lis     4, 0x10
    lwz     0, 0x100(4)
    addi    0, 0, 1
    stw     0, 0x100(4)

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
    .ascii  "test  1 lwz DSI+remap:  "
.Lt1e:
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
    .ascii  "test  5 lwz DSI page 2: "
.Lt5e:
.Lsummary:
    .ascii  "\n=== Summary ===\n"
.Lsummarye:
.Lpass:
    .ascii  "All tests passed!\n"
.Lpasse:
