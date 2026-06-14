#pragma once
#include <stdint.h>

void fb_init(void);
int fb_ready(void);
int fb_width(void);
int fb_height(void);
uint32_t* fb_backbuffer(void);
void fb_flip(void);
