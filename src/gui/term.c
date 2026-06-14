#include "term.h"
#include "gfx.h"
#include "font.h"
#include "palette.h"
#include "pit.h"
#include "string.h"

#define SCROLLBACK 300
#define INPUT_SIZE 256

typedef struct {
    char ch;
    uint8_t attr;
} cell_t;

static cell_t cells[TERM_ROWS][TERM_COLS];
static cell_t history[SCROLLBACK][TERM_COLS];
static int history_count;
static int history_head;
static int view_offset;

static int cur_row;
static int cur_col;
static uint8_t cur_attr;

static volatile char input_ring[INPUT_SIZE];
static volatile uint32_t input_head;
static volatile uint32_t input_tail;

static volatile int dirty;

static cell_t make_cell(char c)
{
    cell_t cell;
    cell.ch = c;
    cell.attr = cur_attr;
    return cell;
}

void term_init(void)
{
    cur_attr = (uint8_t)(COL_LIGHT_GREY | (COL_BLACK << 4));
    term_clear();
}

void term_clear(void)
{
    for (int r = 0; r < TERM_ROWS; r++)
        for (int c = 0; c < TERM_COLS; c++)
            cells[r][c] = make_cell(' ');
    cur_row = 0;
    cur_col = 0;
    view_offset = 0;
    dirty = 1;
}

void term_set_color(uint8_t fg, uint8_t bg)
{
    cur_attr = (uint8_t)((fg & 15) | ((bg & 15) << 4));
}

static void history_push(cell_t* line)
{
    memcpy(history[history_head], line, sizeof(cell_t) * TERM_COLS);
    history_head = (history_head + 1) % SCROLLBACK;
    if (history_count < SCROLLBACK)
        history_count++;
}

static void scroll(void)
{
    if (cur_row < TERM_ROWS)
        return;
    history_push(cells[0]);
    for (int r = 1; r < TERM_ROWS; r++)
        memcpy(cells[r - 1], cells[r], sizeof(cells[0]));
    for (int c = 0; c < TERM_COLS; c++)
        cells[TERM_ROWS - 1][c] = make_cell(' ');
    cur_row = TERM_ROWS - 1;
}

void term_putc(char c)
{
    view_offset = 0;
    switch (c) {
    case '\n':
        cur_col = 0;
        cur_row++;
        break;
    case '\r':
        cur_col = 0;
        break;
    case '\b':
        if (cur_col > 0) {
            cur_col--;
            cells[cur_row][cur_col] = make_cell(' ');
        }
        break;
    case '\t':
        cur_col = (cur_col + 4) & ~3;
        if (cur_col >= TERM_COLS) {
            cur_col = 0;
            cur_row++;
        }
        break;
    default:
        cells[cur_row][cur_col] = make_cell(c);
        cur_col++;
        if (cur_col >= TERM_COLS) {
            cur_col = 0;
            cur_row++;
        }
        break;
    }
    scroll();
    dirty = 1;
}

void term_scroll_view_up(void)
{
    int step = TERM_ROWS - 1;
    if (view_offset + step <= history_count)
        view_offset += step;
    else
        view_offset = history_count;
    dirty = 1;
}

void term_scroll_view_down(void)
{
    int step = TERM_ROWS - 1;
    if (view_offset > step)
        view_offset -= step;
    else
        view_offset = 0;
    dirty = 1;
}

void term_feed_key(char c)
{
    uint32_t next = (input_head + 1) % INPUT_SIZE;
    if (next != input_tail) {
        input_ring[input_head] = c;
        input_head = next;
    }
}

int term_read_key(void)
{
    if (input_head == input_tail)
        return -1;
    char c = input_ring[input_tail];
    input_tail = (input_tail + 1) % INPUT_SIZE;
    return (int)(uint8_t)c;
}

int term_pixel_width(void)
{
    return TERM_COLS * FONT_W + TERM_PAD * 2;
}

int term_pixel_height(void)
{
    return TERM_ROWS * FONT_H + TERM_PAD * 2;
}

int term_consume_dirty(void)
{
    if (!dirty)
        return 0;
    dirty = 0;
    return 1;
}

static const cell_t* visible_row(int r)
{
    if (view_offset == 0)
        return cells[r];
    int idx = r - view_offset;
    if (idx >= 0)
        return cells[idx];
    int h = history_count + idx;
    int slot = (history_head + SCROLLBACK - history_count + h) % SCROLLBACK;
    return history[slot];
}

void term_draw(int ox, int oy)
{
    gfx_fill(ox, oy, term_pixel_width(), term_pixel_height(), palette_rgb(COL_BLACK));
    for (int r = 0; r < TERM_ROWS; r++) {
        const cell_t* row = visible_row(r);
        int py = oy + TERM_PAD + r * FONT_H;
        for (int c = 0; c < TERM_COLS; c++) {
            int px = ox + TERM_PAD + c * FONT_W;
            uint8_t bg = (uint8_t)(row[c].attr >> 4);
            uint8_t fg = (uint8_t)(row[c].attr & 15);
            if (bg != COL_BLACK)
                gfx_fill(px, py, FONT_W, FONT_H, palette_rgb(bg));
            if (row[c].ch != ' ')
                gfx_char(px, py, palette_rgb(fg), row[c].ch);
        }
    }
    if (view_offset == 0 && (pit_ticks() / 50) % 2 == 0) {
        int px = ox + TERM_PAD + cur_col * FONT_W;
        int py = oy + TERM_PAD + cur_row * FONT_H;
        gfx_fill(px, py + FONT_H - 3, FONT_W, 2, 0xFFE8EAF0);
    }
    if (view_offset > 0)
        gfx_text_bold(ox + term_pixel_width() - 110, oy + 4, 0xFF8AE234, "[scrollback]");
}
