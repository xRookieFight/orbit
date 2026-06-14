#pragma once
#include <stdint.h>
#include <stdarg.h>
#include "fs.h"
#include "user.h"

void api_init(void);

void api_print(const char* fmt, ...);
void api_vprint(const char* fmt, va_list args);
void api_putc(char c);
void api_write(const char* s);
void api_writen(const char* data, uint32_t size);
void api_error(const char* fmt, ...);

void api_readline(char* buf, uint32_t size);
void api_read_secret(char* buf, uint32_t size);
char api_getc(void);

fs_node_t* api_cwd(void);
void api_set_cwd(fs_node_t* dir);
fs_node_t* api_fs_root(void);

user_t* api_user(void);

const char* api_version(void);
uint32_t api_uptime_ms(void);

void api_redirect_begin(fs_node_t* file, int append);
void api_redirect_end(void);
