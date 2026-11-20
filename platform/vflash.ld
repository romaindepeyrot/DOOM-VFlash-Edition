/* SPDX-License-Identifier: GPL-2.0-or-later */
ENTRY(_start)

MEMORY {
    RAM (rwx) : ORIGIN = 0x10004000, LENGTH = 0x00ffbff0
}

PHDRS {
    program PT_LOAD FLAGS(7);
}

SECTIONS {
    .text : {
        KEEP(*(.text.start))
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(8);
    } > RAM :program

    .ARM.exidx : { *(.ARM.exidx*) } > RAM :program
    .ARM.extab : { *(.ARM.extab*) } > RAM :program
    .data : { *(.data .data.*) } > RAM :program

    .bss (NOLOAD) : {
        . = ALIGN(8);
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(8);
        __bss_end = .;
    } > RAM :program

    .stack (NOLOAD) : {
        . = ALIGN(8);
        . += 128K;
        __stack_top = .;
        __heap_start = .;
    } > RAM :program
}
