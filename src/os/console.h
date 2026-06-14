#pragma once
#include <stdint.h>
#include <stdarg.h>

#ifndef COL_BLACK
#define COL_BLACK         0
#define COL_BLUE          1
#define COL_GREEN         2
#define COL_CYAN          3
#define COL_RED           4
#define COL_MAGENTA       5
#define COL_BROWN         6
#define COL_LIGHT_GREY    7
#define COL_DARK_GREY     8
#define COL_LIGHT_BLUE    9
#define COL_LIGHT_GREEN   10
#define COL_LIGHT_CYAN    11
#define COL_LIGHT_RED     12
#define COL_LIGHT_MAGENTA 13
#define COL_YELLOW        14
#define COL_WHITE         15
#endif

typedef struct {
    int (*active)(void);
    void (*clear)(void);
    void (*set_color)(uint8_t fg, uint8_t bg);
    void (*putc)(char c);
    int (*read_key)(void);
    void (*pump)(void);
} console_backend_t;

void console_init(void);
void console_bind_backend(const console_backend_t* backend);
void console_clear(void);
void console_putc(char c);
void console_write(const char* s);
void console_printf(const char* fmt, ...);
void console_vprintf(const char* fmt, va_list args);
void console_set_color(uint8_t fg, uint8_t bg);

char console_getc(void);
void console_readline(char* buffer, uint32_t size);
