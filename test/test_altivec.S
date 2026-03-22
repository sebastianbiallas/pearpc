/*
 *  PearPC AArch64 JIT Test: AltiVec (VMX) Operations
 *
 *  Tests vector arithmetic, load/store, permute, compare, and splat
 *  instructions. Uses custom opcodes for output:
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

    /* Enable AltiVec: set MSR_VEC (bit 25) */
    mfmsr   3
    oris    3, 3, 0x0200    /* set bit 25 = MSR_VEC */
    /* Use rfi trick to set MSR: SRR1=new MSR, SRR0=next insn */
    bl      1f
1:  mflr    4
    addi    4, 4, (2f - 1b) /* SRR0 = address of label 2 */
    mtsrr0  4
    mtsrr1  3
    rfi
2:
    /* Now AltiVec is enabled */

    /*
     * Test 1: vxor - zero a vector register
     */
    PRINT   .Ltest1, (.Ltest1_end - .Ltest1)
    vxor    0, 0, 0         /* v0 = 0 */
    /* Store v0 to stack and check first word */
    addi    5, 1, -16
    rlwinm  5, 5, 0, 0, 27  /* align to 16 bytes */
    stvx    0, 0, 5
    lwz     3, 0(5)
    CHECK   0

    /*
     * Test 2: vspltisw - splat immediate signed word
     */
    PRINT   .Ltest2, (.Ltest2_end - .Ltest2)
    vspltisw 1, 5           /* v1 = {5, 5, 5, 5} */
    addi    5, 1, -16
    rlwinm  5, 5, 0, 0, 27
    stvx    1, 0, 5
    lwz     3, 0(5)
    CHECK   5

    /*
     * Test 3: vspltisw with negative value
     */
    PRINT   .Ltest3, (.Ltest3_end - .Ltest3)
    vspltisw 2, -1          /* v2 = {0xFFFFFFFF, ...} */
    addi    5, 1, -16
    rlwinm  5, 5, 0, 0, 27
    stvx    2, 0, 5
    lwz     3, 0(5)
    CHECK32 0xFFFFFFFF

    /*
     * Test 4: vadduwm - vector add unsigned word modulo
     */
    PRINT   .Ltest4, (.Ltest4_end - .Ltest4)
    vspltisw 1, 7           /* v1 = {7, 7, 7, 7} */
    vspltisw 2, 3           /* v2 = {3, 3, 3, 3} */
    vadduwm 3, 1, 2         /* v3 = {10, 10, 10, 10} */
    addi    5, 1, -16
    rlwinm  5, 5, 0, 0, 27
    stvx    3, 0, 5
    lwz     3, 0(5)
    CHECK   10

    /*
     * Test 5: vsubuwm - vector sub unsigned word modulo
     */
    PRINT   .Ltest5, (.Ltest5_end - .Ltest5)
    vspltisw 1, 10          /* v1 = {10, 10, 10, 10} */
    vspltisw 2, 3           /* v2 = {3, 3, 3, 3} */
    vsubuwm 3, 1, 2         /* v3 = {7, 7, 7, 7} */
    addi    5, 1, -16
    rlwinm  5, 5, 0, 0, 27
    stvx    3, 0, 5
    lwz     3, 0(5)
    CHECK   7

    /*
     * Test 6: vor / vand
     */
    PRINT   .Ltest6, (.Ltest6_end - .Ltest6)
    vspltisw 1, 0xF         /* v1 = {0xF, 0xF, 0xF, 0xF} */
    vspltisw 2, 6           /* v2 = {6, 6, 6, 6} */
    vand    3, 1, 2         /* v3 = {6 & 0xF, ...} = {6, 6, 6, 6} */
    addi    5, 1, -16
    rlwinm  5, 5, 0, 0, 27
    stvx    3, 0, 5
    lwz     3, 0(5)
    CHECK   6

    /*
     * Test 7: vspltisb - splat immediate byte
     */
    PRINT   .Ltest7, (.Ltest7_end - .Ltest7)
    vspltisb 1, 1           /* v1 = {0x01010101, ...} */
    addi    5, 1, -16
    rlwinm  5, 5, 0, 0, 27
    stvx    1, 0, 5
    lwz     3, 0(5)
    CHECK32 0x01010101

    /*
     * Test 8: vaddubm - vector add unsigned byte modulo
     */
    PRINT   .Ltest8, (.Ltest8_end - .Ltest8)
    vspltisb 1, 10          /* v1 = {0x0A0A0A0A, ...} */
    vspltisb 2, 5           /* v2 = {0x05050505, ...} */
    vaddubm 3, 1, 2         /* v3 = {0x0F0F0F0F, ...} */
    addi    5, 1, -16
    rlwinm  5, 5, 0, 0, 27
    stvx    3, 0, 5
    lwz     3, 0(5)
    CHECK32 0x0F0F0F0F

    /*
     * Test 9: vmrghw - vector merge high word
     */
    PRINT   .Ltest9, (.Ltest9_end - .Ltest9)
    vspltisw 1, 1           /* v1 = {1, 1, 1, 1} */
    vspltisw 2, 2           /* v2 = {2, 2, 2, 2} */
    vmrghw  3, 1, 2         /* v3 = {v1[0], v2[0], v1[1], v2[1]} = {1, 2, 1, 2} */
    addi    5, 1, -16
    rlwinm  5, 5, 0, 0, 27
    stvx    3, 0, 5
    lwz     3, 0(5)         /* first word */
    CHECK   1
    PRINT   .Ltest9b, (.Ltest9b_end - .Ltest9b)
    lwz     3, 4(5)         /* second word */
    CHECK   2

    /*
     * Test 10: vcmpequw. - vector compare equal unsigned word (with CR update)
     */
    PRINT   .Ltest10, (.Ltest10_end - .Ltest10)
    vspltisw 1, 7
    vspltisw 2, 7
    vcmpequw. 3, 1, 2       /* all equal => CR6 bit 24 set */
    mfcr    3
    rlwinm  3, 3, 0, 24, 24 /* extract CR6[24] (all-true bit) */
    CHECK32 0x00000080       /* bit 24 = 0x80 in the shifted CR6 field */

    /*
     * Test 11: lvx / stvx round-trip with known data
     */
    PRINT   .Ltest11, (.Ltest11_end - .Ltest11)
    /* Write a known pattern to memory */
    addi    5, 1, -32
    rlwinm  5, 5, 0, 0, 27  /* 16-byte align */
    lis     3, 0xDEAD
    ori     3, 3, 0xBEEF
    stw     3, 0(5)
    lis     3, 0xCAFE
    ori     3, 3, 0xBABE
    stw     3, 4(5)
    lis     3, 0x1234
    ori     3, 3, 0x5678
    stw     3, 8(5)
    lis     3, 0xAAAA
    ori     3, 3, 0xBBBB
    stw     3, 12(5)
    /* Load into vector register */
    lvx     4, 0, 5
    /* Store to a different location */
    addi    6, 5, 16
    stvx    4, 0, 6
    /* Verify */
    lwz     3, 0(6)
    CHECK32 0xDEADBEEF

    /*
     * Test 12: vspltw - vector splat word
     */
    PRINT   .Ltest12, (.Ltest12_end - .Ltest12)
    /* Use the vector from test 11: v4 = {0xDEADBEEF, 0xCAFEBABE, 0x12345678, 0xAAAABBBB} */
    vspltw  5, 4, 1         /* splat word 1 (0xCAFEBABE) to all elements */
    addi    5, 1, -16
    rlwinm  5, 5, 0, 0, 27
    stvx    5, 0, 5
    lwz     3, 0(5)
    CHECK32 0xCAFEBABE

    /*
     * Done
     */
    PRINT   .Ldone, (.Ldone_end - .Ldone)
    mr      3, 30           /* exit code = number of failures */
    .long   0x00333304

/* String constants */
.Lok:   .ascii "OK\n"
.Lfail: .ascii "FAIL\n"
.Ltest1:
    .ascii "Test 1 (vxor zero): "
.Ltest1_end:
.Ltest2:
    .ascii "Test 2 (vspltisw 5): "
.Ltest2_end:
.Ltest3:
    .ascii "Test 3 (vspltisw -1): "
.Ltest3_end:
.Ltest4:
    .ascii "Test 4 (vadduwm): "
.Ltest4_end:
.Ltest5:
    .ascii "Test 5 (vsubuwm): "
.Ltest5_end:
.Ltest6:
    .ascii "Test 6 (vand): "
.Ltest6_end:
.Ltest7:
    .ascii "Test 7 (vspltisb): "
.Ltest7_end:
.Ltest8:
    .ascii "Test 8 (vaddubm): "
.Ltest8_end:
.Ltest9:
    .ascii "Test 9 (vmrghw w0): "
.Ltest9_end:
.Ltest9b:
    .ascii "Test 9b (vmrghw w1): "
.Ltest9b_end:
.Ltest10:
    .ascii "Test 10 (vcmpequw.): "
.Ltest10_end:
.Ltest11:
    .ascii "Test 11 (lvx/stvx): "
.Ltest11_end:
.Ltest12:
    .ascii "Test 12 (vspltw): "
.Ltest12_end:
.Ldone:
    .ascii "\nDone. Failures: "
.Ldone_end:
