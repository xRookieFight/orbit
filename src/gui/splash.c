#include "splash.h"
#include "fb.h"
#include "gfx.h"
#include "theme.h"
#include "pit.h"
#include "orbit.h"
#include "logo_data.h"
#include <stdint.h>

#define BAR_W 360
#define BAR_H 10

static void draw_base(void)
{
    int w = fb_width();
    int h = fb_height();
    gfx_clip_reset();
    gfx_vgradient(0, 0, w, h, THEME_WALL_TOP, THEME_WALL_BOTTOM);

    int cx = w / 2;
    int cy = h / 2 - 90;
    gfx_blit_argb(cx - ORBIT_LOGO_W / 2, cy - ORBIT_LOGO_H / 2, ORBIT_LOGO_W, ORBIT_LOGO_H, orbit_logo_data);

    gfx_text_scaled(cx - 5 * 12, cy + 120, 0xFFE8EAF0, "ORBIT", 3, 1);
    gfx_text(cx - gfx_text_width(ORBIT_TAGLINE) / 2, cy + 178, THEME_TEXT_DIM, ORBIT_TAGLINE);
    gfx_text(cx - gfx_text_width("Made by Orbit Developers") / 2, h - 60, 0xFF55617A, "Made by Orbit Developers");
}

static void draw_bar(int filled)
{
    int w = fb_width();
    int h = fb_height();
    int bx = (w - BAR_W) / 2;
    int by = h / 2 + 130;

    gfx_rounded(bx, by, BAR_W, BAR_H, 5, 0xFF1A2336);
    if (filled > 8)
        gfx_rounded(bx, by, filled, BAR_H, 5, THEME_ACCENT);
    fb_flip();
}

void splash_show(void)
{
    if (!fb_ready())
        return;

    draw_base();
    draw_bar(0);
    pit_sleep(700);

    static const struct {
        int target;
        int step_ms;
        int pause_ms;
    } seg[] = {
        { 60, 6, 200 },
        { 90, 25, 350 },
        { 190, 8, 150 },
        { 215, 35, 500 },
        { 290, 7, 120 },
        { 300, 60, 600 },
        { 360, 4, 0 },
    };
    int seg_count = (int)(sizeof(seg) / sizeof(seg[0]));
    int filled = 0;
    for (int s = 0; s < seg_count; s++) {
        while (filled < seg[s].target) {
            filled += 4;
            if (filled > seg[s].target)
                filled = seg[s].target;
            draw_bar(filled);
            pit_sleep((uint32_t)seg[s].step_ms);
        }
        if (seg[s].pause_ms)
            pit_sleep((uint32_t)seg[s].pause_ms);
    }
    pit_sleep(250);
}
