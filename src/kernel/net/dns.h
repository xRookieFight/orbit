#pragma once
#include <stdint.h>

typedef void (*dns_cb)(uint32_t ip);

int dns_resolve(const char* host, dns_cb cb);
void dns_poll(void);
int dns_busy(void);
