#pragma once
#include <stdint.h>

#define TERM_COLS 88
#define TERM_ROWS 26
#define TERM_PAD  8

void term_init(void);
void term_clear(void);
void term_putc(char c);
void term_set_color(uint8_t fg, uint8_t bg);
void term_scroll_view_up(void);
void term_scroll_view_down(void);
void term_feed_key(char c);
int term_read_key(void);
void term_draw(int ox, int oy);
int term_pixel_width(void);
int term_pixel_height(void);
int term_consume_dirty(void);
