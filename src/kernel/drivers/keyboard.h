#pragma once

#define KEY_PAGE_UP   0x101
#define KEY_PAGE_DOWN 0x102

void keyboard_init(void);
int keyboard_poll(void);
