#include "appreg.h"
#include "gfx.h"
#include "theme.h"
#include "fb.h"
#include "proc.h"
#include "pit.h"
#include "heap.h"
#include "fmt.h"
#include "orbit.h"
#include "api.h"
#include "desktop.h"

static window_t* sysinfo_win;

static void sysinfo_draw(window_t* win, int ox, int oy)
{
    int x = ox + 16;
    int y = oy + 14;
    char buf[96];

    gfx_text_bold(x, y, THEME_ACCENT, "System");
    y += 24;
    ksnprintf(buf, sizeof(buf), "%s  (build %s)", ORBIT_BANNER, ORBIT_BUILD);
    gfx_text(x, y, THEME_TEXT, buf);
    y += 20;
    gfx_text(x, y, THEME_TEXT, "Architecture: x86_64 long mode");
    y += 20;
    ksnprintf(buf, sizeof(buf), "Display: %dx%d, 32-bit", fb_width(), fb_height());
    gfx_text(x, y, THEME_TEXT, buf);
    y += 28;

    gfx_text_bold(x, y, THEME_ACCENT, "Uptime");
    y += 24;
    uint32_t s = api_uptime_ms() / 1000;
    ksnprintf(buf, sizeof(buf), "%u:%02u:%02u  (%u ticks)", s / 3600, (s / 60) % 60, s % 60, pit_ticks());
    gfx_text(x, y, THEME_TEXT, buf);
    y += 28;

    gfx_text_bold(x, y, THEME_ACCENT, "Memory");
    y += 24;
    unsigned used = (unsigned)heap_used();
    unsigned total = (unsigned)heap_total();
    ksnprintf(buf, sizeof(buf), "%u KB / %u KB", used / 1024, total / 1024);
    gfx_text(x, y, THEME_TEXT, buf);
    y += 22;
    int bar_w = win->w - 32;
    gfx_rounded(x, y, bar_w, 12, 6, 0xFF1A2030);
    int fill = (int)((uint64_t)bar_w * used / (total ? total : 1));
    if (fill > 6)
        gfx_rounded(x, y, fill, 12, 6, THEME_ACCENT);
    y += 30;

    gfx_text_bold(x, y, THEME_ACCENT, "Processes");
    y += 24;
    for (int i = 0; i < proc_count() && y < oy + win->h - 20; i++) {
        proc_t* p = proc_at(i);
        ksnprintf(buf, sizeof(buf), "%4d  %-8s  %s", p->pid, proc_state_name(p->state), p->name);
        gfx_text(x, y, THEME_TEXT_DIM, buf);
        y += 18;
    }
}

static void sysinfo_tick(window_t* win)
{
    (void)win;
    desktop_mark_dirty();
}

static window_t* sysinfo_create(void)
{
    sysinfo_win = wm_create("System Monitor", 200, 140, 440, 420);
    sysinfo_win->on_draw = sysinfo_draw;
    sysinfo_win->on_tick = sysinfo_tick;
    return sysinfo_win;
}

REGISTER_GUI_APP("System Monitor", sysinfo_create);
