#include "desktop.h"
#include "fb.h"
#include "gfx.h"
#include "theme.h"
#include "term.h"
#include "wm.h"
#include "mouse.h"
#include "keyboard.h"
#include "serial.h"
#include "rtc.h"
#include "pit.h"
#include "heap.h"
#include "power.h"
#include "fmt.h"
#include "string.h"
#include "orbit.h"
#include "ata.h"
#include "diskmap.h"
#include "appreg.h"
#include "console.h"
#include "apps/browser.h"
#include "dns.h"
#include "tcp.h"

#define TASKBAR_H 44
#define MENU_W 250
#define MENU_ITEM_H 36

static int menu_count(void)
{
    return 1 + appreg_count() + 3;
}

static const char* menu_label(int i, int* is_sep, int* is_power)
{
    int c = appreg_count();
    *is_sep = 0;
    *is_power = 0;
    if (i == 0)
        return "Terminal";
    if (i <= c)
        return appreg_name(i - 1);
    if (i == c + 1) {
        *is_sep = 1;
        return "";
    }
    *is_power = 1;
    return (i == c + 2) ? "Reboot" : "Shut Down";
}

#define CURSOR_W 11
#define CURSOR_H 17

static uint32_t* wallpaper;
static uint32_t* scene;
static int cur_px = -1;
static int cur_py = -1;
static int last_mx = -1;
static int last_my = -1;
static window_t* term_win;
static int active;
static int dirty;
static int shell_requested;
static int start_open;
static int dragging;
static window_t* drag_win;
static int drag_dx;
static int drag_dy;
static int prev_buttons;
static uint32_t last_blink;
static int last_second;
static uint32_t last_draw_tick;

static const char* cursor_img[] = {
    "X..........",
    "XX.........",
    "X#X........",
    "X##X.......",
    "X###X......",
    "X####X.....",
    "X#####X....",
    "X######X...",
    "X#######X..",
    "X########X.",
    "X#####XXXXX",
    "X##X##X....",
    "X#X.X##X...",
    "XX..X##X...",
    "X....X##X..",
    ".....X##X..",
    "......XX...",
};

window_t* desktop_terminal(void)
{
    return term_win;
}

void desktop_mark_dirty(void)
{
    dirty = 1;
}

int desktop_active(void)
{
    return active;
}

int desktop_shell_requested(void)
{
    return shell_requested;
}

static void desktop_open_terminal(void)
{
    wm_show(term_win);
    shell_requested = 1;
}

static void wallpaper_paint_procedural(int w, int h)
{
    for (int y = 0; y < h; y++) {
        uint32_t row;
        if (y < h / 2)
            row = gfx_mix(THEME_WALL_TOP, THEME_WALL_MID, (y * 256) / (h / 2));
        else
            row = gfx_mix(THEME_WALL_MID, THEME_WALL_BOTTOM, ((y - h / 2) * 256) / (h / 2));
        for (int x = 0; x < w; x++)
            wallpaper[(size_t)y * (size_t)w + (size_t)x] = row;
    }

    uint32_t* save = fb_backbuffer();
    memcpy(save, wallpaper, (size_t)w * (size_t)h * 4);

    int cx = w / 2;
    int cy = h / 2 - 40;
    gfx_clip_reset();
    gfx_circle(cx, cy, 70, gfx_mix(THEME_WALL_MID, THEME_ACCENT, 60));
    gfx_circle(cx - 22, cy - 22, 24, gfx_mix(THEME_WALL_MID, THEME_ACCENT, 100));
    gfx_ring(cx, cy, 120, 5, gfx_mix(THEME_WALL_MID, THEME_ACCENT, 90));
    gfx_ring(cx, cy, 150, 2, gfx_mix(THEME_WALL_MID, THEME_ACCENT, 45));

    memcpy(wallpaper, save, (size_t)w * (size_t)h * 4);
}

static int wallpaper_load_disk(int w, int h)
{
    static uint8_t hdr[512];
    if (ata_read(WALL_LBA, 1, hdr) != 0)
        return 0;
    uint32_t magic = *(uint32_t*)hdr;
    uint32_t sw = *(uint32_t*)(hdr + 4);
    uint32_t sh = *(uint32_t*)(hdr + 8);
    if (magic != WALL_MAGIC || sw == 0 || sh == 0 || sw > 4096 || sh > 4096)
        return 0;

    size_t bytes = (size_t)sw * (size_t)sh * 4;
    uint32_t sectors = (uint32_t)((bytes + 511) / 512);
    uint32_t* src = (uint32_t*)kmalloc((size_t)sectors * 512);
    if (!src)
        return 0;
    if (ata_read(WALL_LBA + 1, sectors, src) != 0) {
        kfree(src);
        return 0;
    }

    for (int y = 0; y < h; y++) {
        uint32_t sy = (uint32_t)y * sh / (uint32_t)h;
        for (int x = 0; x < w; x++) {
            uint32_t sx = (uint32_t)x * sw / (uint32_t)w;
            wallpaper[(size_t)y * (size_t)w + (size_t)x] = src[(size_t)sy * (size_t)sw + (size_t)sx];
        }
    }
    kfree(src);
    return 1;
}

static void wallpaper_build(void)
{
    int w = fb_width();
    int h = fb_height();
    wallpaper = (uint32_t*)kmalloc((size_t)w * (size_t)h * 4);
    if (!wallpaper)
        return;

    if (!wallpaper_load_disk(w, h))
        wallpaper_paint_procedural(w, h);

    uint32_t* save = fb_backbuffer();
    memcpy(save, wallpaper, (size_t)w * (size_t)h * 4);

    int cx = w / 2;
    int cy = h / 2 - 40;
    gfx_clip_reset();
    gfx_text_scaled(cx - 5 * 16, cy + 170, gfx_mix(THEME_WALL_MID, 0xFFFFFFFF, 140), "ORBIT", 4, 1);
    gfx_text(cx - gfx_text_width(ORBIT_TAGLINE) / 2, cy + 240,
             gfx_mix(THEME_WALL_MID, 0xFFFFFFFF, 80), ORBIT_TAGLINE);

    memcpy(wallpaper, save, (size_t)w * (size_t)h * 4);
}

static void term_on_draw(window_t* win, int ox, int oy)
{
    (void)win;
    term_draw(ox, oy);
}

static void term_on_key(window_t* win, int ch)
{
    (void)win;
    if (ch == KEY_PAGE_UP)
        term_scroll_view_up();
    else if (ch == KEY_PAGE_DOWN)
        term_scroll_view_down();
    else
        term_feed_key((char)ch);
}

static void menu_rect(int* x, int* y, int* w, int* h)
{
    *w = MENU_W;
    *h = 64 + menu_count() * MENU_ITEM_H + 12;
    *x = 8;
    *y = fb_height() - TASKBAR_H - *h - 8;
}

static void menu_action(int index)
{
    int c = appreg_count();
    if (index == 0)
        desktop_open_terminal();
    else if (index <= c)
        wm_show(appreg_window(index - 1));
    else if (index == c + 2)
        power_reboot();
    else if (index == c + 3)
        power_off();
}

static void draw_taskbar(void)
{
    int w = fb_width();
    int h = fb_height();
    int by = h - TASKBAR_H;

    gfx_blend_fill(0, by, w, TASKBAR_H, THEME_TASKBAR);
    gfx_hline(0, by, w, 0xFF1C2331);

    int mx = mouse_x();
    int my = mouse_y();
    int hover = my >= by && mx >= 8 && mx < 108;
    if (start_open || hover)
        gfx_rounded(8, by + 6, 100, TASKBAR_H - 12, 6, start_open ? THEME_ACCENT_DARK : 0xFF2A3142);
    gfx_circle(28, by + TASKBAR_H / 2, 8, THEME_ACCENT);
    gfx_circle(25, by + TASKBAR_H / 2 - 3, 3, 0xFFBFD6FF);
    gfx_text_bold(44, by + (TASKBAR_H - 16) / 2, THEME_TEXT, "Orbit");

    int bx = 124;
    window_t* focused = wm_focused();
    for (int i = 0; i < wm_count(); i++) {
        window_t* win = wm_window(i);
        int bw = 150;
        uint32_t bg = win->visible ? (win == focused ? 0xFF323B50 : 0xFF242B3A) : 0xFF1A2030;
        gfx_rounded(bx, by + 6, bw, TASKBAR_H - 12, 6, bg);
        if (win->visible && win == focused)
            gfx_fill(bx + 8, by + TASKBAR_H - 9, bw - 16, 2, THEME_ACCENT);
        char label[19];
        strlcpy(label, win->title, sizeof(label));
        gfx_text(bx + 12, by + (TASKBAR_H - 16) / 2, win->visible ? THEME_TEXT : THEME_TEXT_DIM, label);
        bx += bw + 8;
    }

    rtc_time_t t;
    rtc_now(&t);
    char clock[16];
    char date[16];
    ksnprintf(clock, sizeof(clock), "%02d:%02d:%02d", t.hour, t.minute, t.second);
    ksnprintf(date, sizeof(date), "%02d/%02d/%d", t.day, t.month, t.year);
    gfx_text_bold(w - 92, by + 5, THEME_TEXT, clock);
    gfx_text(w - 92, by + 23, THEME_TEXT_DIM, date);
}

static void draw_start_menu(void)
{
    int x, y, w, h;
    menu_rect(&x, &y, &w, &h);

    gfx_shadow(x, y, w, h, 10);
    gfx_rounded_blend(x, y, w, h, 10, THEME_MENU_BG);
    gfx_rect(x, y, w, h, THEME_BORDER);

    gfx_circle(x + 28, y + 30, 12, THEME_ACCENT);
    gfx_circle(x + 23, y + 25, 4, 0xFFBFD6FF);
    gfx_text_scaled(x + 50, y + 14, THEME_TEXT, "Orbit", 2, 1);
    gfx_text(x + 50, y + 44, THEME_TEXT_DIM, ORBIT_BANNER);
    gfx_hline(x + 12, y + 62, w - 24, 0xFF2A3142);

    int my = mouse_y();
    int mx = mouse_x();
    int iy = y + 64 + 6;
    int total = menu_count();
    for (int i = 0; i < total; i++) {
        int is_sep, is_power;
        const char* label = menu_label(i, &is_sep, &is_power);
        if (is_sep) {
            gfx_hline(x + 12, iy + MENU_ITEM_H / 2, w - 24, 0xFF2A3142);
        } else {
            int hover = mx >= x && mx < x + w && my >= iy && my < iy + MENU_ITEM_H;
            if (hover)
                gfx_rounded_blend(x + 6, iy + 2, w - 12, MENU_ITEM_H - 4, 6, THEME_MENU_HOVER);
            gfx_fill(x + 18, iy + MENU_ITEM_H / 2 - 4, 8, 8, is_power ? THEME_CLOSE : THEME_ACCENT);
            gfx_text(x + 40, iy + (MENU_ITEM_H - 16) / 2, THEME_TEXT, label);
        }
        iy += MENU_ITEM_H;
    }
}

static void draw_cursor(void)
{
    int mx = mouse_x();
    int my = mouse_y();
    for (int row = 0; row < 17; row++) {
        for (int col = 0; col < 11; col++) {
            char c = cursor_img[row][col];
            if (c == 'X')
                gfx_pixel(mx + col, my + row, 0xFF101010);
            else if (c == '#')
                gfx_pixel(mx + col, my + row, 0xFFF5F5F5);
        }
    }
}

static void scene_compose(void)
{
    int w = fb_width();
    int h = fb_height();
    uint32_t* back = fb_backbuffer();
    if (wallpaper)
        memcpy(back, wallpaper, (size_t)w * (size_t)h * 4);
    else
        gfx_fill(0, 0, w, h, THEME_WALL_BOTTOM);
    gfx_clip_reset();
    wm_draw_all();
    draw_taskbar();
    if (start_open)
        draw_start_menu();
    if (scene)
        memcpy(scene, back, (size_t)w * (size_t)h * 4);
}

static void present_full(void)
{
    scene_compose();
    draw_cursor();
    cur_px = mouse_x();
    cur_py = mouse_y();
    fb_flip();
}

static void present_cursor(void)
{
    int w = fb_width();
    int h = fb_height();
    uint32_t* back = fb_backbuffer();

    if (scene && cur_px > -CURSOR_W && cur_py > -CURSOR_H) {
        for (int row = 0; row < CURSOR_H; row++) {
            int yy = cur_py + row;
            if (yy < 0 || yy >= h)
                continue;
            int x0 = cur_px < 0 ? 0 : cur_px;
            int x1 = cur_px + CURSOR_W;
            if (x1 > w)
                x1 = w;
            if (x1 > x0)
                memcpy(back + (size_t)yy * (size_t)w + x0,
                       scene + (size_t)yy * (size_t)w + x0,
                       (size_t)(x1 - x0) * 4);
        }
        fb_flip_rect(cur_px, cur_py, CURSOR_W, CURSOR_H);
    }

    draw_cursor();
    fb_flip_rect(mouse_x(), mouse_y(), CURSOR_W, CURSOR_H);
    cur_px = mouse_x();
    cur_py = mouse_y();
}

static void taskbar_click(int mx)
{
    if (mx >= 8 && mx < 108) {
        start_open = !start_open;
        dirty = 1;
        return;
    }
    int bx = 124;
    for (int i = 0; i < wm_count(); i++) {
        window_t* win = wm_window(i);
        if (mx >= bx && mx < bx + 150) {
            if (win == term_win && !win->visible) {
                desktop_open_terminal();
            } else if (win->visible && win == wm_focused()) {
                wm_hide(win);
            } else {
                wm_show(win);
            }
            dirty = 1;
            return;
        }
        bx += 158;
    }
}

static void menu_click(int mx, int my)
{
    int x, y, w, h;
    menu_rect(&x, &y, &w, &h);
    if (mx < x || mx >= x + w || my < y || my >= y + h) {
        start_open = 0;
        dirty = 1;
        return;
    }
    int iy = y + 64 + 6;
    int total = menu_count();
    for (int i = 0; i < total; i++) {
        int is_sep, is_power;
        menu_label(i, &is_sep, &is_power);
        if (!is_sep && my >= iy && my < iy + MENU_ITEM_H) {
            start_open = 0;
            dirty = 1;
            menu_action(i);
            return;
        }
        iy += MENU_ITEM_H;
    }
}

static void clamp_window(window_t* win)
{
    int w = fb_width();
    int h = fb_height();
    if (win->x < -win->w + 60)
        win->x = -win->w + 60;
    if (win->x > w - 60)
        win->x = w - 60;
    if (win->y < 0)
        win->y = 0;
    if (win->y > h - TASKBAR_H - WM_TITLE_H)
        win->y = h - TASKBAR_H - WM_TITLE_H;
}

static void handle_mouse(void)
{
    int buttons = mouse_buttons();
    int pressed = buttons & ~prev_buttons;
    int released = prev_buttons & ~buttons;
    prev_buttons = buttons;

    int mx = mouse_x();
    int my = mouse_y();

    if (pressed & 1) {
        if (my >= fb_height() - TASKBAR_H) {
            taskbar_click(mx);
        } else if (start_open) {
            menu_click(mx, my);
        } else {
            window_t* win = wm_hit(mx, my);
            if (win) {
                wm_show(win);
                dirty = 1;
                if (wm_hit_close(win, mx, my)) {
                    wm_hide(win);
                } else if (wm_hit_title(win, mx, my)) {
                    dragging = 1;
                    drag_win = win;
                    drag_dx = mx - win->x;
                    drag_dy = my - win->y;
                } else if (win->on_click) {
                    win->on_click(win, mx - win->x, my - win->y - WM_TITLE_H);
                    dirty = 1;
                }
            }
        }
    }

    if (dragging && (buttons & 1)) {
        if (drag_win->x != mx - drag_dx || drag_win->y != my - drag_dy) {
            drag_win->x = mx - drag_dx;
            drag_win->y = my - drag_dy;
            clamp_window(drag_win);
            dirty = 1;
        }
    }

    if (released & 1)
        dragging = 0;
}

static void route_key(int c)
{
    window_t* win = wm_focused();
    if (win && win->on_key)
        win->on_key(win, c);
}

static int desktop_console_active(void)
{
    return desktop_active();
}

static void desktop_console_clear(void)
{
    term_clear();
}

static void desktop_console_set_color(uint8_t fg, uint8_t bg)
{
    term_set_color(fg, bg);
}

static void desktop_console_putc(char c)
{
    term_putc(c);
}

static int desktop_console_read_key(void)
{
    return term_read_key();
}

static const console_backend_t desktop_console = {
    desktop_console_active,
    desktop_console_clear,
    desktop_console_set_color,
    desktop_console_putc,
    desktop_console_read_key,
    desktop_pump
};

void desktop_pump(void)
{
    if (!active)
        return;

    int c;
    while ((c = keyboard_poll()) >= 0)
        route_key((char)c);
    while (serial_has_input())
        term_feed_key(serial_getc());

    handle_mouse();

    dns_poll();
    tcp_poll();
    browser_poll();

    uint32_t t = pit_ticks();
    if (t / 50 != last_blink) {
        last_blink = t / 50;
        dirty = 1;
    }

    rtc_time_t now;
    rtc_now(&now);
    if (now.second != last_second) {
        last_second = now.second;
        wm_tick_all();
        dirty = 1;
    }

    if (term_consume_dirty())
        dirty = 1;
    mouse_consume_dirty();

    int mx = mouse_x();
    int my = mouse_y();

    if (dirty && t != last_draw_tick) {
        present_full();
        dirty = 0;
        last_draw_tick = t;
        last_mx = mx;
        last_my = my;
    } else if (mx != last_mx || my != last_my) {
        present_cursor();
        last_mx = mx;
        last_my = my;
    }
}

void desktop_init(void)
{
    term_init();
    wallpaper_build();
    scene = (uint32_t*)kmalloc((size_t)fb_width() * (size_t)fb_height() * 4);

    int tw = term_pixel_width();
    int th = term_pixel_height();
    term_win = wm_create("Terminal", (fb_width() - tw) / 2, 56, tw, th);
    term_win->on_draw = term_on_draw;
    term_win->on_key = term_on_key;

    appreg_init_all();

    mouse_init(fb_width(), fb_height());

    active = 1;
    console_bind_backend(&desktop_console);
    pit_set_idle(desktop_pump);
    dirty = 1;
    present_full();
}
