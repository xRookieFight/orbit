#include "netcmd.h"
#include "app.h"
#include "api.h"
#include "net.h"
#include "arp.h"
#include "icmp.h"
#include "dhcp.h"
#include "pit.h"
#include "string.h"

static void print_iface(void)
{
    char ipbuf[16], maskbuf[16], gwbuf[16], macbuf[18];
    net_ip_format(net_ip(), ipbuf);
    net_ip_format(net_netmask(), maskbuf);
    net_ip_format(net_gateway(), gwbuf);
    net_mac_format(net_mac(), macbuf);

    const char* link;
    if (!net_has_device())
        link = "no device";
    else
        link = net_is_up() ? "up" : "down";

    api_print("eth0  link %s\n", link);
    api_print("      mac %s\n", macbuf);
    api_print("      ip %s  mask %s  gw %s\n", ipbuf, maskbuf, gwbuf);
    api_print("      RX %u pkts %u bytes  TX %u pkts %u bytes  drop %u\n",
              net_rx_packets(), net_rx_bytes(), net_tx_packets(), net_tx_bytes(),
              net_rx_dropped());
}

static int cmd_ifconfig(int argc, char** argv)
{
    if (argc < 2) {
        print_iface();
        return 0;
    }
    if (strcmp(argv[1], "up") == 0) {
        net_set_up(1);
        api_print("eth0 up\n");
        return 0;
    }
    if (strcmp(argv[1], "down") == 0) {
        net_set_up(0);
        api_print("eth0 down\n");
        return 0;
    }

    uint32_t val;
    if (strcmp(argv[1], "ip") == 0 && argc >= 3 && net_ip_parse(argv[2], &val)) {
        net_set_ip(val);
        api_print("ip set to %s\n", argv[2]);
        return 0;
    }
    if (strcmp(argv[1], "mask") == 0 && argc >= 3 && net_ip_parse(argv[2], &val)) {
        net_set_netmask(val);
        api_print("mask set to %s\n", argv[2]);
        return 0;
    }
    if (strcmp(argv[1], "gw") == 0 && argc >= 3 && net_ip_parse(argv[2], &val)) {
        net_set_gateway(val);
        api_print("gateway set to %s\n", argv[2]);
        return 0;
    }

    api_print("usage: ifconfig [up|down|ip <a>|mask <a>|gw <a>]\n");
    return 1;
}

static int cmd_ip(int argc, char** argv)
{
    if (argc >= 2 && strcmp(argv[1], "set") == 0) {
        uint32_t ip, mask, gw;
        if (argc < 3 || !net_ip_parse(argv[2], &ip)) {
            api_print("usage: ip set <ip> [mask] [gateway]\n");
            return 1;
        }
        net_set_ip(ip);
        if (argc >= 4 && net_ip_parse(argv[3], &mask))
            net_set_netmask(mask);
        if (argc >= 5 && net_ip_parse(argv[4], &gw))
            net_set_gateway(gw);
        api_print("configured\n");
        print_iface();
        return 0;
    }
    print_iface();
    return 0;
}

static int cmd_ping(int argc, char** argv)
{
    if (argc < 2) {
        api_print("usage: ping <ip> [count]\n");
        return 1;
    }
    if (!net_is_up()) {
        api_print("ping: network is down\n");
        return 1;
    }
    uint32_t ip;
    if (!net_ip_parse(argv[1], &ip)) {
        api_print("ping: invalid address %s\n", argv[1]);
        return 1;
    }
    int count = argc >= 3 ? atoi(argv[2]) : 4;
    if (count <= 0 || count > 100)
        count = 4;

    api_print("PING %s\n", argv[1]);
    int recv = 0;
    for (int i = 1; i <= count; i++) {
        uint32_t rtt = 0;
        int r = icmp_ping(ip, (uint16_t)i, &rtt);
        if (r == 0) {
            api_print("  reply from %s  seq=%d  time=%ums\n", argv[1], i, rtt);
            recv++;
        } else if (r == -2) {
            api_print("  seq=%d  host unreachable (no arp)\n", i);
        } else if (r == -1) {
            api_print("  seq=%d  request timed out\n", i);
        } else {
            api_print("  seq=%d  send failed\n", i);
        }
        if (i < count)
            pit_sleep(500);
    }
    api_print("--- %s ping statistics ---\n", argv[1]);
    api_print("%d sent, %d received, %d lost\n", count, recv, count - recv);
    return recv > 0 ? 0 : 1;
}

static int cmd_arp(int argc, char** argv)
{
    if (argc >= 2 && strcmp(argv[1], "-d") == 0) {
        arp_cache_clear();
        api_print("arp cache cleared\n");
        return 0;
    }
    int n = arp_cache_count();
    if (n == 0) {
        api_print("arp cache empty\n");
        return 0;
    }
    api_print("  ADDRESS          MAC\n");
    for (int i = 0; i < n; i++) {
        uint32_t ip;
        uint8_t mac[6];
        if (!arp_cache_entry(i, &ip, mac))
            break;
        char ipbuf[16], macbuf[18];
        net_ip_format(ip, ipbuf);
        net_mac_format(mac, macbuf);
        api_print("  %-15s  %s\n", ipbuf, macbuf);
    }
    return 0;
}

static int cmd_netstat(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    print_iface();
    return 0;
}

static int cmd_dhcp(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    if (!net_has_device()) {
        api_print("dhcp: no network device\n");
        return 1;
    }
    api_print("dhcp: requesting lease...\n");
    if (dhcp_configure() == 0) {
        api_print("dhcp: lease acquired\n");
        print_iface();
        return 0;
    }
    api_print("dhcp: no response (keeping static config)\n");
    return 1;
}

static const app_t net_apps[] = {
    { "ifconfig", "show or set interface (up/down/ip/mask/gw)", cmd_ifconfig },
    { "ip", "show or set ip config (ip set <ip> [mask] [gw])", cmd_ip },
    { "ping", "send ICMP echo (ping <ip> [count])", cmd_ping },
    { "arp", "show arp cache (-d to clear)", cmd_arp },
    { "netstat", "show network statistics", cmd_netstat },
    { "dhcp", "obtain ip via DHCP", cmd_dhcp },
};

void netcmd_register(void)
{
    int count = (int)(sizeof(net_apps) / sizeof(net_apps[0]));
    for (int i = 0; i < count; i++)
        app_register(&net_apps[i]);
}
