ENTRY(_start)
OUTPUT_FORMAT(elf64-x86-64)

SECTIONS
{
    . = 0x10000;

    .text : {
        *(.text.boot)
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    . = ALIGN(16);
    __bss_start = .;
    .bss : {
        *(COMMON)
        *(.bss*)
    }
    . = ALIGN(16);
    __bss_end = .;

    /DISCARD/ : {
        *(.eh_frame)
        *(.note*)
        *(.comment)
        *(.gnu*)
    }
}
