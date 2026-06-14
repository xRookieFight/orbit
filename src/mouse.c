#include "mouse.h"
#include "isr.h"
#include "io.h"

#define PS2_DATA   0x60
#define PS2_STATUS 0x64
#define PS2_CMD    0x64

static volatile int pos_x;
static volatile int pos_y;
static volatile int buttons;
static volatile int dirty;
static int max_x;
static int max_y;

static uint8_t packet[3];
static int packet_index;

static void wait_write(void)
{
    for (int i = 0; i < 100000; i++)
        if (!(inb(PS2_STATUS) & 2))
            return;
}

static void wait_read(void)
{
    for (int i = 0; i < 100000; i++)
        if (inb(PS2_STATUS) & 1)
            return;
}

static void mouse_send(uint8_t value)
{
    wait_write();
    outb(PS2_CMD, 0xD4);
    wait_write();
    outb(PS2_DATA, value);
    wait_read();
    inb(PS2_DATA);
}

static void mouse_callback(regs_t* regs)
{
    (void)regs;
    uint8_t status = inb(PS2_STATUS);
    if (!(status & 1))
        return;
    uint8_t data = inb(PS2_DATA);
    if (!(status & 0x20))
        return;

    if (packet_index == 0 && !(data & 0x08))
        return;

    packet[packet_index++] = data;
    if (packet_index < 3)
        return;
    packet_index = 0;

    int dx = (int)packet[1];
    int dy = (int)packet[2];
    if (packet[0] & 0x10)
        dx -= 256;
    if (packet[0] & 0x20)
        dy -= 256;

    pos_x += dx;
    pos_y -= dy;
    if (pos_x < 0)
        pos_x = 0;
    if (pos_y < 0)
        pos_y = 0;
    if (pos_x > max_x)
        pos_x = max_x;
    if (pos_y > max_y)
        pos_y = max_y;

    buttons = packet[0] & 7;
    dirty = 1;
}

void mouse_init(int screen_w, int screen_h)
{
    max_x = screen_w - 1;
    max_y = screen_h - 1;
    pos_x = screen_w / 2;
    pos_y = screen_h / 2;

    wait_write();
    outb(PS2_CMD, 0xA8);

    wait_write();
    outb(PS2_CMD, 0x20);
    wait_read();
    uint8_t status = inb(PS2_DATA);
    status |= 2;
    status &= (uint8_t)~0x20;
    wait_write();
    outb(PS2_CMD, 0x60);
    wait_write();
    outb(PS2_DATA, status);

    mouse_send(0xF6);
    mouse_send(0xF4);

    irq_register(12, mouse_callback);
}

int mouse_x(void)
{
    return pos_x;
}

int mouse_y(void)
{
    return pos_y;
}

int mouse_buttons(void)
{
    return buttons;
}

int mouse_consume_dirty(void)
{
    if (!dirty)
        return 0;
    dirty = 0;
    return 1;
}
