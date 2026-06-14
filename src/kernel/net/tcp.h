#pragma once
#include <stdint.h>

typedef void (*tcp_data_cb)(const uint8_t* data, uint16_t len);
typedef void (*tcp_event_cb)(int connected);

int tcp_connect(uint32_t ip, uint16_t port, tcp_data_cb on_data, tcp_event_cb on_event);
int tcp_write(const void* data, uint16_t len);
void tcp_close(void);
int tcp_active(void);

void tcp_input(uint32_t src_ip, const uint8_t* data, uint16_t len);
void tcp_poll(void);
