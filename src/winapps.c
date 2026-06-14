#include "winapps.h"
#include "gfx.h"
#include "theme.h"
#include "fb.h"
#include "fs.h"
#include "heap.h"
#include "proc.h"
#include "pit.h"
#include "rtc.h"
#include "fmt.h"
#include "string.h"
#include "orbit.h"
#include "desktop.h"
#include "api.h"

#define FILES_ROW_H 24
#define FILES_HEADER 36

static window_t* files_win;
static window_t* sysinfo_win;
static window_t* about_win;
static fs_node_t* files_cwd;

window_t* winapps_files(void)
{
    return files_win;
}

window_t* winapps_sysinfo(void)
{
    return sysinfo_win;
}

window_t* winapps_about(void)
{
    return about_win;
}

static int files_visible_rows(void)
{
    return (files_win->h - FILES_HEADER - 8) / FILES_ROW_H;
}

static fs_node_t* files_entry_at(int index)
{
    if (files_cwd->parent) {
        if (index == 0)
            return files_cwd->parent;
        index--;
    }
    fs_node_t* child = files_cwd->children;
    while (child && index > 0) {
        child = child->next;
        index--;
    }
    return index == 0 ? child : 0;
}

static void files_draw(window_t* win, int ox, int oy)
{
    (void)win;
    char path[ORBIT_MAX_PATH];
    fs_abspath(files_cwd, path, sizeof(path));

    gfx_fill(ox, oy, win->w, FILES_HEADER - 6, 0xFF1B202C);
    gfx_text_bold(ox + 12, oy + 7, THEME_ACCENT, path);
    gfx_hline(ox, oy + FILES_HEADER - 6, win->w, 0xFF2A3142);

    int row = 0;
    int max_rows = files_visible_rows();
    int iy = oy + FILES_HEADER;

    if (files_cwd->parent) {
        gfx_fill(ox + 12, iy + 6, 12, 10, 0xFFD8A642);
        gfx_text(ox + 36, iy + 4, THEME_TEXT_DIM, "..");
        iy += FILES_ROW_H;
        row++;
    }

    for (fs_node_t* child = files_cwd->children; child && row < max_rows; child = child->next, row++) {
        if (child->type == FS_DIR) {
            gfx_fill(ox + 12, iy + 6, 12, 10, 0xFFD8A642);
            gfx_fill(ox + 12, iy + 4, 6, 3, 0xFFD8A642);
        } else {
            gfx_fill(ox + 13, iy + 4, 10, 13, 0xFFC9CFDD);
            gfx_fill(ox + 14, iy + 7, 8, 1, 0xFF5A6378);
            gfx_fill(ox + 14, iy + 10, 8, 1, 0xFF5A6378);
        }
        gfx_text(ox + 36, iy + 4, THEME_TEXT, child->name);
        if (child->type == FS_FILE) {
            char sz[24];
            ksnprintf(sz, sizeof(sz), "%u B", child->size);
            gfx_text(ox + win->w - 12 - gfx_text_width(sz), iy + 4, THEME_TEXT_DIM, sz);
        }
        iy += FILES_ROW_H;
    }
}

static void files_click(window_t* win, int cx, int cy)
{
    (void)cx;
    (void)win;
    if (cy < FILES_HEADER)
        return;
    int index = (cy - FILES_HEADER) / FILES_ROW_H;
    fs_node_t* node = files_entry_at(index);
    if (node && node->type == FS_DIR) {
        files_cwd = node;
        desktop_mark_dirty();
    }
}

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
    gfx_text(cx - gfx_text_width("Made by xRookieFight") / 2, y, THEME_ACCENT, "Made by xRookieFight");
}

void winapps_init(void)
{
    files_cwd = fs_root();

    files_win = wm_create("Files", 120, 100, 460, 380);
    files_win->on_draw = files_draw;
    files_win->on_click = files_click;

    sysinfo_win = wm_create("System Monitor", 200, 140, 440, 420);
    sysinfo_win->on_draw = sysinfo_draw;
    sysinfo_win->on_tick = sysinfo_tick;

    about_win = wm_create("About Orbit", 280, 180, 380, 330);
    about_win->on_draw = about_draw;
}
