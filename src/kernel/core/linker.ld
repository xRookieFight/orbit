ENTRY(_start)
OUTPUT_FORMAT(elf64-x86-64)

PHDRS
{
    text PT_LOAD FLAGS(5);   /* R + X */
    rodata PT_LOAD FLAGS(4); /* R */
    data PT_LOAD FLAGS(6);   /* R + W */
}

SECTIONS
{
    . = 0x10000;

    .text : {
        *(.text.boot)
        *(.text*)
    } :text

    .rodata : {
        *(.rodata*)
        . = ALIGN(8);
        __gui_apps_start = .;
        KEEP(*(gui_apps))
        __gui_apps_end = .;
    } :rodata

    .data : {
        *(.data*)
    } :data

    . = ALIGN(16);
    __bss_start = .;
    .bss : {
        *(COMMON)
        *(.bss*)
    } :data
    . = ALIGN(16);
    __bss_end = .;

    /DISCARD/ : {
        *(.eh_frame)
        *(.note*)
        *(.comment)
        *(.gnu*)
    }
}
