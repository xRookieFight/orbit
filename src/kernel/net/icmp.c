#include "icmp.h"
#include "ip.h"
#include "net.h"
#include "string.h"
#include "pit.h"

#define ICMP_ECHO_REPLY   0
#define ICMP_ECHO_REQUEST 8
#define ICMP_PING_ID      0x4F42
#define ICMP_PAYLOAD_LEN  32

typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
} icmp_header_t;

static volatile int await_active;
static volatile uint16_t await_id;
static volatile uint16_t await_seq;
static volatile int got_reply;

void icmp_input(uint32_t src_ip, const uint8_t* data, uint16_t len)
{
    if (len < (uint16_t)sizeof(icmp_header_t))
        return;
    const icmp_header_t* h = (const icmp_header_t*)data;

    if (h->type == ICMP_ECHO_REQUEST) {
        uint8_t reply[ETH_FRAME_MAX];
        if (len > sizeof(reply))
            return;
        memcpy(reply, data, len);
        icmp_header_t* rh = (icmp_header_t*)reply;
        rh->type = ICMP_ECHO_REPLY;
        rh->code = 0;
        rh->checksum = 0;
        rh->checksum = htons(ip_checksum(reply, len));
        ip_send(src_ip, IP_PROTO_ICMP, reply, len);
        return;
    }

    if (h->type == ICMP_ECHO_REPLY) {
        if (await_active && ntohs(h->id) == await_id && ntohs(h->seq) == await_seq)
            got_reply = 1;
    }
}

int icmp_ping(uint32_t ip, uint16_t seq, uint32_t* rtt_ms)
{
    uint8_t buf[sizeof(icmp_header_t) + ICMP_PAYLOAD_LEN];
    icmp_header_t* h = (icmp_header_t*)buf;
    h->type = ICMP_ECHO_REQUEST;
    h->code = 0;
    h->checksum = 0;
    h->id = htons(ICMP_PING_ID);
    h->seq = htons(seq);
    for (int i = 0; i < ICMP_PAYLOAD_LEN; i++)
        buf[sizeof(icmp_header_t) + i] = (uint8_t)('a' + (i % 26));
    h->checksum = htons(ip_checksum(buf, sizeof(buf)));

    await_id = ICMP_PING_ID;
    await_seq = seq;
    got_reply = 0;
    await_active = 1;

    uint32_t start = pit_ticks();
    int r = ip_send(ip, IP_PROTO_ICMP, buf, sizeof(buf));
    if (r != 0) {
        await_active = 0;
        return r == -2 ? -2 : -3;
    }

    for (int t = 0; t < 100; t++) {
        pit_sleep(10);
        if (got_reply) {
            uint32_t freq = pit_frequency();
            uint32_t now = pit_ticks();
            *rtt_ms = freq ? ((now - start) * 1000u) / freq : 0;
            await_active = 0;
            return 0;
        }
    }
    await_active = 0;
    return -1;
}
