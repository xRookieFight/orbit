#pragma once
#include <stdint.h>

#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP  6
#define IP_PROTO_UDP  17

uint16_t ip_checksum(const void* data, uint16_t len);
int ip_send(uint32_t dst_ip, uint8_t proto, const void* payload, uint16_t len);
void ip_input(const uint8_t* src_mac, const uint8_t* data, uint16_t len);
