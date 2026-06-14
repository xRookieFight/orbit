#include "pci.h"
#include "io.h"

#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC

static uint32_t pci_address(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off)
{
    return (uint32_t)((1u << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) |
                      ((uint32_t)func << 8) | (off & 0xFC));
}

uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off)
{
    outl(PCI_CONFIG_ADDR, pci_address(bus, slot, func, off));
    return inl(PCI_CONFIG_DATA);
}

uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off)
{
    uint32_t v = pci_read32(bus, slot, func, off);
    return (uint16_t)((v >> ((off & 2) * 8)) & 0xFFFF);
}

void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off, uint32_t val)
{
    outl(PCI_CONFIG_ADDR, pci_address(bus, slot, func, off));
    outl(PCI_CONFIG_DATA, val);
}

void pci_enable_bus_master(const pci_device_t* dev)
{
    uint32_t cmd = pci_read32(dev->bus, dev->slot, dev->func, 0x04);
    cmd |= 0x07;
    pci_write32(dev->bus, dev->slot, dev->func, 0x04, cmd);
}

int pci_find(uint16_t vendor, uint16_t device, pci_device_t* out)
{
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint32_t id = pci_read32((uint8_t)bus, slot, func, 0x00);
                uint16_t v = (uint16_t)(id & 0xFFFF);
                uint16_t d = (uint16_t)(id >> 16);
                if (v == 0xFFFF)
                    continue;
                if (v == vendor && d == device) {
                    out->bus = (uint8_t)bus;
                    out->slot = slot;
                    out->func = func;
                    out->vendor = v;
                    out->device = d;
                    out->bar0 = pci_read32((uint8_t)bus, slot, func, 0x10);
                    out->irq = (uint8_t)(pci_read32((uint8_t)bus, slot, func, 0x3C) & 0xFF);
                    return 0;
                }
            }
        }
    }
    return -1;
}
