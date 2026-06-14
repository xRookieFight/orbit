#pragma once
#include <stdarg.h>
#include <stddef.h>

typedef void (*fmt_sink_t)(char c, void* ctx);

void fmt_vformat(fmt_sink_t sink, void* ctx, const char* fmt, va_list args);
int kvsnprintf(char* buf, size_t size, const char* fmt, va_list args);
int ksnprintf(char* buf, size_t size, const char* fmt, ...);
