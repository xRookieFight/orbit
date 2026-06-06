#include "api.h"
#include "console.h"
#include "fmt.h"
#include "heap.h"
#include "pit.h"
#include "string.h"
#include "orbit.h"

static fs_node_t* current_dir;
static int redirect_active;
static char* redirect_buf;
static uint32_t redirect_len;
static uint32_t redirect_cap;
static fs_node_t* redirect_file;
static int redirect_append;

void api_init(void)
{
    fs_node_t* home = fs_resolve(fs_root(), user_current()->home);
    current_dir = home ? home : fs_root();
}

void api_putc(char c)
{
    if (redirect_active) {
        if (redirect_len + 1 >= redirect_cap) {
            uint32_t new_cap = redirect_cap ? redirect_cap * 2 : 256;
            char* nb = (char*)kmalloc(new_cap);
            if (!nb)
                return;
            if (redirect_buf) {
                memcpy(nb, redirect_buf, redirect_len);
                kfree(redirect_buf);
            }
            redirect_buf = nb;
            redirect_cap = new_cap;
        }
        redirect_buf[redirect_len++] = c;
        return;
    }
    console_putc(c);
}

void api_write(const char* s)
{
    while (*s)
        api_putc(*s++);
}

void api_writen(const char* data, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++)
        api_putc(data[i]);
}

static void api_sink(char c, void* ctx)
{
    (void)ctx;
    api_putc(c);
}

void api_vprint(const char* fmt, va_list args)
{
    fmt_vformat(api_sink, NULL, fmt, args);
}

void api_print(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fmt_vformat(api_sink, NULL, fmt, args);
    va_end(args);
}

void api_error(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    console_vprintf(fmt, args);
    va_end(args);
}

void api_readline(char* buf, uint32_t size)
{
    console_readline(buf, size);
}

void api_read_secret(char* buf, uint32_t size)
{
    uint32_t len = 0;
    for (;;) {
        char c = console_getc();
        if (c == '\r' || c == '\n') {
            console_putc('\n');
            break;
        }
        if (c == '\b' || c == 127) {
            if (len > 0) {
                len--;
                console_putc('\b');
                console_putc(' ');
                console_putc('\b');
            }
            continue;
        }
        if (c >= 32 && c < 127 && len + 1 < size) {
            buf[len++] = c;
            console_putc('*');
        }
    }
    buf[len] = '\0';
}

char api_getc(void)
{
    return console_getc();
}

fs_node_t* api_cwd(void)
{
    return current_dir;
}

void api_set_cwd(fs_node_t* dir)
{
    if (dir && dir->type == FS_DIR)
        current_dir = dir;
}

fs_node_t* api_fs_root(void)
{
    return fs_root();
}

user_t* api_user(void)
{
    return user_current();
}

const char* api_version(void)
{
    return ORBIT_BANNER;
}

uint32_t api_uptime_ms(void)
{
    uint32_t freq = pit_frequency();
    if (freq == 0)
        return 0;
    return (pit_ticks() * 1000u) / freq;
}

void api_redirect_begin(fs_node_t* file, int append)
{
    redirect_active = 1;
    redirect_buf = NULL;
    redirect_len = 0;
    redirect_cap = 0;
    redirect_file = file;
    redirect_append = append;
}

void api_redirect_end(void)
{
    if (redirect_file) {
        if (redirect_append)
            fs_append(redirect_file, redirect_buf ? redirect_buf : "", redirect_len);
        else
            fs_write(redirect_file, redirect_buf ? redirect_buf : "", redirect_len);
    }
    if (redirect_buf)
        kfree(redirect_buf);
    redirect_active = 0;
    redirect_buf = NULL;
    redirect_len = 0;
    redirect_cap = 0;
    redirect_file = NULL;
    redirect_append = 0;
}
