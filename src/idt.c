#include "idt.h"
#include "string.h"

struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry entries[256];
static struct idt_ptr pointer;

extern void idt_load(uint32_t ptr);

void idt_set_gate(uint8_t num, uint32_t base, uint16_t selector, uint8_t flags)
{
    entries[num].base_low = (uint16_t)(base & 0xFFFF);
    entries[num].base_high = (uint16_t)((base >> 16) & 0xFFFF);
    entries[num].selector = selector;
    entries[num].zero = 0;
    entries[num].flags = flags;
}

void idt_init(void)
{
    pointer.limit = (uint16_t)(sizeof(entries) - 1);
    pointer.base = (uint32_t)&entries;
    memset(entries, 0, sizeof(entries));
    idt_load((uint32_t)&pointer);
}
