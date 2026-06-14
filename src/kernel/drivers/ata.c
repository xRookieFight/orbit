#include "ata.h"
#include "io.h"

#define ATA_DATA    0x1F0
#define ATA_SECCNT  0x1F2
#define ATA_LBA0    0x1F3
#define ATA_LBA1    0x1F4
#define ATA_LBA2    0x1F5
#define ATA_DRIVE   0x1F6
#define ATA_CMD     0x1F7
#define ATA_STATUS  0x1F7

#define ATA_SR_BSY  0x80
#define ATA_SR_DRQ  0x08
#define ATA_SR_ERR  0x01

static int ata_poll(void)
{
    for (int i = 0; i < 4; i++)
        inb(ATA_STATUS);
    uint32_t spin = 0;
    while (inb(ATA_STATUS) & ATA_SR_BSY) {
        if (++spin > 10000000u)
            return -1;
    }
    uint8_t s = inb(ATA_STATUS);
    if (s & ATA_SR_ERR)
        return -1;
    if (!(s & ATA_SR_DRQ))
        return -1;
    return 0;
}

static int ata_read_chunk(uint32_t lba, uint8_t count, uint16_t* buf)
{
    while (inb(ATA_STATUS) & ATA_SR_BSY)
        ;
    outb(ATA_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECCNT, count);
    outb(ATA_LBA0, (uint8_t)lba);
    outb(ATA_LBA1, (uint8_t)(lba >> 8));
    outb(ATA_LBA2, (uint8_t)(lba >> 16));
    outb(ATA_CMD, 0x20);

    uint32_t sectors = count ? count : 256;
    for (uint32_t s = 0; s < sectors; s++) {
        if (ata_poll() != 0)
            return -1;
        for (int i = 0; i < 256; i++)
            *buf++ = inw(ATA_DATA);
    }
    return 0;
}

int ata_read(uint32_t lba, uint32_t count, void* buf)
{
    uint16_t* p = (uint16_t*)buf;
    while (count > 0) {
        uint32_t chunk = count > 256 ? 256 : count;
        if (ata_read_chunk(lba, (uint8_t)(chunk & 0xFF), p) != 0)
            return -1;
        lba += chunk;
        count -= chunk;
        p += chunk * 256;
    }
    return 0;
}
