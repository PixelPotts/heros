OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 128M
}

SECTIONS
{
    . = 0x80000000;

    .text : {
        *(.text.init)
        *(.text .text.*)
    } > RAM

    .rodata : {
        . = ALIGN(4);
        *(.rodata .rodata.*)
    } > RAM

    .data : {
        . = ALIGN(4);
        *(.data .data.*)
    } > RAM

    .bss : {
        . = ALIGN(4);
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > RAM

    . = ALIGN(4096);
    _heap_start = .;

    /* Reserve heap up to stack region */
    _heap_end = 0x87E00000;

    /* Stack lives at top of RAM */
    _stack_top = 0x87FF0000;
}
