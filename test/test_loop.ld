/* Linker script: load at physical address 0x100000 */
ENTRY(_start)
SECTIONS
{
    . = 0x100000;
    .text : { *(.text) }
}
