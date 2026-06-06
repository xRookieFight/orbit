#pragma once
#include <stdint.h>

void pit_init(uint32_t frequency);
uint32_t pit_ticks(void);
uint32_t pit_frequency(void);
void pit_sleep(uint32_t ms);
