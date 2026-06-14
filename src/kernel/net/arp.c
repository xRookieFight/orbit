#include "arp.h"
#include "net.h"
#include "string.h"
#include "pit.h"

#define ARP_CACHE_SIZE 16
#define ARP_HTYPE_ETH  1
#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY   2

typedef struct __attribute__((packed)) {
    uint16_t htype;
    uint16_t ptype;
    uint8_t hlen;
    uint8_t plen;
    uint16_t oper;
    uint8_t sha[6];
    uint8_t spa[4];
    uint8_t tha[6];
    uint8_t tpa[4];
} arp_packet_t;

typedef struct {
    uint32_t ip;
    uint8_t mac[6];
    int valid;
} arp_entry_t;

static arp_entry_t cache[ARP_CACHE_SIZE];

static uint32_t pack_ip(const uint8_t* b)
{
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8) | (uint32_t)b[3];
}

static void unpack_ip(uint32_t ip, uint8_t* b)
{
    b[0] = (uint8_t)(ip >> 24);
    b[1] = (uint8_t)(ip >> 16);
    b[2] = (uint8_t)(ip >> 8);
    b[3] = (uint8_t)ip;
}

void arp_init(void)
{
    memset(cache, 0, sizeof(cache));
}

static void arp_cache_put(uint32_t ip, const uint8_t* mac)
{
    int free_slot = -1;
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (cache[i].valid && cache[i].ip == ip) {
            memcpy(cache[i].mac, mac, 6);
            return;
        }
        if (!cache[i].valid && free_slot < 0)
            free_slot = i;
    }
    if (free_slot < 0)
        free_slot = 0;
    cache[free_slot].ip = ip;
    memcpy(cache[free_slot].mac, mac, 6);
    cache[free_slot].valid = 1;
}

int arp_lookup(uint32_t ip, uint8_t mac_out[6])
{
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (cache[i].valid && cache[i].ip == ip) {
            memcpy(mac_out, cache[i].mac, 6);
            return 1;
        }
    }
    return 0;
}

static void arp_send(uint16_t oper, uint32_t target_ip, const uint8_t* target_mac)
{
    arp_packet_t pkt;
    pkt.htype = htons(ARP_HTYPE_ETH);
    pkt.ptype = htons(ETH_P_IP);
    pkt.hlen = 6;
    pkt.plen = 4;
    pkt.oper = htons(oper);
    memcpy(pkt.sha, net_mac(), 6);
    unpack_ip(net_ip(), pkt.spa);
    memcpy(pkt.tha, target_mac, 6);
    unpack_ip(target_ip, pkt.tpa);

    const uint8_t* dst = (oper == ARP_OP_REQUEST) ? net_broadcast_mac : target_mac;
    net_send(dst, ETH_P_ARP, &pkt, sizeof(pkt));
}

void arp_request(uint32_t ip)
{
    static const uint8_t zero[6] = { 0, 0, 0, 0, 0, 0 };
    arp_send(ARP_OP_REQUEST, ip, zero);
}

void arp_input(const uint8_t* data, uint16_t len)
{
    if (len < (uint16_t)sizeof(arp_packet_t))
        return;
    const arp_packet_t* pkt = (const arp_packet_t*)data;
    if (ntohs(pkt->htype) != ARP_HTYPE_ETH || ntohs(pkt->ptype) != ETH_P_IP)
        return;

    uint32_t sender_ip = pack_ip(pkt->spa);
    uint32_t target_ip = pack_ip(pkt->tpa);
    arp_cache_put(sender_ip, pkt->sha);

    if (ntohs(pkt->oper) == ARP_OP_REQUEST && target_ip == net_ip())
        arp_send(ARP_OP_REPLY, sender_ip, pkt->sha);
}

int arp_resolve(uint32_t ip, uint8_t mac_out[6])
{
    if (arp_lookup(ip, mac_out))
        return 1;
    for (int attempt = 0; attempt < 4; attempt++) {
        arp_request(ip);
        for (int t = 0; t < 50; t++) {
            pit_sleep(10);
            if (arp_lookup(ip, mac_out))
                return 1;
        }
    }
    return 0;
}

void arp_cache_clear(void)
{
    memset(cache, 0, sizeof(cache));
}

int arp_cache_count(void)
{
    int n = 0;
    for (int i = 0; i < ARP_CACHE_SIZE; i++)
        if (cache[i].valid)
            n++;
    return n;
}

int arp_cache_entry(int idx, uint32_t* ip, uint8_t mac_out[6])
{
    int n = 0;
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!cache[i].valid)
            continue;
        if (n == idx) {
            *ip = cache[i].ip;
            memcpy(mac_out, cache[i].mac, 6);
            return 1;
        }
        n++;
    }
    return 0;
}
