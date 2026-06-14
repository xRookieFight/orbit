#include "net.h"
#include "arp.h"
#include "ip.h"
#include "string.h"
#include "fmt.h"

const uint8_t net_broadcast_mac[ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

static uint8_t if_mac[ETH_ALEN];
static net_tx_fn if_tx;
static int if_up;
static int if_present;

static uint32_t if_ip;
static uint32_t if_mask;
static uint32_t if_gw;
static uint32_t if_dns;

static uint32_t stat_rx_packets;
static uint32_t stat_tx_packets;
static uint32_t stat_rx_bytes;
static uint32_t stat_tx_bytes;
static uint32_t stat_rx_dropped;

void net_init(void)
{
    if_tx = 0;
    if_up = 0;
    if_present = 0;
    if_ip = 0x0A00020Fu;
    if_mask = 0xFFFFFF00u;
    if_gw = 0x0A000202u;
    stat_rx_packets = 0;
    stat_tx_packets = 0;
    stat_rx_bytes = 0;
    stat_tx_bytes = 0;
    stat_rx_dropped = 0;
    arp_init();
}

void net_bind(const uint8_t mac[ETH_ALEN], net_tx_fn tx)
{
    memcpy(if_mac, mac, ETH_ALEN);
    if_tx = tx;
    if_present = 1;
    if_up = 1;
}

int net_is_up(void)
{
    return if_up && if_present;
}

void net_set_up(int up)
{
    if (if_present)
        if_up = up ? 1 : 0;
}

int net_has_device(void)
{
    return if_present;
}

const uint8_t* net_mac(void)
{
    return if_mac;
}

uint32_t net_ip(void)
{
    return if_ip;
}

uint32_t net_netmask(void)
{
    return if_mask;
}

uint32_t net_gateway(void)
{
    return if_gw;
}

void net_set_ip(uint32_t ip)
{
    if_ip = ip;
}

void net_set_netmask(uint32_t mask)
{
    if_mask = mask;
}

void net_set_gateway(uint32_t gw)
{
    if_gw = gw;
}

uint32_t net_dns(void)
{
    if (if_dns)
        return if_dns;
    if (if_gw)
        return (if_gw & 0xFFFFFF00u) | 3u;
    return 0;
}

void net_set_dns(uint32_t dns)
{
    if_dns = dns;
}

uint32_t net_rx_packets(void) { return stat_rx_packets; }
uint32_t net_tx_packets(void) { return stat_tx_packets; }
uint32_t net_rx_bytes(void)   { return stat_rx_bytes; }
uint32_t net_tx_bytes(void)   { return stat_tx_bytes; }
uint32_t net_rx_dropped(void) { return stat_rx_dropped; }

void net_send(const uint8_t dst_mac[ETH_ALEN], uint16_t ethertype, const void* payload, uint16_t len)
{
    if (!net_is_up() || !if_tx)
        return;
    if (len > ETH_FRAME_MAX - ETH_HLEN)
        return;

    uint8_t frame[ETH_FRAME_MAX];
    memcpy(frame, dst_mac, ETH_ALEN);
    memcpy(frame + ETH_ALEN, if_mac, ETH_ALEN);
    uint16_t et = htons(ethertype);
    frame[12] = (uint8_t)(et & 0xFF);
    frame[13] = (uint8_t)(et >> 8);
    if (len)
        memcpy(frame + ETH_HLEN, payload, len);

    uint16_t total = (uint16_t)(ETH_HLEN + len);
    if (total < 60)
        total = 60;

    if_tx(frame, total);
    stat_tx_packets++;
    stat_tx_bytes += total;
}

void net_rx_frame(const uint8_t* frame, uint16_t len)
{
    if (len < ETH_HLEN) {
        stat_rx_dropped++;
        return;
    }
    stat_rx_packets++;
    stat_rx_bytes += len;

    uint16_t ethertype = (uint16_t)((frame[12] << 8) | frame[13]);
    const uint8_t* payload = frame + ETH_HLEN;
    uint16_t plen = (uint16_t)(len - ETH_HLEN);
    const uint8_t* src_mac = frame + ETH_ALEN;

    if (ethertype == ETH_P_ARP)
        arp_input(payload, plen);
    else if (ethertype == ETH_P_IP)
        ip_input(src_mac, payload, plen);
    else
        stat_rx_dropped++;
}

int net_ip_parse(const char* s, uint32_t* out)
{
    uint32_t parts[4];
    int idx = 0;
    int digits = 0;
    uint32_t cur = 0;
    for (const char* p = s;; p++) {
        if (*p >= '0' && *p <= '9') {
            cur = cur * 10 + (uint32_t)(*p - '0');
            if (cur > 255)
                return 0;
            digits++;
        } else if (*p == '.' || *p == '\0') {
            if (digits == 0 || idx >= 4)
                return 0;
            parts[idx++] = cur;
            cur = 0;
            digits = 0;
            if (*p == '\0')
                break;
        } else {
            return 0;
        }
    }
    if (idx != 4)
        return 0;
    *out = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
    return 1;
}

void net_ip_format(uint32_t ip, char* buf)
{
    ksnprintf(buf, 16, "%u.%u.%u.%u",
              (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);
}

void net_mac_format(const uint8_t* mac, char* buf)
{
    ksnprintf(buf, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
              mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
