#pragma once
#include <stdint.h>

typedef struct {
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    uint16_t vendor;
    uint16_t device;
    uint32_t bar0;
    uint8_t irq;
} pci_device_t;

uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off);
uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off);
void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off, uint32_t val);
void pci_enable_bus_master(const pci_device_t* dev);
int pci_find(uint16_t vendor, uint16_t device, pci_device_t* out);
