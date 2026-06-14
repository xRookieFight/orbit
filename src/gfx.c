#include "gfx.h"
#include "fb.h"
#include "font.h"
#include "string.h"

static int clip_x0;
static int clip_y0;
static int clip_x1;
static int clip_y1;

void gfx_clip(int x, int y, int w, int h)
{
    clip_x0 = x < 0 ? 0 : x;
    clip_y0 = y < 0 ? 0 : y;
    clip_x1 = x + w > fb_width() ? fb_width() : x + w;
    clip_y1 = y + h > fb_height() ? fb_height() : y + h;
}

void gfx_clip_reset(void)
{
    clip_x0 = 0;
    clip_y0 = 0;
    clip_x1 = fb_width();
    clip_y1 = fb_height();
}

uint32_t gfx_mix(uint32_t a, uint32_t b, int t256)
{
    uint32_t ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
    uint32_t br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;
    uint32_t r = (ar * (256 - (uint32_t)t256) + br * (uint32_t)t256) >> 8;
    uint32_t g = (ag * (256 - (uint32_t)t256) + bg * (uint32_t)t256) >> 8;
    uint32_t bl = (ab * (256 - (uint32_t)t256) + bb * (uint32_t)t256) >> 8;
    return 0xFF000000u | (r << 16) | (g << 8) | bl;
}

static inline void put(int x, int y, uint32_t color)
{
    if (x < clip_x0 || x >= clip_x1 || y < clip_y0 || y >= clip_y1)
        return;
    fb_backbuffer()[(size_t)y * (size_t)fb_width() + (size_t)x] = color | 0xFF000000u;
}

static inline void blend(int x, int y, uint32_t argb)
{
    if (x < clip_x0 || x >= clip_x1 || y < clip_y0 || y >= clip_y1)
        return;
    uint32_t alpha = argb >> 24;
    if (alpha == 0)
        return;
    uint32_t* p = &fb_backbuffer()[(size_t)y * (size_t)fb_width() + (size_t)x];
    if (alpha == 255) {
        *p = argb | 0xFF000000u;
        return;
    }
    uint32_t dst = *p;
    uint32_t sr = (argb >> 16) & 0xFF, sg = (argb >> 8) & 0xFF, sb = argb & 0xFF;
    uint32_t dr = (dst >> 16) & 0xFF, dg = (dst >> 8) & 0xFF, db = dst & 0xFF;
    uint32_t r = (sr * alpha + dr * (255 - alpha)) / 255;
    uint32_t g = (sg * alpha + dg * (255 - alpha)) / 255;
    uint32_t b = (sb * alpha + db * (255 - alpha)) / 255;
    *p = 0xFF000000u | (r << 16) | (g << 8) | b;
}

void gfx_pixel(int x, int y, uint32_t color)
{
    put(x, y, color);
}

void gfx_blend_pixel(int x, int y, uint32_t argb)
{
    blend(x, y, argb);
}

void gfx_fill(int x, int y, int w, int h, uint32_t color)
{
    int x0 = x < clip_x0 ? clip_x0 : x;
    int y0 = y < clip_y0 ? clip_y0 : y;
    int x1 = x + w > clip_x1 ? clip_x1 : x + w;
    int y1 = y + h > clip_y1 ? clip_y1 : y + h;
    if (x0 >= x1 || y0 >= y1)
        return;
    uint32_t c = color | 0xFF000000u;
    for (int yy = y0; yy < y1; yy++) {
        uint32_t* row = &fb_backbuffer()[(size_t)yy * (size_t)fb_width()];
        for (int xx = x0; xx < x1; xx++)
            row[xx] = c;
    }
}

void gfx_blend_fill(int x, int y, int w, int h, uint32_t argb)
{
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            blend(xx, yy, argb);
}

void gfx_rect(int x, int y, int w, int h, uint32_t color)
{
    gfx_hline(x, y, w, color);
    gfx_hline(x, y + h - 1, w, color);
    gfx_vline(x, y, h, color);
    gfx_vline(x + w - 1, y, h, color);
}

void gfx_hline(int x, int y, int w, uint32_t color)
{
    gfx_fill(x, y, w, 1, color);
}

void gfx_vline(int x, int y, int h, uint32_t color)
{
    gfx_fill(x, y, 1, h, color);
}

void gfx_vgradient(int x, int y, int w, int h, uint32_t top, uint32_t bottom)
{
    if (h <= 0)
        return;
    for (int i = 0; i < h; i++) {
        int t = h > 1 ? (i * 256) / (h - 1) : 0;
        gfx_hline(x, y + i, w, gfx_mix(top, bottom, t));
    }
}

static int isqrt(int v)
{
    int r = 0;
    while ((r + 1) * (r + 1) <= v)
        r++;
    return r;
}

static int corner_inset(int radius, int dy)
{
    int d = radius - 1 - dy;
    return radius - isqrt(radius * radius - d * d);
}

void gfx_rounded(int x, int y, int w, int h, int radius, uint32_t color)
{
    for (int i = 0; i < h; i++) {
        int inset = 0;
        if (i < radius)
            inset = corner_inset(radius, i);
        else if (i >= h - radius)
            inset = corner_inset(radius, h - 1 - i);
        gfx_hline(x + inset, y + i, w - inset * 2, color);
    }
}

void gfx_rounded_blend(int x, int y, int w, int h, int radius, uint32_t argb)
{
    for (int i = 0; i < h; i++) {
        int inset = 0;
        if (i < radius)
            inset = corner_inset(radius, i);
        else if (i >= h - radius)
            inset = corner_inset(radius, h - 1 - i);
        for (int xx = x + inset; xx < x + w - inset; xx++)
            blend(xx, y + i, argb);
    }
}

void gfx_rounded_top(int x, int y, int w, int h, int radius, uint32_t color)
{
    for (int i = 0; i < h; i++) {
        int inset = i < radius ? corner_inset(radius, i) : 0;
        gfx_hline(x + inset, y + i, w - inset * 2, color);
    }
}

void gfx_shadow(int x, int y, int w, int h, int radius)
{
    static const uint32_t layers[6] = {
        0x30000000, 0x26000000, 0x1C000000, 0x12000000, 0x0A000000, 0x05000000
    };
    for (int i = 0; i < 6; i++) {
        int e = i + 1;
        uint32_t c = layers[i];
        gfx_blend_fill(x - e, y - e + 2, w + e * 2, 1, c);
        gfx_blend_fill(x - e, y + h + e - 1 + 2, w + e * 2, 1, c);
        gfx_blend_fill(x - e, y - e + 1 + 2, 1, h + e * 2 - 2, c);
        gfx_blend_fill(x + w + e - 1, y - e + 1 + 2, 1, h + e * 2 - 2, c);
    }
    (void)radius;
}

void gfx_circle(int cx, int cy, int r, uint32_t color)
{
    for (int dy = -r; dy <= r; dy++) {
        int half = isqrt(r * r - dy * dy);
        gfx_hline(cx - half, cy + dy, half * 2 + 1, color);
    }
}

void gfx_ring(int cx, int cy, int r, int thickness, uint32_t color)
{
    int inner = r - thickness;
    for (int dy = -r; dy <= r; dy++) {
        int outer_half = isqrt(r * r - dy * dy);
        int inner_half = (dy >= -inner && dy <= inner) ? isqrt(inner * inner - dy * dy) : -1;
        if (inner_half < 0) {
            gfx_hline(cx - outer_half, cy + dy, outer_half * 2 + 1, color);
        } else {
            gfx_hline(cx - outer_half, cy + dy, outer_half - inner_half, color);
            gfx_hline(cx + inner_half + 1, cy + dy, outer_half - inner_half, color);
        }
    }
}

static void draw_glyph(int x, int y, uint32_t color, unsigned char c, const unsigned char (*font)[16], int scale)
{
    const unsigned char* g = font[c];
    for (int row = 0; row < FONT_H; row++) {
        unsigned char bits = g[row];
        for (int col = 0; col < FONT_W; col++) {
            if (!(bits & (0x80 >> col)))
                continue;
            if (scale == 1)
                put(x + col, y + row, color);
            else
                gfx_fill(x + col * scale, y + row * scale, scale, scale, color);
        }
    }
}

void gfx_char(int x, int y, uint32_t color, char c)
{
    draw_glyph(x, y, color, (unsigned char)c, font_regular, 1);
}

void gfx_text(int x, int y, uint32_t color, const char* s)
{
    for (; *s; s++, x += FONT_W)
        draw_glyph(x, y, color, (unsigned char)*s, font_regular, 1);
}

void gfx_text_bold(int x, int y, uint32_t color, const char* s)
{
    for (; *s; s++, x += FONT_W)
        draw_glyph(x, y, color, (unsigned char)*s, font_bold, 1);
}

void gfx_text_scaled(int x, int y, uint32_t color, const char* s, int scale, int bold)
{
    const unsigned char (*font)[16] = bold ? font_bold : font_regular;
    for (; *s; s++, x += FONT_W * scale)
        draw_glyph(x, y, color, (unsigned char)*s, font, scale);
}

int gfx_text_width(const char* s)
{
    return (int)strlen(s) * FONT_W;
}
