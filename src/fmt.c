#include "fmt.h"
#include "string.h"
#include <stdint.h>

static void emit_number(fmt_sink_t sink, void* ctx, unsigned int value,
                        int base, int is_signed, int width, char pad)
{
    char digits_buf[34];
    int negative = 0;

    if (is_signed && base == 10 && (int)value < 0) {
        negative = 1;
        value = (unsigned int)(-(int)value);
    }

    if (value == 0) {
        digits_buf[0] = '0';
        digits_buf[1] = '\0';
    } else {
        static const char digits[] = "0123456789abcdef";
        char tmp[34];
        int i = 0;
        while (value) {
            tmp[i++] = digits[value % (unsigned)base];
            value /= (unsigned)base;
        }
        int j = 0;
        while (i > 0)
            digits_buf[j++] = tmp[--i];
        digits_buf[j] = '\0';
    }

    int len = (int)strlen(digits_buf) + (negative ? 1 : 0);
    for (int k = len; k < width; k++)
        sink(pad, ctx);
    if (negative)
        sink('-', ctx);
    for (char* p = digits_buf; *p; p++)
        sink(*p, ctx);
}

void fmt_vformat(fmt_sink_t sink, void* ctx, const char* fmt, va_list args)
{
    while (*fmt) {
        if (*fmt != '%') {
            sink(*fmt++, ctx);
            continue;
        }
        fmt++;

        char pad = ' ';
        int left = 0;
        int width = 0;
        if (*fmt == '-') {
            left = 1;
            fmt++;
        }
        if (*fmt == '0') {
            pad = '0';
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        switch (*fmt) {
        case 'c':
            sink((char)va_arg(args, int), ctx);
            break;
        case 's': {
            const char* s = va_arg(args, const char*);
            if (!s)
                s = "(null)";
            int len = (int)strlen(s);
            if (!left)
                for (int k = len; k < width; k++)
                    sink(pad, ctx);
            while (*s)
                sink(*s++, ctx);
            if (left)
                for (int k = len; k < width; k++)
                    sink(' ', ctx);
            break;
        }
        case 'd':
        case 'i':
            emit_number(sink, ctx, (unsigned int)va_arg(args, int), 10, 1, width, pad);
            break;
        case 'u':
            emit_number(sink, ctx, va_arg(args, unsigned int), 10, 0, width, pad);
            break;
        case 'x':
            emit_number(sink, ctx, va_arg(args, unsigned int), 16, 0, width, pad);
            break;
        case 'p':
            sink('0', ctx);
            sink('x', ctx);
            emit_number(sink, ctx, (unsigned int)(uintptr_t)va_arg(args, void*), 16, 0, 8, '0');
            break;
        case '%':
            sink('%', ctx);
            break;
        case '\0':
            return;
        default:
            sink('%', ctx);
            sink(*fmt, ctx);
            break;
        }
        fmt++;
    }
}

typedef struct {
    char* buf;
    size_t size;
    size_t pos;
} buf_ctx_t;

static void buf_sink(char c, void* ctx)
{
    buf_ctx_t* b = (buf_ctx_t*)ctx;
    if (b->pos + 1 < b->size)
        b->buf[b->pos++] = c;
}

int kvsnprintf(char* buf, size_t size, const char* fmt, va_list args)
{
    buf_ctx_t ctx = { buf, size, 0 };
    fmt_vformat(buf_sink, &ctx, fmt, args);
    if (size > 0)
        buf[ctx.pos] = '\0';
    return (int)ctx.pos;
}

int ksnprintf(char* buf, size_t size, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int n = kvsnprintf(buf, size, fmt, args);
    va_end(args);
    return n;
}
