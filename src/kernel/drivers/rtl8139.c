#include "rtl8139.h"
#include "pci.h"
#include "net.h"
#include "io.h"
#include "isr.h"
#include "heap.h"
#include "string.h"

#define RTL_VENDOR 0x10EC
#define RTL_DEVICE 0x8139

#define REG_MAC      0x00
#define REG_TSD0     0x10
#define REG_TSAD0    0x20
#define REG_RBSTART  0x30
#define REG_CMD      0x37
#define REG_CAPR     0x38
#define REG_IMR      0x3C
#define REG_ISR      0x3E
#define REG_RCR      0x44
#define REG_CONFIG1  0x52

#define CMD_RST 0x10
#define CMD_RE  0x08
#define CMD_TE  0x04
#define CMD_BUFE 0x01

#define ISR_ROK 0x01
#define ISR_TOK 0x04

#define RX_BUF_SIZE 8192
#define RX_BUF_ALLOC (8192 + 16 + 1536)
#define TX_BUF_SIZE 2048

static uint16_t io_base;
static uint8_t* rx_buffer;
static uint8_t* tx_buffer[4];
static int tx_cur;
static uint32_t rx_offset;
static uint8_t hw_mac[6];

static void rtl8139_send(const void* data, uint16_t len)
{
    int d = tx_cur;
    if (len > TX_BUF_SIZE)
        len = TX_BUF_SIZE;
    memcpy(tx_buffer[d], data, len);
    outl(io_base + REG_TSAD0 + (uint16_t)(d * 4), (uint32_t)(uintptr_t)tx_buffer[d]);
    outl(io_base + REG_TSD0 + (uint16_t)(d * 4), len);
    tx_cur = (d + 1) & 3;
}

static void rtl8139_receive(void)
{
    while (!(inb(io_base + REG_CMD) & CMD_BUFE)) {
        uint8_t* p = rx_buffer + rx_offset;
        uint16_t status = (uint16_t)(p[0] | (p[1] << 8));
        uint16_t pkt_len = (uint16_t)(p[2] | (p[3] << 8));

        if (!(status & 0x01) || pkt_len < 4 || pkt_len > 1522) {
            rx_offset = 0;
            outw(io_base + REG_CAPR, (uint16_t)(rx_offset - 16));
            break;
        }

        uint16_t frame_len = (uint16_t)(pkt_len - 4);
        net_rx_frame(p + 4, frame_len);

        rx_offset = (rx_offset + pkt_len + 4 + 3) & ~3u;
        rx_offset %= RX_BUF_SIZE;
        outw(io_base + REG_CAPR, (uint16_t)(rx_offset - 16));
    }
}

static void rtl8139_irq(regs_t* regs)
{
    (void)regs;
    uint16_t status = inw(io_base + REG_ISR);
    outw(io_base + REG_ISR, status);
    if (status & ISR_ROK)
        rtl8139_receive();
}

int rtl8139_init(void)
{
    pci_device_t dev;
    if (pci_find(RTL_VENDOR, RTL_DEVICE, &dev) != 0)
        return -1;

    pci_enable_bus_master(&dev);
    io_base = (uint16_t)(dev.bar0 & 0xFFFC);

    rx_buffer = (uint8_t*)kmalloc(RX_BUF_ALLOC);
    if (!rx_buffer)
        return -1;
    memset(rx_buffer, 0, RX_BUF_ALLOC);

    for (int i = 0; i < 4; i++) {
        tx_buffer[i] = (uint8_t*)kmalloc(TX_BUF_SIZE);
        if (!tx_buffer[i])
            return -1;
    }
    tx_cur = 0;
    rx_offset = 0;

    outb(io_base + REG_CONFIG1, 0x00);

    outb(io_base + REG_CMD, CMD_RST);
    while (inb(io_base + REG_CMD) & CMD_RST)
        ;

    outl(io_base + REG_RBSTART, (uint32_t)(uintptr_t)rx_buffer);
    outw(io_base + REG_IMR, ISR_ROK | ISR_TOK);
    outl(io_base + REG_RCR, 0x0000E78Fu);
    outw(io_base + REG_CAPR, (uint16_t)(rx_offset - 16));

    irq_register(dev.irq, rtl8139_irq);

    outb(io_base + REG_CMD, CMD_RE | CMD_TE);

    for (int i = 0; i < 6; i++)
        hw_mac[i] = inb(io_base + REG_MAC + (uint16_t)i);

    net_bind(hw_mac, rtl8139_send);
    return 0;
}
