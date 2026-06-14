#pragma once
#include <stdint.h>

void arp_init(void);
void arp_input(const uint8_t* data, uint16_t len);
void arp_request(uint32_t ip);
int arp_lookup(uint32_t ip, uint8_t mac_out[6]);
int arp_resolve(uint32_t ip, uint8_t mac_out[6]);
void arp_cache_clear(void);
int arp_cache_count(void);
int arp_cache_entry(int idx, uint32_t* ip, uint8_t mac_out[6]);
