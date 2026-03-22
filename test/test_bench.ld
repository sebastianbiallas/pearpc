/* Linker script for C test programs: .text, .rodata, .data, .bss */
ENTRY(_start)
SECTIONS
{
    . = 0x100000;
    .text   : { *(.text .text.*) }
    .rodata : { *(.rodata .rodata.*) }
    .data   : { *(.data .data.*) }
    .bss    : { *(.bss .bss.* COMMON) }
    _end = .;
}
