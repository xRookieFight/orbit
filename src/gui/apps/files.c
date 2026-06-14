#include "appreg.h"
#include "gfx.h"
#include "theme.h"
#include "fs.h"
#include "fmt.h"
#include "string.h"
#include "orbit.h"
#include "desktop.h"

#define FILES_ROW_H 24
#define FILES_HEADER 36

static window_t* files_win;
static fs_node_t* files_cwd;

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

static window_t* files_create(void)
{
    files_cwd = fs_root();
    files_win = wm_create("Files", 120, 100, 460, 380);
    files_win->on_draw = files_draw;
    files_win->on_click = files_click;
    return files_win;
}

REGISTER_GUI_APP("Files", files_create);
