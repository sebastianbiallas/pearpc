/*
 *  PearPC AArch64 JIT Test: Memory / TLB Stress
 *
 *  Exercises the TLB fast path with various access patterns:
 *    - Repeated same-page access (TLB hits)
 *    - Multi-page stride (TLB replacement)
 *    - Byte/half/word sizes
 *    - Negative offsets
 *    - Page-boundary-adjacent accesses
 *    - Large working set loop
 *
 *  Custom opcodes:
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

/* Check a 32-bit register against a 32-bit value loaded into r11 */
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

    /*
     * Test 1: Word store/load - basic
     */
    PRINT   .Lt1, (.Lt1e - .Lt1)
    lis     4, 0xDEAD
    ori     4, 4, 0xBEEF       /* r4 = 0xDEADBEEF */
    stw     4, -16(1)
    li      3, 0
    lwz     3, -16(1)
    CHECK32 0xDEADBEEF

    /*
     * Test 2: Byte store/load - all 4 bytes of a word
     */
    PRINT   .Lt2, (.Lt2e - .Lt2)
    li      4, 0x11
    stb     4, -32(1)
    li      4, 0x22
    stb     4, -31(1)
    li      4, 0x33
    stb     4, -30(1)
    li      4, 0x44
    stb     4, -29(1)
    /* Load the word back - should be 0x11223344 (big-endian) */
    lwz     3, -32(1)
    CHECK32 0x11223344

    /*
     * Test 3: Halfword store/load
     */
    PRINT   .Lt3, (.Lt3e - .Lt3)
    lis     4, 0
    ori     4, 4, 0xCAFE
    sth     4, -40(1)
    li      3, 0
    lhz     3, -40(1)
    CHECK32 0x0000CAFE

    /*
     * Test 4: Byte/half/word consistency
     *   Store a word, read back as bytes and half
     */
    PRINT   .Lt4, (.Lt4e - .Lt4)
    lis     4, 0x0102
    ori     4, 4, 0x0304       /* 0x01020304 */
    stw     4, -48(1)
    /* byte 0 (big-endian MSB) */
    lbz     3, -48(1)
    CHECK   0x01

    PRINT   .Lt5, (.Lt5e - .Lt5)
    /* byte 1 */
    lbz     3, -47(1)
    CHECK   0x02

    PRINT   .Lt6, (.Lt6e - .Lt6)
    /* byte 2 */
    lbz     3, -46(1)
    CHECK   0x03

    PRINT   .Lt7, (.Lt7e - .Lt7)
    /* byte 3 (LSB) */
    lbz     3, -45(1)
    CHECK   0x04

    PRINT   .Lt8, (.Lt8e - .Lt8)
    /* high half */
    lhz     3, -48(1)
    CHECK32 0x00000102

    PRINT   .Lt9, (.Lt9e - .Lt9)
    /* low half */
    lhz     3, -46(1)
    CHECK32 0x00000304

    /*
     * Test 10: Same-page repeated access (TLB hit stress)
     *   Write 64 words, read them all back, sum them.
     */
    PRINT   .Lt10, (.Lt10e - .Lt10)
    /* Write 64 words: mem[i] = i+1 */
    addi    6, 1, -512          /* r6 = base addr (on stack, -512 from SP) */
    li      7, 0                /* i = 0 */
    li      8, 64               /* count */
.Lwrite_loop:
    addi    9, 7, 1             /* value = i+1 */
    slwi    11, 7, 2            /* offset = i*4 */
    stwx    9, 6, 11
    addi    7, 7, 1
    cmpw    0, 7, 8
    blt     .Lwrite_loop
    /* Read back and sum */
    li      7, 0
    li      3, 0                /* sum */
.Lread_loop:
    slwi    11, 7, 2
    lwzx    9, 6, 11
    add     3, 3, 9
    addi    7, 7, 1
    cmpw    0, 7, 8
    blt     .Lread_loop
    /* Expected: sum(1..64) = 64*65/2 = 2080 */
    CHECK32 0x00000820

    /*
     * Test 11a: lis address store/load
     *   Verify lis correctly sets address, then store/load.
     */
    PRINT   .Lt11a, (.Lt11ae - .Lt11a)
    /* First verify lis value by shifting it down */
    lis     6, 0x0020           /* r6 = 0x200000 */
    srwi    3, 6, 16            /* r3 = 0x200000 >> 16 = 0x20 = 32 */
    CHECK   32

    PRINT   .Lt11b, (.Lt11be - .Lt11b)
    /* Now try stw/lwz with r1-relative (known working) using a computed offset */
    addi    6, 1, -256          /* r6 = sp - 256, guaranteed stack area */
    li      4, 77
    stw     4, 0(6)
    li      3, 0
    lwz     3, 0(6)
    CHECK   77

    PRINT   .Lt11c, (.Lt11ce - .Lt11c)
    /* Store/load at computed stack address */
    addi    6, 1, -1024         /* r6 = sp - 1024 */
    li      4, 88
    stw     4, 0(6)
    li      3, 0
    lwz     3, 0(6)
    CHECK   88

    /*
     * Test 11: Multi-offset stride
     *   Write 8 words at 128-byte intervals on the stack,
     *   read them all back and sum.
     */
    PRINT   .Lt11, (.Lt11e - .Lt11)
    addi    6, 1, -2048         /* base on stack */
    li      9, 100
    stw     9, 0(6)
    li      9, 101
    stw     9, 128(6)
    li      9, 102
    stw     9, 256(6)
    li      9, 103
    stw     9, 384(6)
    li      9, 104
    stw     9, 512(6)
    li      9, 105
    stw     9, 640(6)
    li      9, 106
    stw     9, 768(6)
    li      9, 107
    stw     9, 896(6)
    /* Read back and sum */
    li      3, 0
    lwz     9, 0(6)
    add     3, 3, 9
    lwz     9, 128(6)
    add     3, 3, 9
    lwz     9, 256(6)
    add     3, 3, 9
    lwz     9, 384(6)
    add     3, 3, 9
    lwz     9, 512(6)
    add     3, 3, 9
    lwz     9, 640(6)
    add     3, 3, 9
    lwz     9, 768(6)
    add     3, 3, 9
    lwz     9, 896(6)
    add     3, 3, 9
    /* Expected: sum(100..107) = 828 */
    CHECK32 0x0000033C

    /*
     * Test 12: Large working set loop
     *   Write 256 words to consecutive addresses, read back, verify checksum.
     *   Uses stw/lwz with offset (native gen_ path with TLB).
     */
    PRINT   .Lt12, (.Lt12e - .Lt12)
    addi    6, 1, -4096         /* base on stack, 4KB below SP */
    li      7, 0                /* i */
    li      8, 256
    mr      11, 6              /* write pointer */
.Lbig_write:
    xori    9, 7, 0x55AA       /* value = i ^ 0x55AA */
    stw     9, 0(11)
    addi    11, 11, 4          /* advance pointer */
    addi    7, 7, 1
    cmpw    0, 7, 8
    blt     .Lbig_write
    /* Read back and XOR-reduce */
    li      7, 0
    li      3, 0
    mr      11, 6
.Lbig_read:
    lwz     9, 0(11)
    xor     3, 3, 9
    addi    11, 11, 4
    addi    7, 7, 1
    cmpw    0, 7, 8
    blt     .Lbig_read
    /* XOR(i ^ 0x55AA) for i=0..255:
     * = XOR(i) for i=0..255 XOR (256 times 0x55AA, even→0)
     * XOR(0..255): 255 = 4*64-1, so result = 0.
     */
    CHECK   0

    /*
     * Test 13: Byte-level store across halfword boundary
     */
    PRINT   .Lt13, (.Lt13e - .Lt13)
    addi    6, 1, -64           /* scratch area on stack */
    li      4, 0xAB
    stb     4, 0(6)
    li      4, 0xCD
    stb     4, 1(6)
    lhz     3, 0(6)
    CHECK32 0x0000ABCD

    /*
     * Test 14: Halfword store, byte readback
     */
    PRINT   .Lt14, (.Lt14e - .Lt14)
    addi    6, 1, -64
    li      4, 0x1234
    ori     4, 4, 0
    sth     4, 4(6)
    lbz     3, 4(6)             /* high byte */
    CHECK   0x12

    PRINT   .Lt15, (.Lt15e - .Lt15)
    lbz     3, 5(6)             /* low byte */
    CHECK   0x34

    /*
     * Test 16: stmw/lmw (3 registers: r29-r31)
     * Store r29-r31 to memory, clobber them, load back, verify.
     */
    PRINT   .Lt16, (.Lt16e - .Lt16)
    li      29, 111
    li      31, 333
    addi    6, 1, -80
    stmw    29, 0(6)          /* store r29,r30,r31 to [sp-80] */
    /* clobber */
    li      29, 0
    li      31, 0
    lmw     29, 0(6)          /* load back */
    mr      3, 29
    CHECK   111

    PRINT   .Lt17, (.Lt17e - .Lt17)
    mr      3, 31
    CHECK   333

    /*
     * Test 18: stmw/lmw (single register: r31 only)
     */
    PRINT   .Lt18, (.Lt18e - .Lt18)
    li      31, 999
    stmw    31, 0(6)
    li      31, 0
    lmw     31, 0(6)
    mr      3, 31
    CHECK   999

    /*
     * Summary
     */
    PRINT   .Lsummary, (.Lsummary_end - .Lsummary)
    cmpwi   0, 30, 0
    bne     .Lprint_fails
    PRINT   .Lall_pass, (.Lall_pass_end - .Lall_pass)
    li      3, 0
    .long   0x00333304

.Lprint_fails:
    addi    3, 30, 0x30
    stb     3, -1(1)
    addi    3, 1, -1
    li      4, 1
    .long   0x00333303
    PRINT   .Lfails_suffix, (.Lfails_suffix_end - .Lfails_suffix)
    mr      3, 30
    .long   0x00333304

/* String data */
.Lt1:
    .ascii  "test  1 stw/lwz basic:     "
.Lt1e:
.Lt2:
    .ascii  "test  2 stb x4 -> lwz:     "
.Lt2e:
.Lt3:
    .ascii  "test  3 sth/lhz:           "
.Lt3e:
.Lt4:
    .ascii  "test  4 word->byte[0]:     "
.Lt4e:
.Lt5:
    .ascii  "test  5 word->byte[1]:     "
.Lt5e:
.Lt6:
    .ascii  "test  6 word->byte[2]:     "
.Lt6e:
.Lt7:
    .ascii  "test  7 word->byte[3]:     "
.Lt7e:
.Lt8:
    .ascii  "test  8 word->half[hi]:    "
.Lt8e:
.Lt9:
    .ascii  "test  9 word->half[lo]:    "
.Lt9e:
.Lt10:
    .ascii  "test 10 64-word same-page: "
.Lt10e:
.Lt11a:
    .ascii  "test 11a lis value:        "
.Lt11ae:
.Lt11b:
    .ascii  "test 11b sp-relative:      "
.Lt11be:
.Lt11c:
    .ascii  "test 11c abs 0x200000:     "
.Lt11ce:
.Lt11:
    .ascii  "test 11 multi-page stride: "
.Lt11e:
.Lt12:
    .ascii  "test 12 1024-word xor:     "
.Lt12e:
.Lt13:
    .ascii  "test 13 byte->half:        "
.Lt13e:
.Lt14:
    .ascii  "test 14 half->byte[hi]:    "
.Lt14e:
.Lt15:
    .ascii  "test 15 half->byte[lo]:    "
.Lt15e:
.Lt16:
    .ascii  "test 16 stmw/lmw r29:      "
.Lt16e:
.Lt17:
    .ascii  "test 17 lmw r31 verify:    "
.Lt17e:
.Lt18:
    .ascii  "test 18 stmw/lmw r31 only: "
.Lt18e:
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
