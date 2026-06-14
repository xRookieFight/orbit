#include "dns.h"
#include "udp.h"
#include "net.h"
#include "pit.h"
#include "string.h"

#define DNS_PORT      53
#define DNS_LOCAL     50001
#define DNS_TIMEOUT   300

static int busy;
static dns_cb cb;
static uint16_t query_id;
static uint32_t start_tick;
static int retries;
static char pending[256];

static int encode_name(const char* host, uint8_t* out)
{
    int oi = 0;
    int li = 0;
    int len_pos = 0;
    out[oi++] = 0;
    for (const char* p = host;; p++) {
        if (*p == '.' || *p == 0) {
            out[len_pos] = (uint8_t)li;
            len_pos = oi;
            out[oi++] = 0;
            li = 0;
            if (*p == 0)
                break;
        } else {
            out[oi++] = (uint8_t)*p;
            li++;
        }
    }
    return oi;
}

static int skip_name(const uint8_t* msg, int len, int off)
{
    while (off < len) {
        uint8_t b = msg[off];
        if (b == 0)
            return off + 1;
        if ((b & 0xC0) == 0xC0)
            return off + 2;
        off += b + 1;
    }
    return len;
}

static void dns_handler(uint32_t src_ip, uint16_t src_port, const uint8_t* data, uint16_t len)
{
    (void)src_ip;
    (void)src_port;
    if (!busy || len < 12)
        return;
    uint16_t id = (uint16_t)((data[0] << 8) | data[1]);
    if (id != query_id)
        return;
    uint16_t qd = (uint16_t)((data[4] << 8) | data[5]);
    uint16_t an = (uint16_t)((data[6] << 8) | data[7]);
    int off = 12;
    for (int i = 0; i < qd; i++) {
        off = skip_name(data, len, off);
        off += 4;
    }
    for (int i = 0; i < an && off + 10 <= len; i++) {
        off = skip_name(data, len, off);
        if (off + 10 > len)
            break;
        uint16_t type = (uint16_t)((data[off] << 8) | data[off + 1]);
        uint16_t rdlen = (uint16_t)((data[off + 8] << 8) | data[off + 9]);
        int rdata = off + 10;
        if (type == 1 && rdlen == 4 && rdata + 4 <= len) {
            uint32_t ip = ((uint32_t)data[rdata] << 24) | ((uint32_t)data[rdata + 1] << 16) |
                          ((uint32_t)data[rdata + 2] << 8) | (uint32_t)data[rdata + 3];
            busy = 0;
            udp_unbind(DNS_LOCAL);
            if (cb)
                cb(ip);
            return;
        }
        off = rdata + rdlen;
    }
}

static void dns_send(void)
{
    uint8_t pkt[300];
    pkt[0] = (uint8_t)(query_id >> 8);
    pkt[1] = (uint8_t)query_id;
    pkt[2] = 0x01;
    pkt[3] = 0x00;
    pkt[4] = 0;
    pkt[5] = 1;
    pkt[6] = 0;
    pkt[7] = 0;
    pkt[8] = 0;
    pkt[9] = 0;
    pkt[10] = 0;
    pkt[11] = 0;
    int n = encode_name(pending, pkt + 12);
    int off = 12 + n;
    pkt[off++] = 0;
    pkt[off++] = 1;
    pkt[off++] = 0;
    pkt[off++] = 1;
    udp_send(net_dns(), DNS_LOCAL, DNS_PORT, pkt, (uint16_t)off);
}

int dns_resolve(const char* host, dns_cb on_done)
{
    if (busy)
        return -1;
    if (net_dns() == 0)
        return -1;
    uint32_t dummy;
    if (net_ip_parse(host, &dummy)) {
        if (on_done)
            on_done(dummy);
        return 0;
    }
    busy = 1;
    cb = on_done;
    query_id = (uint16_t)(pit_ticks() ^ 0xABCD);
    start_tick = pit_ticks();
    retries = 0;
    int i = 0;
    while (host[i] && i < 255) {
        pending[i] = host[i];
        i++;
    }
    pending[i] = 0;
    udp_bind(DNS_LOCAL, dns_handler);
    dns_send();
    return 0;
}

int dns_busy(void)
{
    return busy;
}

void dns_poll(void)
{
    if (!busy)
        return;
    if (pit_ticks() - start_tick < DNS_TIMEOUT)
        return;
    start_tick = pit_ticks();
    if (++retries > 3) {
        busy = 0;
        udp_unbind(DNS_LOCAL);
        if (cb)
            cb(0);
        return;
    }
    dns_send();
}
