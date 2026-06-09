OUTPUT_ARCH(riscv)
ENTRY(worker_main)

SECTIONS
{
    . = 0x80500000;

    .text : {
        *(.text .text.*)
    }

    . = ALIGN(16);
    _etext = .;

    .rodata : {
        *(.rodata .rodata.*)
        *(.srodata .srodata.*)
    }

    . = ALIGN(16);
    _erodata = .;

    .data : {
        *(.data .data.*)
    }

    .bss : {
        *(.bss .bss.*)
        *(COMMON)
        *(.sbss .sbss.*)
    }

    . = ALIGN(16);
    _end = .;
}
