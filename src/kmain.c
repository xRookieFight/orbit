#include "orbit.h"
#include "console.h"
#include "serial.h"
#include "vga.h"
#include "io.h"
#include "gdt.h"
#include "idt.h"
#include "isr.h"
#include "pic.h"
#include "pit.h"
#include "keyboard.h"
#include "heap.h"
#include "fs.h"
#include "user.h"
#include "proc.h"
#include "api.h"
#include "app.h"
#include "builtins.h"
#include "shell.h"
#include "log.h"

static void warmup_serial(void)
{
    for (volatile int i = 0; i < 400000; i++)
        ;
    serial_putc('\n');
}

void kmain(void)
{
    serial_init();
    console_init();
    warmup_serial();

    console_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    console_printf("\n  %s\n", ORBIT_BANNER);
    console_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    console_printf("  CLI modular operating system   build %s\n\n", ORBIT_BUILD);

    console_printf("[boot] gdt\n");
    gdt_init();
    console_printf("[boot] idt + isr\n");
    idt_init();
    isr_install();
    console_printf("[boot] timer 100hz\n");
    pit_init(100);
    console_printf("[boot] keyboard\n");
    keyboard_init();
    console_printf("[boot] heap\n");
    heap_init();

    __asm__ volatile("sti");

    console_printf("[boot] filesystem\n");
    fs_init();
    console_printf("[boot] users\n");
    user_init();
    console_printf("[boot] processes\n");
    proc_init();
    console_printf("[boot] application api\n");
    api_init();
    app_registry_init();
    builtins_register();
    log_init();

    klog("INFO", "Orbit %s boot complete", ORBIT_VERSION);
    console_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    console_printf("[boot] ready\n\n");
    console_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    shell_run();

    for (;;)
        __asm__ volatile("hlt");
}
