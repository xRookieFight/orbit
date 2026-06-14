#include "serial.h"
#include "io.h"

#define COM1 0x3F8

void serial_init(void)
{
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

static int transmit_empty(void)
{
    return inb(COM1 + 5) & 0x20;
}

void serial_putc(char c)
{
    if (c == '\n') {
        while (!transmit_empty())
            ;
        outb(COM1, '\r');
    }
    while (!transmit_empty())
        ;
    outb(COM1, (uint8_t)c);
}

void serial_write(const char* s)
{
    while (*s)
        serial_putc(*s++);
}

bool serial_has_input(void)
{
    return (inb(COM1 + 5) & 0x01) != 0;
}

char serial_getc(void)
{
    while (!serial_has_input())
        ;
    return (char)inb(COM1);
}
