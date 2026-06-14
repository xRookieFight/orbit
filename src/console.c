#include "console.h"
#include "term.h"
#include "serial.h"
#include "desktop.h"
#include "fmt.h"
#include "io.h"

void console_init(void)
{
}

void console_clear(void)
{
    if (desktop_active())
        term_clear();
}

void console_set_color(uint8_t fg, uint8_t bg)
{
    term_set_color(fg, bg);
}

void console_putc(char c)
{
    if (desktop_active())
        term_putc(c);
    serial_putc(c);
}

void console_write(const char* s)
{
    while (*s)
        console_putc(*s++);
}

static void console_sink(char c, void* ctx)
{
    (void)ctx;
    console_putc(c);
}

void console_vprintf(const char* fmt, va_list args)
{
    fmt_vformat(console_sink, NULL, fmt, args);
}

void console_printf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fmt_vformat(console_sink, NULL, fmt, args);
    va_end(args);
}

char console_getc(void)
{
    for (;;) {
        int c = term_read_key();
        if (c >= 0)
            return (char)c;
        if (!desktop_active() && serial_has_input())
            return serial_getc();
        desktop_pump();
        __asm__ volatile("hlt");
    }
}

void console_readline(char* buffer, uint32_t size)
{
    uint32_t len = 0;
    for (;;) {
        char c = console_getc();
        if (c == '\r' || c == '\n') {
            console_putc('\n');
            break;
        }
        if (c == '\b' || c == 127) {
            if (len > 0) {
                len--;
                console_putc('\b');
                console_putc(' ');
                console_putc('\b');
            }
            continue;
        }
        if (c >= 32 && c < 127 && len + 1 < size) {
            buffer[len++] = c;
            console_putc(c);
        }
    }
    buffer[len] = '\0';
}
