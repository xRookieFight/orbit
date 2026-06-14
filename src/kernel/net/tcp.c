#include "tcp.h"
#include "ip.h"
#include "net.h"
#include "pit.h"
#include "string.h"

#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10

#define TCP_WINDOW 32768

enum {
    ST_CLOSED,
    ST_SYN_SENT,
    ST_ESTABLISHED,
    ST_DONE
};

typedef struct __attribute__((packed)) {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint32_t ack;
    uint8_t data_off;
    uint8_t flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urg;
} tcp_header_t;

static int state;
static uint32_t peer_ip;
static uint16_t peer_port;
static uint16_t local_port;
static uint32_t snd_nxt;
static uint32_t rcv_nxt;
static tcp_data_cb data_cb;
static tcp_event_cb event_cb;
static uint32_t syn_tick;
static int syn_retries;
static uint16_t port_seed = 40000;

static uint16_t tcp_checksum(const tcp_header_t* h, const uint8_t* data, uint16_t dlen, uint16_t hlen)
{
    uint32_t sum = 0;
    uint32_t src = net_ip();
    uint32_t dst = peer_ip;
    sum += (src >> 16) & 0xFFFF;
    sum += src & 0xFFFF;
    sum += (dst >> 16) & 0xFFFF;
    sum += dst & 0xFFFF;
    sum += IP_PROTO_TCP;
    sum += (uint16_t)(hlen + dlen);

    const uint8_t* p = (const uint8_t*)h;
    for (uint16_t i = 0; i + 1 < hlen; i += 2)
        sum += (uint32_t)((p[i] << 8) | p[i + 1]);
    if (hlen & 1)
        sum += (uint32_t)(p[hlen - 1] << 8);

    for (uint16_t i = 0; i + 1 < dlen; i += 2)
        sum += (uint32_t)((data[i] << 8) | data[i + 1]);
    if (dlen & 1)
        sum += (uint32_t)(data[dlen - 1] << 8);

    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
    return htons((uint16_t)~sum);
}

static void tcp_out(uint8_t flags, const uint8_t* data, uint16_t dlen, int with_mss)
{
    uint8_t buf[1500];
    tcp_header_t* h = (tcp_header_t*)buf;
    uint16_t hlen = 20;

    h->src_port = htons(local_port);
    h->dst_port = htons(peer_port);
    h->seq = htonl(snd_nxt);
    h->ack = htonl(rcv_nxt);
    h->flags = flags;
    h->window = htons(TCP_WINDOW);
    h->checksum = 0;
    h->urg = 0;

    if (with_mss) {
        buf[20] = 2;
        buf[21] = 4;
        buf[22] = (1460 >> 8) & 0xFF;
        buf[23] = 1460 & 0xFF;
        hlen = 24;
    }

    h->data_off = (uint8_t)((hlen / 4) << 4);

    if (dlen)
        memcpy(buf + hlen, data, dlen);

    h->checksum = tcp_checksum(h, buf + hlen, dlen, hlen);
    ip_send(peer_ip, IP_PROTO_TCP, buf, (uint16_t)(hlen + dlen));
}

int tcp_connect(uint32_t ip, uint16_t port, tcp_data_cb on_data, tcp_event_cb on_event)
{
    peer_ip = ip;
    peer_port = port;
    local_port = port_seed++;
    if (port_seed == 0)
        port_seed = 40000;
    data_cb = on_data;
    event_cb = on_event;
    snd_nxt = (pit_ticks() << 8) ^ 0x1F2E3D4C;
    rcv_nxt = 0;
    state = ST_SYN_SENT;
    syn_tick = pit_ticks();
    syn_retries = 0;

    tcp_out(TCP_SYN, 0, 0, 1);
    snd_nxt++;
    return 0;
}

int tcp_write(const void* data, uint16_t len)
{
    if (state != ST_ESTABLISHED)
        return -1;
    tcp_out(TCP_PSH | TCP_ACK, (const uint8_t*)data, len, 0);
    snd_nxt += len;
    return 0;
}

void tcp_close(void)
{
    if (state == ST_ESTABLISHED) {
        tcp_out(TCP_FIN | TCP_ACK, 0, 0, 0);
        snd_nxt++;
    }
    state = ST_DONE;
}

int tcp_active(void)
{
    return state == ST_SYN_SENT || state == ST_ESTABLISHED;
}

void tcp_input(uint32_t src_ip, const uint8_t* data, uint16_t len)
{
    if (state == ST_CLOSED || state == ST_DONE)
        return;
    if (len < 20 || src_ip != peer_ip)
        return;

    const tcp_header_t* h = (const tcp_header_t*)data;
    if (ntohs(h->dst_port) != local_port || ntohs(h->src_port) != peer_port)
        return;

    uint16_t hlen = (uint16_t)((h->data_off >> 4) * 4);
    if (hlen < 20 || hlen > len)
        return;

    uint32_t seg_seq = ntohl(h->seq);
    uint8_t flags = h->flags;
    uint16_t dlen = (uint16_t)(len - hlen);

    if (flags & TCP_RST) {
        state = ST_DONE;
        if (event_cb)
            event_cb(0);
        return;
    }

    if (state == ST_SYN_SENT) {
        if ((flags & TCP_SYN) && (flags & TCP_ACK)) {
            rcv_nxt = seg_seq + 1;
            state = ST_ESTABLISHED;
            tcp_out(TCP_ACK, 0, 0, 0);
            if (event_cb)
                event_cb(1);
        }
        return;
    }

    if (state == ST_ESTABLISHED) {
        if (dlen > 0) {
            if (seg_seq == rcv_nxt) {
                rcv_nxt += dlen;
                if (data_cb)
                    data_cb(data + hlen, dlen);
            }
            tcp_out(TCP_ACK, 0, 0, 0);
        }
        if (flags & TCP_FIN) {
            if (seg_seq + dlen == rcv_nxt)
                rcv_nxt++;
            tcp_out(TCP_FIN | TCP_ACK, 0, 0, 0);
            snd_nxt++;
            state = ST_DONE;
            if (event_cb)
                event_cb(0);
        }
    }
}

void tcp_poll(void)
{
    if (state != ST_SYN_SENT)
        return;
    uint32_t now = pit_ticks();
    if (now - syn_tick < 100)
        return;
    syn_tick = now;
    if (++syn_retries > 5) {
        state = ST_DONE;
        if (event_cb)
            event_cb(0);
        return;
    }
    snd_nxt--;
    tcp_out(TCP_SYN, 0, 0, 1);
    snd_nxt++;
}
