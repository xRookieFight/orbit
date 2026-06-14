#pragma once
#include <stdint.h>

typedef void (*udp_handler_t)(uint32_t src_ip, uint16_t src_port, const uint8_t* data, uint16_t len);

void udp_init(void);
int udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port, const void* data, uint16_t len);
void udp_input(uint32_t src_ip, const uint8_t* data, uint16_t len);
int udp_bind(uint16_t port, udp_handler_t handler);
void udp_unbind(uint16_t port);
