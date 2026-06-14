#pragma once
#include <stdint.h>

void mouse_init(int screen_w, int screen_h);
int mouse_x(void);
int mouse_y(void);
int mouse_buttons(void);
int mouse_consume_dirty(void);
