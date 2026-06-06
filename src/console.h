#pragma once
#include <stdint.h>
#include <stdarg.h>

void console_init(void);
void console_clear(void);
void console_putc(char c);
void console_write(const char* s);
void console_printf(const char* fmt, ...);
void console_vprintf(const char* fmt, va_list args);
void console_set_color(uint8_t fg, uint8_t bg);

char console_getc(void);
void console_readline(char* buffer, uint32_t size);
