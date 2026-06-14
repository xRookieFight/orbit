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
        . = ALIGN(8);
        __gui_apps_start = .;
        KEEP(*(gui_apps))
        __gui_apps_end = .;
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
