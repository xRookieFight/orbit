#include "mouse.h"
#include "isr.h"
#include "io.h"

#define PS2_DATA   0x60
#define PS2_STATUS 0x64
#define PS2_CMD    0x64

#define VMM_MAGIC              0x564D5868u
#define VMM_PORT               0x5658
#define VMM_GETVERSION         10u
#define VMM_ABS_DATA           39u
#define VMM_ABS_STATUS         40u
#define VMM_ABS_COMMAND        41u
#define VMM_ABS_ENABLE         0x45414552u
#define VMM_ABS_RELATIVE       0xF5u
#define VMM_ABS_ABSOLUTE       0x53424152u

#define VMM_BTN_LEFT   0x20u
#define VMM_BTN_RIGHT  0x10u
#define VMM_BTN_MIDDLE 0x08u

static volatile int pos_x;
static volatile int pos_y;
static volatile int buttons;
static volatile int dirty;
static int max_x;
static int max_y;
static int vmmouse_present;

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

static void vmm_call(uint32_t cmd, uint32_t in_bx,
                     uint32_t* a, uint32_t* b, uint32_t* c, uint32_t* d)
{
    uint32_t eax = VMM_MAGIC, ebx = in_bx, ecx = cmd, edx = VMM_PORT;
    __asm__ volatile("inl %%dx, %%eax"
                     : "+a"(eax), "+b"(ebx), "+c"(ecx), "+d"(edx));
    if (a) *a = eax;
    if (b) *b = ebx;
    if (c) *c = ecx;
    if (d) *d = edx;
}

static void vmmouse_enable(void)
{
    uint32_t a, b, c, d;
    vmm_call(VMM_ABS_COMMAND, VMM_ABS_ENABLE, 0, 0, 0, 0);
    vmm_call(VMM_ABS_STATUS, 0, &a, &b, &c, &d);
    vmm_call(VMM_ABS_DATA, 1, &a, &b, &c, &d);
    vmm_call(VMM_ABS_COMMAND, VMM_ABS_ABSOLUTE, 0, 0, 0, 0);
}

static int vmmouse_detect(void)
{
    uint32_t a, b, c, d;
    vmm_call(VMM_GETVERSION, 0, &a, &b, &c, &d);
    return b == VMM_MAGIC && a != 0xFFFFFFFFu;
}

static void vmmouse_poll(void)
{
    uint32_t status, flags, x, y, z, junk;
    vmm_call(VMM_ABS_STATUS, 0, &status, &junk, &junk, &junk);
    while ((status & 0xFFFF) >= 4) {
        if ((status & 0xFFFF0000u) == 0xFFFF0000u) {
            vmmouse_enable();
            return;
        }
        vmm_call(VMM_ABS_DATA, 4, &flags, &x, &y, &z);
        pos_x = (int)((x * (uint32_t)max_x) / 0xFFFFu);
        pos_y = (int)((y * (uint32_t)max_y) / 0xFFFFu);
        int b = 0;
        if (flags & VMM_BTN_LEFT)
            b |= 1;
        if (flags & VMM_BTN_RIGHT)
            b |= 2;
        if (flags & VMM_BTN_MIDDLE)
            b |= 4;
        buttons = b;
        dirty = 1;
        vmm_call(VMM_ABS_STATUS, 0, &status, &junk, &junk, &junk);
    }
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

    if (vmmouse_present) {
        vmmouse_poll();
        return;
    }

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

    vmmouse_present = vmmouse_detect();
    if (vmmouse_present)
        vmmouse_enable();

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
