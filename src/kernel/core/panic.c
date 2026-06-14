#include "panic.h"
#include "console.h"

void panic_exception(const char* message, uint64_t int_no, uint64_t err_code, uint64_t rip)
{
    console_set_color(COL_LIGHT_RED, COL_BLACK);
    console_printf("\n*** Orbit kernel panic ***\n");
    console_printf("Exception: %s\n", message);
    console_printf("int=%d err=%d rip=%p\n", (int)int_no, (int)err_code, (void*)rip);
    console_set_color(COL_LIGHT_GREY, COL_BLACK);
    for (;;)
        __asm__ volatile("cli; hlt");
}
