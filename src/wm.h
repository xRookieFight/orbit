#pragma once
#include <stdint.h>

#define WM_MAX_WINDOWS 8
#define WM_TITLE_H 32

typedef struct window window_t;

struct window {
    char title[48];
    int x, y, w, h;
    int visible;
    void (*on_draw)(window_t* win, int ox, int oy);
    void (*on_key)(window_t* win, int ch);
    void (*on_click)(window_t* win, int cx, int cy);
    void (*on_tick)(window_t* win);
    void* user;
};

window_t* wm_create(const char* title, int x, int y, int w, int h);
void wm_show(window_t* win);
void wm_hide(window_t* win);
void wm_raise(window_t* win);
window_t* wm_focused(void);
int wm_count(void);
window_t* wm_at(int index);
window_t* wm_window(int index);
window_t* wm_hit(int mx, int my);
int wm_hit_title(window_t* win, int mx, int my);
int wm_hit_close(window_t* win, int mx, int my);
void wm_draw_all(void);
void wm_tick_all(void);
