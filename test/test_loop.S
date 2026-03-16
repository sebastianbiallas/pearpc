/*
 *  Minimal PPC test program for PearPC JIT testing.
 *
 *  Counts from 0 to 9 in r3, then sets r5 = 0x42 as a marker,
 *  then executes sc (system call) which PearPC's PROM handles.
 *
 *  After execution: r3 = 10, r4 = 10, r5 = 0x42
 */

    .text
    .globl _start
_start:
    li      3, 0            /* r3 = 0 (counter) */
    li      4, 10           /* r4 = 10 (limit) */
.Lloop:
    addi    3, 3, 1         /* r3++ */
    cmpwi   0, 3, 10        /* compare r3 to 10 */
    blt     .Lloop          /* branch if r3 < 10 */

    /* r3 should now be 10 */
    li      5, 0x42         /* r5 = 0x42 (success marker) */

    /* halt: loop forever (PearPC has no clean exit via sc for bare metal) */
.Lhalt:
    b       .Lhalt
