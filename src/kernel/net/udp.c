#include "udp.h"
#include "ip.h"
#include "net.h"
#include "string.h"

#define UDP_BINDINGS 8

typedef struct __attribute__((packed)) {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} udp_header_t;

typedef struct {
    uint16_t port;
    udp_handler_t handler;
    int used;
} udp_binding_t;

static udp_binding_t bindings[UDP_BINDINGS];

void udp_init(void)
{
    memset(bindings, 0, sizeof(bindings));
}

int udp_bind(uint16_t port, udp_handler_t handler)
{
    for (int i = 0; i < UDP_BINDINGS; i++) {
        if (!bindings[i].used) {
            bindings[i].port = port;
            bindings[i].handler = handler;
            bindings[i].used = 1;
            return 0;
        }
    }
    return -1;
}

void udp_unbind(uint16_t port)
{
    for (int i = 0; i < UDP_BINDINGS; i++)
        if (bindings[i].used && bindings[i].port == port)
            bindings[i].used = 0;
}

int udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port, const void* data, uint16_t len)
{
    uint8_t buf[ETH_FRAME_MAX - ETH_HLEN - 20];
    if (len > sizeof(buf) - sizeof(udp_header_t))
        return -1;
    udp_header_t* h = (udp_header_t*)buf;
    h->src_port = htons(src_port);
    h->dst_port = htons(dst_port);
    h->length = htons((uint16_t)(sizeof(udp_header_t) + len));
    h->checksum = 0;
    if (len)
        memcpy(buf + sizeof(udp_header_t), data, len);
    return ip_send(dst_ip, IP_PROTO_UDP, buf, (uint16_t)(sizeof(udp_header_t) + len));
}

void udp_input(uint32_t src_ip, const uint8_t* data, uint16_t len)
{
    if (len < (uint16_t)sizeof(udp_header_t))
        return;
    const udp_header_t* h = (const udp_header_t*)data;
    uint16_t dst_port = ntohs(h->dst_port);
    uint16_t src_port = ntohs(h->src_port);
    const uint8_t* payload = data + sizeof(udp_header_t);
    uint16_t plen = (uint16_t)(len - sizeof(udp_header_t));

    for (int i = 0; i < UDP_BINDINGS; i++)
        if (bindings[i].used && bindings[i].port == dst_port)
            bindings[i].handler(src_ip, src_port, payload, plen);
}
