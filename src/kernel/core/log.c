#include "log.h"
#include "orbit.h"
#include "fmt.h"
#include "serial.h"
#include "fs.h"
#include "pit.h"
#include "string.h"

static fs_node_t* log_file;

void log_init(void)
{
    log_file = fs_resolve(fs_root(), "/var/log/orbit.log");
    if (!log_file)
        log_file = fs_create(fs_root(), "/var/log/orbit.log", FS_FILE, 0);
}

void klog(const char* level, const char* fmt, ...)
{
    char line[256];
    size_t off = (size_t)ksnprintf(line, sizeof(line), "[%8u] %s: ", pit_ticks(), level);

    va_list args;
    va_start(args, fmt);
    off += (size_t)kvsnprintf(line + off, sizeof(line) - off, fmt, args);
    va_end(args);

    if (off < sizeof(line) - 1) {
        line[off++] = '\n';
        line[off] = '\0';
    }

    if (ORBIT_DEBUG_SERIAL)
        serial_write(line);
    if (log_file)
        fs_append(log_file, line, (uint32_t)off);
}
