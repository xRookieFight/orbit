#include "ip.h"
#include "net.h"
#include "arp.h"
#include "icmp.h"
#include "udp.h"
#include "tcp.h"
#include "string.h"

typedef struct __attribute__((packed)) {
    uint8_t ver_ihl;
    uint8_t tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags_frag;
    uint8_t ttl;
    uint8_t proto;
    uint16_t checksum;
    uint8_t src[4];
    uint8_t dst[4];
} ip_header_t;

static uint16_t ip_id;

uint16_t ip_checksum(const void* data, uint16_t len)
{
    const uint8_t* p = (const uint8_t*)data;
    uint32_t sum = 0;
    while (len > 1) {
        sum += (uint32_t)((p[0] << 8) | p[1]);
        p += 2;
        len = (uint16_t)(len - 2);
    }
    if (len)
        sum += (uint32_t)(p[0] << 8);
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum & 0xFFFF);
}

static void unpack_ip(uint32_t ip, uint8_t* b)
{
    b[0] = (uint8_t)(ip >> 24);
    b[1] = (uint8_t)(ip >> 16);
    b[2] = (uint8_t)(ip >> 8);
    b[3] = (uint8_t)ip;
}

static uint32_t pack_ip(const uint8_t* b)
{
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8) | (uint32_t)b[3];
}

int ip_send(uint32_t dst_ip, uint8_t proto, const void* payload, uint16_t len)
{
    if (!net_is_up())
        return -1;
    if (len > ETH_FRAME_MAX - ETH_HLEN - 20)
        return -1;

    uint8_t dst_mac[6];
    if (dst_ip == 0xFFFFFFFFu) {
        memcpy(dst_mac, net_broadcast_mac, 6);
    } else {
        uint32_t next_hop;
        if ((dst_ip & net_netmask()) == (net_ip() & net_netmask()))
            next_hop = dst_ip;
        else
            next_hop = net_gateway();
        if (!arp_resolve(next_hop, dst_mac))
            return -2;
    }

    uint8_t packet[ETH_FRAME_MAX - ETH_HLEN];
    ip_header_t* h = (ip_header_t*)packet;
    h->ver_ihl = 0x45;
    h->tos = 0;
    h->total_len = htons((uint16_t)(20 + len));
    h->id = htons(ip_id++);
    h->flags_frag = 0;
    h->ttl = 64;
    h->proto = proto;
    h->checksum = 0;
    unpack_ip(net_ip(), h->src);
    unpack_ip(dst_ip, h->dst);
    h->checksum = htons(ip_checksum(h, 20));

    if (len)
        memcpy(packet + 20, payload, len);

    net_send(dst_mac, ETH_P_IP, packet, (uint16_t)(20 + len));
    return 0;
}

void ip_input(const uint8_t* src_mac, const uint8_t* data, uint16_t len)
{
    (void)src_mac;
    if (len < 20)
        return;
    const ip_header_t* h = (const ip_header_t*)data;
    if ((h->ver_ihl >> 4) != 4)
        return;
    uint16_t ihl = (uint16_t)((h->ver_ihl & 0x0F) * 4);
    if (ihl < 20 || ihl > len)
        return;

    uint32_t dst = pack_ip(h->dst);
    if (dst != net_ip() && dst != 0xFFFFFFFFu)
        return;

    uint32_t src = pack_ip(h->src);
    uint16_t total = ntohs(h->total_len);
    if (total > len)
        total = len;
    const uint8_t* payload = data + ihl;
    uint16_t plen = (uint16_t)(total - ihl);

    if (h->proto == IP_PROTO_ICMP)
        icmp_input(src, payload, plen);
    else if (h->proto == IP_PROTO_UDP)
        udp_input(src, payload, plen);
    else if (h->proto == IP_PROTO_TCP)
        tcp_input(src, payload, plen);
}
