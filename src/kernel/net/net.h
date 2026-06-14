#pragma once
#include <stdint.h>

#define ETH_P_IP  0x0800
#define ETH_P_ARP 0x0806
#define ETH_ALEN  6
#define ETH_HLEN  14
#define ETH_FRAME_MAX 1514

typedef void (*net_tx_fn)(const void* data, uint16_t len);

extern const uint8_t net_broadcast_mac[ETH_ALEN];

static inline uint16_t htons(uint16_t v)
{
    return (uint16_t)((v << 8) | (v >> 8));
}

static inline uint16_t ntohs(uint16_t v)
{
    return htons(v);
}

static inline uint32_t htonl(uint32_t v)
{
    return ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8) |
           ((v & 0x00FF0000u) >> 8) | ((v & 0xFF000000u) >> 24);
}

static inline uint32_t ntohl(uint32_t v)
{
    return htonl(v);
}

void net_init(void);
void net_bind(const uint8_t mac[ETH_ALEN], net_tx_fn tx);

int net_is_up(void);
void net_set_up(int up);
int net_has_device(void);

const uint8_t* net_mac(void);
uint32_t net_ip(void);
uint32_t net_netmask(void);
uint32_t net_gateway(void);
void net_set_ip(uint32_t ip);
void net_set_netmask(uint32_t mask);
void net_set_gateway(uint32_t gw);
uint32_t net_dns(void);
void net_set_dns(uint32_t dns);

void net_send(const uint8_t dst_mac[ETH_ALEN], uint16_t ethertype, const void* payload, uint16_t len);
void net_rx_frame(const uint8_t* frame, uint16_t len);

uint32_t net_rx_packets(void);
uint32_t net_tx_packets(void);
uint32_t net_rx_bytes(void);
uint32_t net_tx_bytes(void);
uint32_t net_rx_dropped(void);

int net_ip_parse(const char* s, uint32_t* out);
void net_ip_format(uint32_t ip, char* buf);
void net_mac_format(const uint8_t* mac, char* buf);
