/*
 *  Minimal C runtime startup for PearPC freestanding test programs.
 *  ELF loader already sets r1 (stack pointer).
 */
    .text
    .globl _start
_start:
    stwu    1, -16(1)       /* minimal stack frame for ABI */
    bl      main
    .long   0x00333304      /* exit(r3) */
