#pragma once
#include <stdint.h>

void icmp_input(uint32_t src_ip, const uint8_t* data, uint16_t len);
int icmp_ping(uint32_t ip, uint16_t seq, uint32_t* rtt_ms);
