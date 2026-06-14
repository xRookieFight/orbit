#include "dhcp.h"
#include "udp.h"
#include "net.h"
#include "string.h"
#include "pit.h"

#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67
#define DHCP_OP_REQUEST  1
#define DHCP_MSG_DISCOVER 1
#define DHCP_MSG_OFFER    2
#define DHCP_MSG_REQUEST  3
#define DHCP_MSG_ACK      5

typedef struct __attribute__((packed)) {
    uint8_t op;
    uint8_t htype;
    uint8_t hlen;
    uint8_t hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint8_t ciaddr[4];
    uint8_t yiaddr[4];
    uint8_t siaddr[4];
    uint8_t giaddr[4];
    uint8_t chaddr[16];
    uint8_t sname[64];
    uint8_t file[128];
    uint8_t cookie[4];
} dhcp_msg_t;

static volatile int dhcp_state;
static uint32_t cur_xid;
static uint32_t lease_ip;
static uint32_t lease_server;
static uint32_t lease_mask;
static uint32_t lease_gw;

static uint32_t pack_ip(const uint8_t* b)
{
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8) | (uint32_t)b[3];
}

static const uint8_t* opt_find(const uint8_t* opt, uint16_t len, uint8_t code, uint8_t* opt_len)
{
    uint16_t i = 0;
    while (i < len) {
        uint8_t c = opt[i];
        if (c == 0xFF)
            break;
        if (c == 0) {
            i++;
            continue;
        }
        if (i + 1 >= len)
            break;
        uint8_t l = opt[i + 1];
        if (i + 2 + l > len)
            break;
        if (c == code) {
            *opt_len = l;
            return &opt[i + 2];
        }
        i = (uint16_t)(i + 2 + l);
    }
    return 0;
}

static void dhcp_handler(uint32_t src_ip, uint16_t src_port, const uint8_t* data, uint16_t len)
{
    (void)src_ip;
    (void)src_port;
    if (len < (uint16_t)sizeof(dhcp_msg_t))
        return;
    const dhcp_msg_t* msg = (const dhcp_msg_t*)data;
    if (msg->xid != cur_xid)
        return;

    const uint8_t* opt = data + sizeof(dhcp_msg_t);
    uint16_t opt_len = (uint16_t)(len - sizeof(dhcp_msg_t));
    uint8_t l;

    const uint8_t* type = opt_find(opt, opt_len, 53, &l);
    if (!type)
        return;

    if (*type == DHCP_MSG_OFFER) {
        lease_ip = pack_ip(msg->yiaddr);
        const uint8_t* sid = opt_find(opt, opt_len, 54, &l);
        lease_server = sid ? pack_ip(sid) : 0;
        const uint8_t* mask = opt_find(opt, opt_len, 1, &l);
        lease_mask = mask ? pack_ip(mask) : 0xFFFFFF00u;
        const uint8_t* gw = opt_find(opt, opt_len, 3, &l);
        lease_gw = gw ? pack_ip(gw) : 0;
        dhcp_state = 1;
    } else if (*type == DHCP_MSG_ACK) {
        const uint8_t* mask = opt_find(opt, opt_len, 1, &l);
        if (mask)
            lease_mask = pack_ip(mask);
        const uint8_t* gw = opt_find(opt, opt_len, 3, &l);
        if (gw)
            lease_gw = pack_ip(gw);
        const uint8_t* dns = opt_find(opt, opt_len, 6, &l);
        if (dns && l >= 4)
            net_set_dns(pack_ip(dns));
        dhcp_state = 2;
    }
}

static void fill_header(dhcp_msg_t* msg)
{
    memset(msg, 0, sizeof(*msg));
    msg->op = DHCP_OP_REQUEST;
    msg->htype = 1;
    msg->hlen = 6;
    msg->xid = cur_xid;
    msg->flags = htons(0x8000);
    memcpy(msg->chaddr, net_mac(), 6);
    msg->cookie[0] = 99;
    msg->cookie[1] = 130;
    msg->cookie[2] = 83;
    msg->cookie[3] = 99;
}

static void put_ip(uint8_t* dst, uint32_t ip)
{
    dst[0] = (uint8_t)(ip >> 24);
    dst[1] = (uint8_t)(ip >> 16);
    dst[2] = (uint8_t)(ip >> 8);
    dst[3] = (uint8_t)ip;
}

void dhcp_init(void)
{
    dhcp_state = 0;
}

int dhcp_configure(void)
{
    if (!net_is_up())
        return -1;

    uint32_t saved_ip = net_ip();
    uint32_t saved_mask = net_netmask();
    uint32_t saved_gw = net_gateway();

    cur_xid = (pit_ticks() << 8) ^ 0x4F524254u;
    dhcp_state = 0;
    udp_bind(DHCP_CLIENT_PORT, dhcp_handler);
    net_set_ip(0);

    uint8_t buf[sizeof(dhcp_msg_t) + 64];
    dhcp_msg_t* msg = (dhcp_msg_t*)buf;
    fill_header(msg);
    uint8_t* o = buf + sizeof(dhcp_msg_t);
    int n = 0;
    o[n++] = 53; o[n++] = 1; o[n++] = DHCP_MSG_DISCOVER;
    o[n++] = 55; o[n++] = 2; o[n++] = 1; o[n++] = 3;
    o[n++] = 0xFF;

    udp_send(0xFFFFFFFFu, DHCP_CLIENT_PORT, DHCP_SERVER_PORT, buf, (uint16_t)(sizeof(dhcp_msg_t) + n));

    int offered = 0;
    for (int t = 0; t < 200; t++) {
        pit_sleep(10);
        if (dhcp_state == 1) {
            offered = 1;
            break;
        }
    }
    if (!offered) {
        udp_unbind(DHCP_CLIENT_PORT);
        net_set_ip(saved_ip);
        return -1;
    }

    fill_header(msg);
    o = buf + sizeof(dhcp_msg_t);
    n = 0;
    o[n++] = 53; o[n++] = 1; o[n++] = DHCP_MSG_REQUEST;
    o[n++] = 50; o[n++] = 4; put_ip(&o[n], lease_ip); n += 4;
    o[n++] = 54; o[n++] = 4; put_ip(&o[n], lease_server); n += 4;
    o[n++] = 55; o[n++] = 2; o[n++] = 1; o[n++] = 3;
    o[n++] = 0xFF;

    dhcp_state = 1;
    udp_send(0xFFFFFFFFu, DHCP_CLIENT_PORT, DHCP_SERVER_PORT, buf, (uint16_t)(sizeof(dhcp_msg_t) + n));

    int acked = 0;
    for (int t = 0; t < 200; t++) {
        pit_sleep(10);
        if (dhcp_state == 2) {
            acked = 1;
            break;
        }
    }
    udp_unbind(DHCP_CLIENT_PORT);

    if (!acked) {
        net_set_ip(saved_ip);
        net_set_netmask(saved_mask);
        net_set_gateway(saved_gw);
        return -1;
    }

    net_set_ip(lease_ip);
    net_set_netmask(lease_mask);
    net_set_gateway(lease_gw);
    return 0;
}
