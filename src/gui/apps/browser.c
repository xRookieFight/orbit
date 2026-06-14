#include "apps/browser.h"
#include "appreg.h"
#include "gfx.h"
#include "theme.h"
#include "font.h"
#include "heap.h"
#include "string.h"
#include "fmt.h"
#include "net.h"
#include "dns.h"
#include "tcp.h"
#include "desktop.h"

#define URL_MAX     512
#define HOST_MAX    128
#define PATH_MAX    384
#define RECV_CAP    (512 * 1024)
#define TEXT_CAP    (512 * 1024)
#define TOK_MAX     24000
#define HREF_CAP    (64 * 1024)
#define LINK_MAX    4000

#define PAD         12
#define TOOLBAR_H   44
#define STATUS_H    24
#define SCROLL_W    10
#define LINE_H      20
#define HEAD_H      34
#define PARA_GAP    10

#define ST_NORMAL   0
#define ST_BOLD     1
#define ST_LINK     2
#define ST_HEAD     3

#define BRK_NONE    0
#define BRK_LINE    1
#define BRK_PARA    2

enum {
    B_IDLE,
    B_RESOLVING,
    B_CONNECTING,
    B_LOADING,
    B_DONE,
    B_ERROR
};

typedef struct {
    int off;
    int len;
    uint8_t style;
    uint8_t brk;
    int link;
} tok_t;

static window_t* win;
static int state;
static int editing = 1;
static char url[URL_MAX];
static int url_len;
static char host[HOST_MAX];
static char path[PATH_MAX];
static uint16_t port;
static char title[96];
static char status[96];

static uint32_t pend_ip;
static int pend_connect;
static uint8_t* recv_buf;
static int recv_len;

static char* text;
static int text_used;
static tok_t* toks;
static int ntok;
static char* hrefs;
static int hrefs_used;
static int link_off[LINK_MAX];
static int nlink;
static int scroll_px;
static int content_h_px;

static void set_status(const char* s)
{
    int i = 0;
    while (s[i] && i < (int)sizeof(status) - 1) {
        status[i] = s[i];
        i++;
    }
    status[i] = 0;
    desktop_mark_dirty();
}

static int parse_url(void)
{
    const char* p = url;
    if (strncmp(p, "http://", 7) == 0)
        p += 7;
    else if (strncmp(p, "https://", 8) == 0)
        p += 8;

    int hi = 0;
    port = 80;
    while (*p && *p != '/' && *p != ':' && hi < HOST_MAX - 1)
        host[hi++] = *p++;
    host[hi] = 0;
    if (hi == 0)
        return -1;

    if (*p == ':') {
        p++;
        int pn = 0;
        while (*p >= '0' && *p <= '9') {
            pn = pn * 10 + (*p - '0');
            p++;
        }
        if (pn > 0 && pn < 65536)
            port = (uint16_t)pn;
    }

    int pi = 0;
    if (*p != '/')
        path[pi++] = '/';
    while (*p && pi < PATH_MAX - 1)
        path[pi++] = *p++;
    path[pi] = 0;
    return 0;
}

static int ci_match(const char* a, const char* b, int n)
{
    for (int i = 0; i < n; i++) {
        char ca = a[i];
        if (ca >= 'A' && ca <= 'Z')
            ca = (char)(ca + 32);
        if (ca != b[i])
            return 0;
    }
    return 1;
}

static int find_href(const char* tag, int tlen, char* out, int cap)
{
    for (int i = 0; i + 4 < tlen; i++) {
        if (ci_match(tag + i, "href", 4)) {
            int j = i + 4;
            while (j < tlen && (tag[j] == ' ' || tag[j] == '='))
                j++;
            char q = 0;
            if (j < tlen && (tag[j] == '"' || tag[j] == '\'')) {
                q = tag[j];
                j++;
            }
            int o = 0;
            while (j < tlen && o < cap - 1) {
                char c = tag[j];
                if (q && c == q)
                    break;
                if (!q && (c == ' ' || c == '>'))
                    break;
                out[o++] = c;
                j++;
            }
            out[o] = 0;
            return o;
        }
    }
    return 0;
}

static void emit_word(const char* w, int wlen, uint8_t style, uint8_t* pbrk, int link)
{
    if (wlen <= 0 || ntok >= TOK_MAX || text_used + wlen >= TEXT_CAP - 1)
        return;
    tok_t* t = &toks[ntok++];
    t->off = text_used;
    t->len = wlen;
    t->style = style;
    t->brk = *pbrk;
    t->link = link;
    *pbrk = BRK_NONE;
    memcpy(text + text_used, w, wlen);
    text_used += wlen;
}

static void html_parse(const char* html, int hlen)
{
    ntok = 0;
    text_used = 0;
    hrefs_used = 0;
    nlink = 0;

    int skip = 0;
    int bold = 0;
    int head = 0;
    int link = -1;
    uint8_t pbrk = BRK_NONE;
    char word[256];
    int wl = 0;

    for (int i = 0; i < hlen;) {
        if (html[i] == '<') {
            if (wl > 0) {
                emit_word(word, wl, head ? ST_HEAD : (link >= 0 ? ST_LINK : (bold ? ST_BOLD : ST_NORMAL)), &pbrk, link);
                wl = 0;
            }
            int j = i + 1;
            while (j < hlen && html[j] != '>')
                j++;
            const char* tag = html + i + 1;
            int tlen = j - (i + 1);
            int close = (tlen > 0 && tag[0] == '/');
            const char* nm = close ? tag + 1 : tag;
            int nl = close ? tlen - 1 : tlen;

            if (nl >= 6 && ci_match(nm, "script", 6))
                skip = !close;
            else if (nl >= 5 && ci_match(nm, "style", 5))
                skip = !close;
            else if (!skip) {
                if (nl >= 1 && (nm[0] == 'b' || nm[0] == 'B') && (nl == 1 || nm[1] == ' '))
                    bold = !close;
                else if (nl >= 6 && ci_match(nm, "strong", 6))
                    bold = !close;
                else if (nl >= 1 && (nm[0] == 'a' || nm[0] == 'A') && (nl == 1 || nm[1] == ' ')) {
                    if (close) {
                        link = -1;
                    } else if (nlink < LINK_MAX) {
                        char href[512];
                        int hn = find_href(tag, tlen, href, sizeof(href));
                        if (hn > 0 && hrefs_used + hn + 1 < HREF_CAP) {
                            link_off[nlink] = hrefs_used;
                            memcpy(hrefs + hrefs_used, href, hn + 1);
                            hrefs_used += hn + 1;
                            link = nlink++;
                        }
                    }
                } else if (nl == 2 && (nm[0] == 'h' || nm[0] == 'H') && nm[1] >= '1' && nm[1] <= '6') {
                    head = !close;
                    pbrk = BRK_PARA;
                } else if ((nl >= 2 && ci_match(nm, "br", 2)) ||
                           (nl >= 1 && (nm[0] == 'p' || nm[0] == 'P') && (nl == 1 || nm[1] == ' ')) ||
                           (nl >= 3 && ci_match(nm, "div", 3)) ||
                           (nl >= 2 && ci_match(nm, "tr", 2)) ||
                           (nl >= 2 && ci_match(nm, "hr", 2)) ||
                           (nl >= 2 && ci_match(nm, "ul", 2)) ||
                           (nl >= 2 && ci_match(nm, "ol", 2))) {
                    if (pbrk < BRK_LINE)
                        pbrk = BRK_LINE;
                } else if ((nl == 2 || (nl > 2 && nm[2] == ' ')) && ci_match(nm, "li", 2) && !close) {
                    pbrk = BRK_LINE;
                    emit_word("\x07", 1, ST_NORMAL, &pbrk, -1);
                }
            }
            i = (j < hlen) ? j + 1 : hlen;
            continue;
        }
        if (skip) {
            i++;
            continue;
        }
        char c = html[i++];
        if (c == '&') {
            if (ci_match(html + i - 1, "&amp;", 5)) { c = '&'; i += 4; }
            else if (ci_match(html + i - 1, "&lt;", 4)) { c = '<'; i += 3; }
            else if (ci_match(html + i - 1, "&gt;", 4)) { c = '>'; i += 3; }
            else if (ci_match(html + i - 1, "&quot;", 6)) { c = '"'; i += 5; }
            else if (ci_match(html + i - 1, "&#39;", 5)) { c = '\''; i += 4; }
            else if (ci_match(html + i - 1, "&nbsp;", 6)) { c = ' '; i += 5; }
        }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (wl > 0) {
                emit_word(word, wl, head ? ST_HEAD : (link >= 0 ? ST_LINK : (bold ? ST_BOLD : ST_NORMAL)), &pbrk, link);
                wl = 0;
            }
        } else if (wl < (int)sizeof(word) - 1) {
            word[wl++] = c;
        }
    }
    if (wl > 0)
        emit_word(word, wl, head ? ST_HEAD : (link >= 0 ? ST_LINK : (bold ? ST_BOLD : ST_NORMAL)), &pbrk, link);
}

static void finish_load(void)
{
    int hdr_end = -1;
    for (int i = 0; i + 3 < recv_len; i++) {
        if (recv_buf[i] == '\r' && recv_buf[i + 1] == '\n' &&
            recv_buf[i + 2] == '\r' && recv_buf[i + 3] == '\n') {
            hdr_end = i + 4;
            break;
        }
    }
    if (hdr_end < 0)
        hdr_end = 0;
    html_parse((const char*)recv_buf + hdr_end, recv_len - hdr_end);
    scroll_px = 0;
    state = B_DONE;
    set_status("Done");
}

static void on_tcp_data(const uint8_t* data, uint16_t len)
{
    int n = len;
    if (recv_len + n > RECV_CAP)
        n = RECV_CAP - recv_len;
    if (n > 0) {
        memcpy(recv_buf + recv_len, data, n);
        recv_len += n;
    }
    desktop_mark_dirty();
}

static void send_request(void)
{
    char req[640];
    int n = 0;
    const char* parts[] = { "GET ", path, " HTTP/1.0\r\nHost: ", host,
                            "\r\nUser-Agent: Orbit/2.0\r\nConnection: close\r\n\r\n" };
    for (int k = 0; k < 5; k++) {
        const char* s = parts[k];
        while (*s && n < (int)sizeof(req) - 1)
            req[n++] = *s++;
    }
    tcp_write(req, (uint16_t)n);
}

static void on_tcp_event(int connected)
{
    if (connected) {
        state = B_LOADING;
        set_status("Loading...");
        send_request();
    } else {
        if (state == B_LOADING && recv_len > 0)
            finish_load();
        else {
            state = B_ERROR;
            set_status("Connection failed");
        }
    }
}

static void on_dns(uint32_t ip)
{
    if (ip == 0) {
        state = B_ERROR;
        set_status("DNS failed");
        return;
    }
    pend_ip = ip;
    pend_connect = 1;
    state = B_CONNECTING;
    set_status("Connecting...");
}

static void navigate(void)
{
    if (!net_is_up()) {
        state = B_ERROR;
        set_status("Network down");
        return;
    }
    if (tcp_active())
        tcp_close();
    if (parse_url() != 0) {
        state = B_ERROR;
        set_status("Bad URL");
        return;
    }
    strlcpy(title, host, sizeof(title));
    recv_len = 0;
    ntok = 0;
    scroll_px = 0;
    editing = 0;
    state = B_RESOLVING;
    set_status("Resolving...");
    dns_resolve(host, on_dns);
}

void browser_poll(void)
{
    if (pend_connect) {
        pend_connect = 0;
        tcp_connect(pend_ip, port, on_tcp_data, on_tcp_event);
    }
}

static void follow_link(int id)
{
    if (id < 0 || id >= nlink)
        return;
    const char* href = hrefs + link_off[id];
    if (strncmp(href, "https://", 8) == 0) {
        set_status("HTTPS not supported");
        return;
    }
    char nu[URL_MAX];
    int n = 0;
    if (strncmp(href, "http://", 7) == 0) {
        while (href[n] && n < URL_MAX - 1) {
            nu[n] = href[n];
            n++;
        }
    } else if (href[0] == '/') {
        const char* h = host;
        while (*h && n < URL_MAX - 1)
            nu[n++] = *h++;
        const char* p = href;
        while (*p && n < URL_MAX - 1)
            nu[n++] = *p++;
    } else {
        const char* h = host;
        while (*h && n < URL_MAX - 1)
            nu[n++] = *h++;
        int cut = 0;
        for (int k = 0; path[k]; k++)
            if (path[k] == '/')
                cut = k + 1;
        for (int k = 0; k < cut && n < URL_MAX - 1; k++)
            nu[n++] = path[k];
        const char* p = href;
        while (*p && n < URL_MAX - 1)
            nu[n++] = *p++;
    }
    nu[n] = 0;
    memcpy(url, nu, n + 1);
    url_len = n;
    navigate();
}

static int content_x(void) { return PAD; }
static int content_y(void) { return TOOLBAR_H; }
static int content_w(void) { return win->w - 2 * PAD - SCROLL_W; }
static int content_view_h(void) { return win->h - TOOLBAR_H - STATUS_H; }

static int layout(int ox, int oy, int do_draw, int hit_dx, int hit_dy)
{
    int cw = content_w();
    int cx = content_x();
    int cy = content_y();
    int vh = content_view_h();
    int hit = -1;

    int x = 0;
    int y = 0;
    int line_h = LINE_H;

    for (int i = 0; i < ntok; i++) {
        tok_t* t = &toks[i];
        int scale = (t->style == ST_HEAD) ? 2 : 1;
        int gw = FONT_W * scale;
        int lh = (t->style == ST_HEAD) ? HEAD_H : LINE_H;
        int bullet = (t->len == 1 && text[t->off] == 7);
        int wpx = bullet ? gw : t->len * gw;

        if (t->brk == BRK_LINE) {
            y += line_h;
            x = 0;
            line_h = LINE_H;
        } else if (t->brk == BRK_PARA) {
            y += line_h + PARA_GAP;
            x = 0;
            line_h = LINE_H;
        }

        if (x > 0 && x + wpx > cw) {
            y += line_h;
            x = 0;
            line_h = lh;
        }
        if (lh > line_h)
            line_h = lh;

        int dx = ox + cx + x;
        int dy = oy + cy + y - scroll_px;

        if (do_draw && dy + lh > oy + cy && dy < oy + cy + vh) {
            if (bullet) {
                gfx_fill(dx + 2, dy + 7, 5, 5, THEME_ACCENT);
            } else {
                char tmp[256];
                int cl = t->len < 255 ? t->len : 255;
                memcpy(tmp, text + t->off, cl);
                tmp[cl] = 0;
                if (t->style == ST_HEAD)
                    gfx_text_scaled(dx, dy, 0xFFFFFFFF, tmp, 2, 1);
                else if (t->style == ST_LINK) {
                    gfx_text(dx, dy, THEME_ACCENT, tmp);
                    gfx_hline(dx, dy + FONT_H - 2, t->len * gw, THEME_ACCENT);
                } else if (t->style == ST_BOLD)
                    gfx_text_bold(dx, dy, THEME_TEXT, tmp);
                else
                    gfx_text(dx, dy, THEME_TEXT, tmp);
            }
        }

        if (!do_draw && t->link >= 0 && !bullet) {
            int rx = x;
            int ry = y;
            if (hit_dx >= rx && hit_dx < rx + wpx && hit_dy >= ry && hit_dy < ry + lh)
                hit = t->link;
        }

        x += wpx + gw;
    }

    content_h_px = y + line_h;
    return hit;
}

static void browser_draw(window_t* w, int ox, int oy)
{
    (void)w;

    gfx_fill(ox, oy, win->w, TOOLBAR_H, 0xFF161C28);
    gfx_hline(ox, oy + TOOLBAR_H - 1, win->w, 0xFF0C111B);

    int btn_w = 44;
    int bar_x = ox + PAD;
    int bar_w = win->w - 2 * PAD - btn_w * 2 - 12;
    int bar_y = oy + 9;
    int bar_h = 26;

    gfx_rounded(bar_x, bar_y, bar_w, bar_h, 6, 0xFF0B0F18);
    gfx_rect(bar_x, bar_y, bar_w, bar_h, editing ? THEME_ACCENT : 0xFF2A3142);
    gfx_clip(bar_x + 8, bar_y, bar_w - 16, bar_h);
    gfx_text(bar_x + 10, bar_y + 5, THEME_TEXT, url);
    if (editing) {
        int caret = bar_x + 10 + url_len * FONT_W;
        gfx_fill(caret, bar_y + 5, 2, FONT_H, THEME_ACCENT);
    }
    gfx_clip_reset();

    int rl_x = bar_x + bar_w + 6;
    int go_x = rl_x + btn_w + 6;
    gfx_rounded(rl_x, bar_y, btn_w, bar_h, 6, 0xFF2A3142);
    gfx_text(rl_x + 14, bar_y + 5, THEME_TEXT, "<-");
    gfx_rounded(go_x, bar_y, btn_w, bar_h, 6, THEME_ACCENT);
    gfx_text_bold(go_x + 12, bar_y + 5, 0xFFFFFFFF, "Go");

    int cx = content_x();
    int cy = content_y();
    int vh = content_view_h();
    gfx_clip(ox + cx, oy + cy, content_w() + SCROLL_W, vh);
    layout(ox, oy, 1, 0, 0);
    gfx_clip_reset();

    int track_x = ox + win->w - SCROLL_W - 2;
    gfx_fill(track_x, oy + cy, SCROLL_W, vh, 0xFF121826);
    if (content_h_px > vh) {
        int th = vh * vh / content_h_px;
        if (th < 24)
            th = 24;
        int range = content_h_px - vh;
        int ty = oy + cy + (vh - th) * scroll_px / (range ? range : 1);
        gfx_rounded(track_x + 1, ty, SCROLL_W - 2, th, 3, THEME_ACCENT_DARK);
    }

    int sy = oy + win->h - STATUS_H;
    gfx_fill(ox, sy, win->w, STATUS_H, 0xFF121826);
    gfx_hline(ox, sy, win->w, 0xFF0C111B);
    gfx_text(ox + PAD, sy + 4, THEME_TEXT_DIM, status);
    char info[40];
    ksnprintf(info, sizeof(info), "%d B", recv_len);
    if (state == B_DONE)
        gfx_text(ox + win->w - PAD - gfx_text_width(info), sy + 4, THEME_TEXT_DIM, info);
}

static void browser_key(window_t* w, int ch)
{
    (void)w;
    if (ch == '\n' || ch == '\r') {
        navigate();
        return;
    }
    if (!editing)
        return;
    if (ch == '\b') {
        if (url_len > 0)
            url[--url_len] = 0;
        desktop_mark_dirty();
        return;
    }
    if (ch >= 32 && ch < 127 && url_len < URL_MAX - 1) {
        url[url_len++] = (char)ch;
        url[url_len] = 0;
        desktop_mark_dirty();
    }
}

static void scroll_by(int dy)
{
    scroll_px += dy;
    int maxs = content_h_px - content_view_h();
    if (maxs < 0)
        maxs = 0;
    if (scroll_px > maxs)
        scroll_px = maxs;
    if (scroll_px < 0)
        scroll_px = 0;
    desktop_mark_dirty();
}

static void browser_click(window_t* w, int cx, int cy)
{
    (void)w;
    int btn_w = 44;
    int bar_x = PAD;
    int bar_w = win->w - 2 * PAD - btn_w * 2 - 12;
    int rl_x = bar_x + bar_w + 6;
    int go_x = rl_x + btn_w + 6;

    if (cy >= 9 && cy < 35) {
        if (cx >= bar_x && cx < bar_x + bar_w) {
            editing = 1;
            desktop_mark_dirty();
            return;
        }
        if (cx >= rl_x && cx < rl_x + btn_w) {
            navigate();
            return;
        }
        if (cx >= go_x && cx < go_x + btn_w) {
            navigate();
            return;
        }
    }

    int track_x = win->w - SCROLL_W - 2;
    int cyv = content_y();
    int vh = content_view_h();
    if (cx >= track_x && cy >= cyv && cy < cyv + vh) {
        int range = content_h_px - vh;
        if (range > 0) {
            scroll_px = (cy - cyv) * range / vh;
            scroll_by(0);
        }
        return;
    }

    if (cy >= cyv && cy < cyv + vh && cx < track_x) {
        editing = 0;
        int dx = cx - content_x();
        int dy = cy - cyv + scroll_px;
        int id = layout(0, 0, 0, dx, dy);
        if (id >= 0)
            follow_link(id);
        else
            desktop_mark_dirty();
    }
}

void browser_init(void)
{
    recv_buf = (uint8_t*)kmalloc(RECV_CAP);
    text = (char*)kmalloc(TEXT_CAP);
    toks = (tok_t*)kmalloc(sizeof(tok_t) * TOK_MAX);
    hrefs = (char*)kmalloc(HREF_CAP);

    const char* def = "example.com";
    url_len = 0;
    while (def[url_len]) {
        url[url_len] = def[url_len];
        url_len++;
    }
    url[url_len] = 0;
    strlcpy(title, "Browser", sizeof(title));
    set_status("Ready");
    state = B_IDLE;
    editing = 1;

    win = wm_create("Browser", 150, 80, 760, 560);
    win->on_draw = browser_draw;
    win->on_key = browser_key;
    win->on_click = browser_click;
}

window_t* browser_window(void)
{
    return win;
}

static window_t* browser_create(void)
{
    browser_init();
    return win;
}

REGISTER_GUI_APP("Browser", browser_create);
