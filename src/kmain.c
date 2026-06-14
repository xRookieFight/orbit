#include "orbit.h"
#include "console.h"
#include "serial.h"
#include "io.h"
#include "idt.h"
#include "isr.h"
#include "pic.h"
#include "pit.h"
#include "keyboard.h"
#include "heap.h"
#include "fb.h"
#include "fs.h"
#include "user.h"
#include "proc.h"
#include "api.h"
#include "app.h"
#include "builtins.h"
#include "net.h"
#include "udp.h"
#include "dhcp.h"
#include "rtl8139.h"
#include "netcmd.h"
#include "shell.h"
#include "log.h"
#include "splash.h"
#include "desktop.h"
#include "palette.h"

static void warmup_serial(void)
{
    for (volatile int i = 0; i < 400000; i++)
        ;
    serial_putc('\n');
}

void kmain(void)
{
    serial_init();
    warmup_serial();
    serial_write("[boot] orbit " ORBIT_VERSION " x86_64\n");

    idt_init();
    isr_install();
    pit_init(100);
    keyboard_init();

    __asm__ volatile("sti");

    heap_init();
    fb_init();
    serial_write("[boot] framebuffer\n");

    splash_show();

    fs_init();
    user_init();
    proc_init();
    api_init();
    app_registry_init();
    builtins_register();

    net_init();
    udp_init();
    dhcp_init();
    if (rtl8139_init() == 0) {
        serial_write("[boot] rtl8139 nic detected\n");
        if (dhcp_configure() == 0)
            serial_write("[boot] dhcp configured\n");
        else
            serial_write("[boot] dhcp failed\n");
    } else {
        serial_write("[boot] no network device\n");
    }
    netcmd_register();

    log_init();

    desktop_init();
    serial_write("[boot] desktop ready\n");

    klog("INFO", "Orbit %s boot complete", ORBIT_VERSION);

    console_set_color(COL_LIGHT_CYAN, COL_BLACK);
    console_printf("  %s\n", ORBIT_BANNER);
    console_set_color(COL_LIGHT_GREY, COL_BLACK);
    console_printf("  %s\n\n", ORBIT_TAGLINE);

    shell_run();

    for (;;)
        __asm__ volatile("hlt");
}
