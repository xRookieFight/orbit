#pragma once
#include <stdint.h>

void gfx_clip(int x, int y, int w, int h);
void gfx_clip_reset(void);

void gfx_pixel(int x, int y, uint32_t color);
void gfx_blend_pixel(int x, int y, uint32_t argb);
void gfx_fill(int x, int y, int w, int h, uint32_t color);
void gfx_blend_fill(int x, int y, int w, int h, uint32_t argb);
void gfx_rect(int x, int y, int w, int h, uint32_t color);
void gfx_hline(int x, int y, int w, uint32_t color);
void gfx_vline(int x, int y, int h, uint32_t color);
void gfx_vgradient(int x, int y, int w, int h, uint32_t top, uint32_t bottom);
void gfx_rounded(int x, int y, int w, int h, int radius, uint32_t color);
void gfx_rounded_blend(int x, int y, int w, int h, int radius, uint32_t argb);
void gfx_rounded_top(int x, int y, int w, int h, int radius, uint32_t color);
void gfx_shadow(int x, int y, int w, int h, int radius);
void gfx_circle(int cx, int cy, int r, uint32_t color);
void gfx_ring(int cx, int cy, int r, int thickness, uint32_t color);

void gfx_char(int x, int y, uint32_t color, char c);
void gfx_text(int x, int y, uint32_t color, const char* s);
void gfx_text_bold(int x, int y, uint32_t color, const char* s);
void gfx_text_scaled(int x, int y, uint32_t color, const char* s, int scale, int bold);
int gfx_text_width(const char* s);

uint32_t gfx_mix(uint32_t a, uint32_t b, int t256);
