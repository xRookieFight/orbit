#include "keyboard.h"
#include "isr.h"
#include "io.h"

#define KBD_DATA   0x60
#define BUFFER_SIZE 256

static const char map_normal[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

static const char map_shift[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' '
};

static volatile int ring[BUFFER_SIZE];
static volatile uint32_t head;
static volatile uint32_t tail;
static int shift_down;
static int caps_lock;

static void push(int c)
{
    uint32_t next = (head + 1) % BUFFER_SIZE;
    if (next != tail) {
        ring[head] = c;
        head = next;
    }
}

static void keyboard_callback(regs_t* regs)
{
    (void)regs;
    uint8_t code = inb(KBD_DATA);

    if (code & 0x80) {
        uint8_t released = code & 0x7F;
        if (released == 0x2A || released == 0x36)
            shift_down = 0;
        return;
    }

    if (code == 0x2A || code == 0x36) {
        shift_down = 1;
        return;
    }
    if (code == 0x3A) {
        caps_lock ^= 1;
        return;
    }
    if (code == 0x49) {
        push(KEY_PAGE_UP);
        return;
    }
    if (code == 0x51) {
        push(KEY_PAGE_DOWN);
        return;
    }
    if (code >= 128)
        return;

    char c = shift_down ? map_shift[code] : map_normal[code];
    if (caps_lock && c >= 'a' && c <= 'z' && !shift_down)
        c = (char)(c - 32);
    else if (caps_lock && c >= 'A' && c <= 'Z' && shift_down)
        c = (char)(c + 32);

    if (c)
        push(c);
}

void keyboard_init(void)
{
    head = 0;
    tail = 0;
    irq_register(1, keyboard_callback);
}

int keyboard_poll(void)
{
    if (head == tail)
        return -1;
    int c = ring[tail];
    tail = (tail + 1) % BUFFER_SIZE;
    return c;
}
