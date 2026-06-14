#include "idt.h"
#include "string.h"

struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t flags;
    uint16_t base_mid;
    uint32_t base_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry entries[256];
static struct idt_ptr pointer;

void idt_set_gate(uint8_t num, uint64_t base, uint16_t selector, uint8_t flags)
{
    entries[num].base_low = (uint16_t)(base & 0xFFFF);
    entries[num].base_mid = (uint16_t)((base >> 16) & 0xFFFF);
    entries[num].base_high = (uint32_t)(base >> 32);
    entries[num].selector = selector;
    entries[num].ist = 0;
    entries[num].flags = flags;
    entries[num].reserved = 0;
}

void idt_init(void)
{
    memset(entries, 0, sizeof(entries));
    pointer.limit = (uint16_t)(sizeof(entries) - 1);
    pointer.base = (uint64_t)&entries;
    __asm__ volatile("lidt %0" : : "m"(pointer));
}
