#include "gdt.h"
#include <stdint.h>

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct gdt_entry entries[3];
static struct gdt_ptr pointer;

extern void gdt_flush(uint32_t ptr);

static void set_entry(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran)
{
    entries[i].base_low = (uint16_t)(base & 0xFFFF);
    entries[i].base_mid = (uint8_t)((base >> 16) & 0xFF);
    entries[i].base_high = (uint8_t)((base >> 24) & 0xFF);
    entries[i].limit_low = (uint16_t)(limit & 0xFFFF);
    entries[i].granularity = (uint8_t)(((limit >> 16) & 0x0F) | (gran & 0xF0));
    entries[i].access = access;
}

void gdt_init(void)
{
    set_entry(0, 0, 0, 0, 0);
    set_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    set_entry(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    pointer.limit = (uint16_t)(sizeof(entries) - 1);
    pointer.base = (uint32_t)&entries;

    gdt_flush((uint32_t)&pointer);
}
