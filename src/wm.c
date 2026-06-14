#include "wm.h"
#include "gfx.h"
#include "theme.h"
#include "string.h"

static window_t windows[WM_MAX_WINDOWS];
static window_t* zorder[WM_MAX_WINDOWS];
static int count;

window_t* wm_create(const char* title, int x, int y, int w, int h)
{
    if (count >= WM_MAX_WINDOWS)
        return 0;
    window_t* win = &windows[count];
    memset(win, 0, sizeof(*win));
    strlcpy(win->title, title, sizeof(win->title));
    win->x = x;
    win->y = y;
    win->w = w;
    win->h = h;
    zorder[count] = win;
    count++;
    return win;
}

static int zindex(window_t* win)
{
    for (int i = 0; i < count; i++)
        if (zorder[i] == win)
            return i;
    return -1;
}

void wm_raise(window_t* win)
{
    int i = zindex(win);
    if (i < 0)
        return;
    for (; i < count - 1; i++)
        zorder[i] = zorder[i + 1];
    zorder[count - 1] = win;
}

void wm_show(window_t* win)
{
    win->visible = 1;
    wm_raise(win);
}

void wm_hide(window_t* win)
{
    win->visible = 0;
}

window_t* wm_focused(void)
{
    for (int i = count - 1; i >= 0; i--)
        if (zorder[i]->visible)
            return zorder[i];
    return 0;
}

int wm_count(void)
{
    return count;
}

window_t* wm_at(int index)
{
    if (index < 0 || index >= count)
        return 0;
    return zorder[index];
}

window_t* wm_window(int index)
{
    if (index < 0 || index >= count)
        return 0;
    return &windows[index];
}

window_t* wm_hit(int mx, int my)
{
    for (int i = count - 1; i >= 0; i--) {
        window_t* w = zorder[i];
        if (!w->visible)
            continue;
        if (mx >= w->x && mx < w->x + w->w &&
            my >= w->y && my < w->y + w->h + WM_TITLE_H)
            return w;
    }
    return 0;
}

int wm_hit_title(window_t* win, int mx, int my)
{
    return mx >= win->x && mx < win->x + win->w &&
           my >= win->y && my < win->y + WM_TITLE_H;
}

int wm_hit_close(window_t* win, int mx, int my)
{
    int bx = win->x + win->w - 44;
    return mx >= bx && mx < win->x + win->w &&
           my >= win->y && my < win->y + WM_TITLE_H;
}

static void draw_window(window_t* win, int focused)
{
    int total_h = win->h + WM_TITLE_H;

    gfx_shadow(win->x, win->y, win->w, total_h, 8);

    gfx_rect(win->x - 1, win->y - 1, win->w + 2, total_h + 2, THEME_BORDER);

    if (focused)
        gfx_vgradient(win->x, win->y, win->w, WM_TITLE_H, THEME_TITLE_TOP, THEME_TITLE_BOTTOM);
    else
        gfx_fill(win->x, win->y, win->w, WM_TITLE_H, THEME_TITLE_IDLE);
    gfx_rounded_top(win->x, win->y, win->w, 8, 8, focused ? THEME_TITLE_TOP : THEME_TITLE_IDLE);

    if (focused)
        gfx_fill(win->x, win->y + WM_TITLE_H - 2, win->w, 2, THEME_ACCENT);

    gfx_circle(win->x + 16, win->y + WM_TITLE_H / 2, 5, focused ? THEME_ACCENT : THEME_TEXT_DIM);
    gfx_text_bold(win->x + 30, win->y + (WM_TITLE_H - 16) / 2,
                  focused ? THEME_TEXT : THEME_TEXT_DIM, win->title);

    int cx = win->x + win->w - 22;
    int cy = win->y + WM_TITLE_H / 2;
    gfx_circle(cx, cy, 8, focused ? THEME_CLOSE : 0xFF6B4A4A);
    for (int i = -3; i <= 3; i++) {
        gfx_pixel(cx + i, cy + i, 0xFFFFFFFF);
        gfx_pixel(cx + i, cy - i, 0xFFFFFFFF);
    }

    gfx_fill(win->x, win->y + WM_TITLE_H, win->w, win->h, THEME_WINDOW_BG);

    if (win->on_draw) {
        gfx_clip(win->x, win->y + WM_TITLE_H, win->w, win->h);
        win->on_draw(win, win->x, win->y + WM_TITLE_H);
        gfx_clip_reset();
    }
}

void wm_draw_all(void)
{
    window_t* top = wm_focused();
    for (int i = 0; i < count; i++) {
        window_t* w = zorder[i];
        if (w->visible)
            draw_window(w, w == top);
    }
}

void wm_tick_all(void)
{
    for (int i = 0; i < count; i++)
        if (zorder[i]->visible && zorder[i]->on_tick)
            zorder[i]->on_tick(zorder[i]);
}
