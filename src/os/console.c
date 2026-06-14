#include "console.h"
#include "serial.h"
#include "fmt.h"

static const console_backend_t* backend;

void console_init(void)
{
}

void console_bind_backend(const console_backend_t* next)
{
    backend = next;
}

void console_clear(void)
{
    if (backend && backend->active && backend->active() && backend->clear)
        backend->clear();
}

void console_set_color(uint8_t fg, uint8_t bg)
{
    if (backend && backend->active && backend->active() && backend->set_color)
        backend->set_color(fg, bg);
}

void console_putc(char c)
{
    if (backend && backend->active && backend->active() && backend->putc)
        backend->putc(c);
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
        int active = backend && backend->active && backend->active();
        int c = active && backend->read_key ? backend->read_key() : -1;
        if (c >= 0 && c < 256)
            return (char)c;
        if (!active && serial_has_input())
            return serial_getc();
        if (active && backend->pump)
            backend->pump();
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
