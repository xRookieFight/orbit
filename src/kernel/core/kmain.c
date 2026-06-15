#include "orbit.h"
#include "console.h"
#include "serial.h"
#include "idt.h"
#include "isr.h"
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
#include "gui.h"

static void warmup_serial(void)
{
#if ORBIT_DEBUG_SERIAL
    for (volatile int i = 0; i < 400000; i++)
        ;
    serial_putc('\n');
#endif
}

static void boot_log(const char* msg)
{
#if ORBIT_DEBUG_SERIAL
    serial_write(msg);
#else
    (void)msg;
#endif
}

static void start_shell(void)
{
    console_set_color(COL_LIGHT_CYAN, COL_BLACK);
    console_printf("  %s\n", ORBIT_BANNER);
    console_set_color(COL_LIGHT_GREY, COL_BLACK);
    console_printf("  %s\n\n", ORBIT_TAGLINE);

    shell_run();
}

static void kernel_init(void)
{
    serial_init();
    warmup_serial();
    boot_log("[boot] orbit " ORBIT_VERSION " x86_64\n");

    idt_init();
    isr_install();
    pit_init(100);
    keyboard_init();

    __asm__ volatile("sti");

    heap_init();
    fb_init();
    boot_log("[boot] framebuffer\n");
}

static void os_init(void)
{
    fs_init();
    user_init();
    proc_init();
    api_init();
    app_registry_init();
    builtins_register();
}

static void network_init(void)
{
    net_init();
    udp_init();
    dhcp_init();
    if (rtl8139_init() == 0) {
        boot_log("[boot] rtl8139 nic detected\n");
        if (dhcp_configure() == 0)
            boot_log("[boot] dhcp configured\n");
        else
            boot_log("[boot] dhcp failed\n");
    } else {
        boot_log("[boot] no network device\n");
    }
    netcmd_register();
}

void kmain(void)
{
    kernel_init();
    gui_show_splash();
    os_init();
    network_init();
    log_init();
    gui_init();

    klog("INFO", "Orbit %s boot complete", ORBIT_VERSION);

    while (!gui_shell_requested()) {
        gui_pump();
        __asm__ volatile("hlt");
    }

    start_shell();

    for (;;)
        __asm__ volatile("hlt");
}
