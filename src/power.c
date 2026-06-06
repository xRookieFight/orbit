#include "power.h"
#include "io.h"

void power_off(void)
{
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outb(0xF4, 0x00);
    for (;;)
        hlt();
}

void power_reboot(void)
{
    outb(0x64, 0xFE);
    outb(0xCF9, 0x0E);
    for (;;)
        hlt();
}
