/*
 *  PearPC AArch64 JIT Test: CR Logical Operations
 *
 *  Tests crand, crandc, cror, crorc, crxor, crnand, crnor, creqv.
 *  Also tests common aliases: crclr (crxor bx,bx,bx), crset (creqv bx,bx,bx).
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

/* Check that CR bit 'bit' has value 'expected' (0 or 1).
 * Extracts the bit from CR into r3 and compares. */
.macro CHECK_CRBIT bit, expected
    mfcr    3
    /* Extract bit: PPC bit numbering is MSB=0, so bit N is (31-N) in a 32-bit word */
    rlwinm  3, 3, (\bit + 1), 31, 31   /* rotate left by bit+1, extract bit 31 */
    cmpwi   0, 3, \expected
    beq     88f
    addi    30, 30, 1
    PRINT   .Lfail, 5
    b       77f
88:
    PRINT   .Lok, 3
77:
.endm

/* Macro: set up CR0=GT and CR1=LT for testing.
 * Must be called before each test since CHECK_CRBIT clobbers CR0. */
.macro SETUP_CR
    li      3, 1
    cmpwi   0, 3, 0     /* CR0: GT → bit 0=0(LT), 1=1(GT), 2=0(EQ), 3=0(SO) */
    li      3, -1
    cmpwi   1, 3, 0     /* CR1: LT → bit 4=1(LT), 5=0(GT), 6=0(EQ), 7=0(SO) */
.endm

_start:
    li      30, 0       /* fail counter */

    /* CR0.GT(bit 1) = 1, CR0.LT(bit 0) = 0
     * CR1.LT(bit 4) = 1, CR1.GT(bit 5) = 0 */

    /*
     * Test 1: crand — AND of two set bits
     * crand 8, 1, 4  → bit8 = CR0.GT(1) AND CR1.LT(1) = 1
     */
    SETUP_CR
    PRINT   .Ltest1, (.Ltest1_end - .Ltest1)
    crand   8, 1, 4
    CHECK_CRBIT 8, 1

    /*
     * Test 2: crand — AND of set and clear bits
     * crand 9, 1, 5  → bit9 = CR0.GT(1) AND CR1.GT(0) = 0
     */
    SETUP_CR
    PRINT   .Ltest2, (.Ltest2_end - .Ltest2)
    crand   9, 1, 5
    CHECK_CRBIT 9, 0

    /*
     * Test 3: cror — OR of set and clear bits
     * cror 10, 0, 4  → bit10 = CR0.LT(0) OR CR1.LT(1) = 1
     */
    SETUP_CR
    PRINT   .Ltest3, (.Ltest3_end - .Ltest3)
    cror    10, 0, 4
    CHECK_CRBIT 10, 1

    /*
     * Test 4: cror — OR of two clear bits
     * cror 11, 0, 5  → bit11 = CR0.LT(0) OR CR1.GT(0) = 0
     */
    SETUP_CR
    PRINT   .Ltest4, (.Ltest4_end - .Ltest4)
    cror    11, 0, 5
    CHECK_CRBIT 11, 0

    /*
     * Test 5: crxor — XOR of two set bits
     * crxor 12, 1, 4  → bit12 = CR0.GT(1) XOR CR1.LT(1) = 0
     */
    SETUP_CR
    PRINT   .Ltest5, (.Ltest5_end - .Ltest5)
    crxor   12, 1, 4
    CHECK_CRBIT 12, 0

    /*
     * Test 6: crxor — XOR of set and clear
     * crxor 13, 1, 5  → bit13 = CR0.GT(1) XOR CR1.GT(0) = 1
     */
    SETUP_CR
    PRINT   .Ltest6, (.Ltest6_end - .Ltest6)
    crxor   13, 1, 5
    CHECK_CRBIT 13, 1

    /*
     * Test 7: crclr (crxor bx, bx, bx) — clear a bit
     * crclr 14  → bit14 = 0
     */
    SETUP_CR
    PRINT   .Ltest7, (.Ltest7_end - .Ltest7)
    crclr   14
    CHECK_CRBIT 14, 0

    /*
     * Test 8: creqv (same bit = crset) — set a bit
     * creqv 15, 15, 15  → bit15 = 1
     */
    SETUP_CR
    PRINT   .Ltest8, (.Ltest8_end - .Ltest8)
    creqv   15, 15, 15
    CHECK_CRBIT 15, 1

    /*
     * Test 9: crandc — AND with complement
     * crandc 16, 1, 5  → bit16 = CR0.GT(1) AND NOT CR1.GT(0) = 1 AND 1 = 1
     */
    SETUP_CR
    PRINT   .Ltest9, (.Ltest9_end - .Ltest9)
    crandc  16, 1, 5
    CHECK_CRBIT 16, 1

    /*
     * Test 10: crandc — AND with complement (result 0)
     * crandc 17, 1, 4  → bit17 = CR0.GT(1) AND NOT CR1.LT(1) = 1 AND 0 = 0
     */
    SETUP_CR
    PRINT   .Ltest10, (.Ltest10_end - .Ltest10)
    crandc  17, 1, 4
    CHECK_CRBIT 17, 0

    /*
     * Test 11: crnand — NAND of two set bits
     * crnand 18, 1, 4  → bit18 = NOT(CR0.GT(1) AND CR1.LT(1)) = NOT(1) = 0
     */
    SETUP_CR
    PRINT   .Ltest11, (.Ltest11_end - .Ltest11)
    crnand  18, 1, 4
    CHECK_CRBIT 18, 0

    /*
     * Test 12: crnand — NAND with a clear bit
     * crnand 19, 0, 4  → bit19 = NOT(CR0.LT(0) AND CR1.LT(1)) = NOT(0) = 1
     */
    SETUP_CR
    PRINT   .Ltest12, (.Ltest12_end - .Ltest12)
    crnand  19, 0, 4
    CHECK_CRBIT 19, 1

    /*
     * Test 13: crnor — NOR of two clear bits
     * crnor 20, 0, 5  → bit20 = NOT(CR0.LT(0) OR CR1.GT(0)) = NOT(0) = 1
     */
    SETUP_CR
    PRINT   .Ltest13, (.Ltest13_end - .Ltest13)
    crnor   20, 0, 5
    CHECK_CRBIT 20, 1

    /*
     * Test 14: crnor — NOR with a set bit
     * crnor 21, 1, 5  → bit21 = NOT(CR0.GT(1) OR CR1.GT(0)) = NOT(1) = 0
     */
    SETUP_CR
    PRINT   .Ltest14, (.Ltest14_end - .Ltest14)
    crnor   21, 1, 5
    CHECK_CRBIT 21, 0

    /*
     * Test 15: crorc — OR with complement
     * crorc 22, 0, 4  → bit22 = CR0.LT(0) OR NOT CR1.LT(1) = 0 OR 0 = 0
     */
    SETUP_CR
    PRINT   .Ltest15, (.Ltest15_end - .Ltest15)
    crorc   22, 0, 4
    CHECK_CRBIT 22, 0

    /*
     * Test 16: crorc — OR with complement (result 1)
     * crorc 23, 0, 5  → bit23 = CR0.LT(0) OR NOT CR1.GT(0) = 0 OR 1 = 1
     */
    SETUP_CR
    PRINT   .Ltest16, (.Ltest16_end - .Ltest16)
    crorc   23, 0, 5
    CHECK_CRBIT 23, 1

    /*
     * Test 17: creqv — XNOR of different bits
     * creqv 24, 1, 5  → bit24 = NOT(CR0.GT(1) XOR CR1.GT(0)) = NOT(1) = 0
     */
    SETUP_CR
    PRINT   .Ltest17, (.Ltest17_end - .Ltest17)
    creqv   24, 1, 5
    CHECK_CRBIT 24, 0

    /*
     * Test 18: creqv — XNOR of same-value bits
     * creqv 25, 1, 4  → bit25 = NOT(CR0.GT(1) XOR CR1.LT(1)) = NOT(0) = 1
     */
    SETUP_CR
    PRINT   .Ltest18, (.Ltest18_end - .Ltest18)
    creqv   25, 1, 4
    CHECK_CRBIT 25, 1

    /* === Summary === */
    PRINT   .Lsummary, (.Lsummary_end - .Lsummary)
    cmpwi   0, 30, 0
    beq     .Lall_passed
    mr      3, 30
    .long   0x00333304      /* exit with fail count */

.Lall_passed:
    PRINT   .Lall_pass, (.Lall_pass_end - .Lall_pass)
    li      3, 0
    .long   0x00333304      /* exit 0 */

/* String constants */
.Ltest1:
    .ascii  "test 1  crand(1,1):    "
.Ltest1_end:
.Ltest2:
    .ascii  "test 2  crand(1,0):    "
.Ltest2_end:
.Ltest3:
    .ascii  "test 3  cror(0,1):     "
.Ltest3_end:
.Ltest4:
    .ascii  "test 4  cror(0,0):     "
.Ltest4_end:
.Ltest5:
    .ascii  "test 5  crxor(1,1):    "
.Ltest5_end:
.Ltest6:
    .ascii  "test 6  crxor(1,0):    "
.Ltest6_end:
.Ltest7:
    .ascii  "test 7  crclr:         "
.Ltest7_end:
.Ltest8:
    .ascii  "test 8  crset:         "
.Ltest8_end:
.Ltest9:
    .ascii  "test 9  crandc(1,~0):  "
.Ltest9_end:
.Ltest10:
    .ascii  "test 10 crandc(1,~1):  "
.Ltest10_end:
.Ltest11:
    .ascii  "test 11 crnand(1,1):   "
.Ltest11_end:
.Ltest12:
    .ascii  "test 12 crnand(0,1):   "
.Ltest12_end:
.Ltest13:
    .ascii  "test 13 crnor(0,0):    "
.Ltest13_end:
.Ltest14:
    .ascii  "test 14 crnor(1,0):    "
.Ltest14_end:
.Ltest15:
    .ascii  "test 15 crorc(0,~1):   "
.Ltest15_end:
.Ltest16:
    .ascii  "test 16 crorc(0,~0):   "
.Ltest16_end:
.Ltest17:
    .ascii  "test 17 creqv(1,0):    "
.Ltest17_end:
.Ltest18:
    .ascii  "test 18 creqv(1,1):    "
.Ltest18_end:
.Lsummary:
    .ascii  "\n=== Summary ===\n"
.Lsummary_end:
.Lall_pass:
    .ascii  "All tests passed!\n"
.Lall_pass_end:
.Lok:
    .ascii  "OK\n"
.Lfail:
    .ascii  "FAIL\n"
