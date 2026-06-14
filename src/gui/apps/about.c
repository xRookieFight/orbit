#include "appreg.h"
#include "gfx.h"
#include "theme.h"
#include "orbit.h"

static window_t* about_win;

static void about_draw(window_t* win, int ox, int oy)
{
    int cx = ox + win->w / 2;
    int cy = oy + 86;

    gfx_circle(cx, cy, 44, gfx_mix(THEME_WINDOW_BG, THEME_ACCENT, 200));
    gfx_circle(cx - 14, cy - 14, 14, gfx_mix(THEME_WINDOW_BG, 0xFFBFD6FF, 200));
    gfx_ring(cx, cy, 66, 4, gfx_mix(THEME_WINDOW_BG, THEME_ACCENT, 120));

    int y = cy + 86;
    gfx_text_scaled(cx - 5 * 16 / 2 * 2, y, THEME_TEXT, "Orbit", 2, 1);
    y += 40;
    gfx_text(cx - gfx_text_width("Version " ORBIT_VERSION " - x86_64") / 2, y, THEME_TEXT_DIM,
             "Version " ORBIT_VERSION " - x86_64");
    y += 22;
    gfx_text(cx - gfx_text_width(ORBIT_TAGLINE) / 2, y, THEME_TEXT_DIM, ORBIT_TAGLINE);
    y += 22;
    gfx_text(cx - gfx_text_width("Made by Orbit Developers") / 2, y, THEME_ACCENT, "Made by Orbit Developers");
}

static window_t* about_create(void)
{
    about_win = wm_create("About Orbit", 280, 180, 380, 330);
    about_win->on_draw = about_draw;
    return about_win;
}

REGISTER_GUI_APP("About Orbit", about_create);
