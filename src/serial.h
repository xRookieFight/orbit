#pragma once
#include <stdint.h>
#include <stdbool.h>

void serial_init(void);
void serial_putc(char c);
void serial_write(const char* s);
bool serial_has_input(void);
char serial_getc(void);
