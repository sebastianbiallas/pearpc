/*
 *  Minimal PPC test program for PearPC AArch64 JIT testing.
 *
 *  Uses custom PearPC opcodes for I/O:
 *    .long 0x00333303  - print string (r3=addr, r4=length)
 *    .long 0x00333304  - exit (r3=exit code)
 *
 *  Expected output:
 *    Hello from PPC!
 *    loop: 1
 *    loop: 2
 *    ...
 *    loop: 10
 *    PASS
 */

    .text
    .globl _start

_start:
    /* Print "Hello from PPC!\n" */
    bl      .Lget_pc
.Lget_pc:
    mflr    10                  /* r10 = address of .Lget_pc */
    addi    3, 10, (.Lhello - .Lget_pc)
    li      4, (.Lhello_end - .Lhello)
    .long   0x00333303          /* print string */

    /* Count loop: r3 counts 1 to 10 */
    li      20, 0               /* r20 = counter */
    li      21, 10              /* r21 = limit */

.Lloop:
    addi    20, 20, 1           /* counter++ */

    /* Print "loop: " */
    bl      .Lget_pc2
.Lget_pc2:
    mflr    10
    addi    3, 10, (.Lloop_prefix - .Lget_pc2)
    li      4, (.Lloop_prefix_end - .Lloop_prefix)
    .long   0x00333303

    /* Print the digit (1-9 as ASCII, 10 as "10") */
    cmpwi   0, 20, 10
    beq     .Lprint_ten
    addi    3, 20, 0x30         /* ASCII '0' + counter */
    /* store byte to stack and print it */
    stb     3, -1(1)
    addi    3, 1, -1
    li      4, 1
    .long   0x00333303
    b       .Lprint_newline

.Lprint_ten:
    bl      .Lget_pc3
.Lget_pc3:
    mflr    10
    addi    3, 10, (.Lten_str - .Lget_pc3)
    li      4, 2
    .long   0x00333303

.Lprint_newline:
    bl      .Lget_pc4
.Lget_pc4:
    mflr    10
    addi    3, 10, (.Lnewline - .Lget_pc4)
    li      4, 1
    .long   0x00333303

    cmpw    0, 20, 21
    blt     .Lloop

    /* Print "PASS\n" */
    bl      .Lget_pc5
.Lget_pc5:
    mflr    10
    addi    3, 10, (.Lpass - .Lget_pc5)
    li      4, (.Lpass_end - .Lpass)
    .long   0x00333303

    /* Exit with code 0 */
    li      3, 0
    .long   0x00333304

/* String data embedded in .text (accessible via PC-relative addressing) */
.Lhello:
    .ascii  "Hello from PPC!\n"
.Lhello_end:

.Lloop_prefix:
    .ascii  "loop: "
.Lloop_prefix_end:

.Lten_str:
    .ascii  "10"

.Lnewline:
    .ascii  "\n"

.Lpass:
    .ascii  "PASS\n"
.Lpass_end:
