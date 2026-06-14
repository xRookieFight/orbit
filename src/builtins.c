#include "builtins.h"
#include "app.h"
#include "api.h"
#include "fs.h"
#include "user.h"
#include "proc.h"
#include "power.h"
#include "heap.h"
#include "pit.h"
#include "console.h"
#include "vga.h"
#include "shell.h"
#include "log.h"
#include "string.h"
#include "orbit.h"
#include "io.h"

static int owner_can_modify(fs_node_t* node)
{
    return user_is_root(api_user()) || node->owner == api_user()->uid;
}

static int cmd_help(int argc, char** argv)
{
    if (argc >= 2) {
        const app_t* app = app_find(argv[1]);
        if (app)
            api_print("%s - %s\n", app->name, app->summary);
        else
            api_print("help: no such command: %s\n", argv[1]);
        return 0;
    }
    api_print("Orbit 1.0 commands:\n");
    for (int i = 0; i < app_count(); i++) {
        const app_t* app = app_at(i);
        api_print("  %-10s %s\n", app->name, app->summary);
    }
    return 0;
}

static int cmd_version(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    api_print("%s\n", api_version());
    api_print("build %s\n", ORBIT_BUILD);
    api_print("architecture i386 protected mode\n");
    return 0;
}

static int cmd_clear(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    console_clear();
    return 0;
}

static int cmd_mode(int argc, char** argv)
{
    if (argc < 2) {
        api_print("display %dx%d\n", vga_cols(), vga_rows());
        api_print("usage: mode <80x25|80x50>\n");
        return 0;
    }
    const char* x = strchr(argv[1], 'x');
    if (!x) {
        api_print("mode: bad format %s (try 80x25 or 80x50)\n", argv[1]);
        return 1;
    }
    int cols = atoi(argv[1]);
    int rows = atoi(x + 1);
    if (vga_set_resolution(cols, rows) != 0) {
        api_print("mode: unsupported %s (try 80x25 or 80x50)\n", argv[1]);
        return 1;
    }
    api_print("[display] switched to %dx%d\n", cols, rows);
    return 0;
}

static int cmd_echo(int argc, char** argv)
{
    for (int i = 1; i < argc; i++) {
        if (i > 1)
            api_putc(' ');
        api_write(argv[i]);
    }
    api_putc('\n');
    return 0;
}

static int cmd_pwd(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    char path[ORBIT_MAX_PATH];
    fs_abspath(api_cwd(), path, sizeof(path));
    api_print("%s\n", path);
    return 0;
}

static void perm_string(fs_node_t* node, char* out)
{
    static const char* rwx = "rwxrwxrwx";
    out[0] = node->type == FS_DIR ? 'd' : '-';
    for (int i = 0; i < 9; i++)
        out[1 + i] = (node->perms & (1u << (8 - i))) ? rwx[i] : '-';
    out[10] = '\0';
}

static void list_entry(fs_node_t* node, int longfmt)
{
    if (longfmt) {
        char perms[11];
        perm_string(node, perms);
        api_print("%s %3u %6u %s%s\n", perms, node->owner, node->size,
                  node->name, node->type == FS_DIR ? "/" : "");
    } else {
        api_print("%s%s\n", node->name, node->type == FS_DIR ? "/" : "");
    }
}

static int cmd_ls(int argc, char** argv)
{
    int longfmt = 0;
    const char* target = NULL;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strchr(argv[i], 'l'))
                longfmt = 1;
        } else {
            target = argv[i];
        }
    }

    fs_node_t* node = target ? fs_resolve(api_cwd(), target) : api_cwd();
    if (!node) {
        api_print("ls: %s: no such file or directory\n", target);
        return 1;
    }
    if (node->type == FS_FILE) {
        list_entry(node, longfmt);
        return 0;
    }
    for (fs_node_t* child = node->children; child; child = child->next)
        list_entry(child, longfmt);
    return 0;
}

static int cmd_cd(int argc, char** argv)
{
    const char* target = argc >= 2 ? argv[1] : api_user()->home;
    fs_node_t* node = fs_resolve(api_cwd(), target);
    if (!node) {
        api_print("cd: %s: no such file or directory\n", target);
        return 1;
    }
    if (node->type != FS_DIR) {
        api_print("cd: %s: not a directory\n", target);
        return 1;
    }
    api_set_cwd(node);
    return 0;
}

static int cmd_mkdir(int argc, char** argv)
{
    if (argc < 2) {
        api_print("usage: mkdir <directory>...\n");
        return 1;
    }
    for (int i = 1; i < argc; i++)
        if (!fs_mkdir(api_cwd(), argv[i], api_user()->uid))
            api_print("mkdir: cannot create directory %s\n", argv[i]);
    return 0;
}

static int cmd_rm(int argc, char** argv)
{
    int recursive = 0;
    for (int i = 1; i < argc; i++)
        if (argv[i][0] == '-' && strchr(argv[i], 'r'))
            recursive = 1;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-')
            continue;
        fs_node_t* node = fs_resolve(api_cwd(), argv[i]);
        if (!node) {
            api_print("rm: %s: no such file or directory\n", argv[i]);
            continue;
        }
        if (!owner_can_modify(node)) {
            api_print("rm: %s: permission denied\n", argv[i]);
            continue;
        }
        int r = fs_remove(api_cwd(), argv[i], recursive);
        if (r == -2)
            api_print("rm: %s: is a directory (use -r)\n", argv[i]);
        else if (r < 0)
            api_print("rm: cannot remove %s\n", argv[i]);
    }
    return 0;
}

static int cmd_cp(int argc, char** argv)
{
    if (argc < 3) {
        api_print("usage: cp <source> <dest>\n");
        return 1;
    }
    if (!fs_copy(api_cwd(), argv[1], argv[2], api_user()->uid))
        api_print("cp: cannot copy %s\n", argv[1]);
    return 0;
}

static int cmd_mv(int argc, char** argv)
{
    if (argc < 3) {
        api_print("usage: mv <source> <dest>\n");
        return 1;
    }
    
    int r = fs_move(api_cwd(), argv[1], argv[2]);
    if (r == -3) {
        api_print("mv: cannot move directory into its own subdirectory\n");
        return 1;
    }
    if (r < 0) {
        api_print("mv: cannot move %s\n", argv[1]);
        return 1;
    }
    return 0;
}

static int cmd_cat(int argc, char** argv)
{
    if (argc < 2) {
        api_print("usage: cat <file>...\n");
        return 1;
    }
    for (int i = 1; i < argc; i++) {
        fs_node_t* node = fs_resolve(api_cwd(), argv[i]);
        if (!node) {
            api_print("cat: %s: no such file or directory\n", argv[i]);
            continue;
        }
        if (node->type != FS_FILE) {
            api_print("cat: %s: is a directory\n", argv[i]);
            continue;
        }
        if (node->data)
            api_writen(node->data, node->size);
    }
    return 0;
}

static int cmd_touch(int argc, char** argv)
{
    if (argc < 2) {
        api_print("usage: touch <file>...\n");
        return 1;
    }
    for (int i = 1; i < argc; i++)
        if (!fs_resolve(api_cwd(), argv[i]))
            fs_create(api_cwd(), argv[i], FS_FILE, api_user()->uid);
    return 0;
}

static int cmd_whoami(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    api_print("%s\n", api_user()->name);
    return 0;
}

static int cmd_login(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    shell_login();
    return 0;
}

static int cmd_logout(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    shell_request_logout();
    return 0;
}

static int cmd_passwd(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    user_t* user = api_user();
    char current[ORBIT_MAX_NAME];
    api_print("current password: ");
    api_read_secret(current, sizeof(current));
    if (!user_authenticate(user->name, current)) {
        api_print("passwd: authentication failed\n");
        return 1;
    }
    char p1[ORBIT_MAX_NAME];
    char p2[ORBIT_MAX_NAME];
    api_print("new password: ");
    api_read_secret(p1, sizeof(p1));
    api_print("retype new password: ");
    api_read_secret(p2, sizeof(p2));
    if (strcmp(p1, p2) != 0) {
        api_print("passwd: passwords do not match\n");
        return 1;
    }
    user_set_password(user, p1);
    klog("AUTH", "password changed user=%s", user->name);
    api_print("passwd: password updated\n");
    return 0;
}

static int cmd_nano(int argc, char** argv)
{
    if (argc < 2) {
        api_print("usage: nano <file>\n");
        return 1;
    }
    fs_node_t* file = fs_resolve(api_cwd(), argv[1]);
    if (file && file->type != FS_FILE) {
        api_print("nano: %s: not a file\n", argv[1]);
        return 1;
    }
    if (!file) {
        file = fs_create(api_cwd(), argv[1], FS_FILE, api_user()->uid);
        if (!file) {
            api_print("nano: cannot create %s\n", argv[1]);
            return 1;
        }
    } else if (!owner_can_modify(file)) {
        api_print("nano: %s: permission denied\n", argv[1]);
        return 1;
    }

    api_print("orbit nano - type lines, a single '.' saves and exits.\n");
    if (file->data && file->size) {
        api_print("----- current -----\n");
        api_writen(file->data, file->size);
        api_print("----- end -----\n");
    }

    uint32_t cap = 4096;
    uint32_t len = 0;
    char* buffer = (char*)kmalloc(cap);
    if (!buffer) {
        api_print("nano: out of memory\n");
        return 1;
    }
    for (;;) {
        char line[ORBIT_MAX_LINE];
        console_readline(line, sizeof(line));
        if (strcmp(line, ".") == 0)
            break;
        uint32_t ll = (uint32_t)strlen(line);
        if (len + ll + 1 > cap) {
            uint32_t nc = cap * 2 + ll;
            char* nb = (char*)kmalloc(nc);
            if (!nb)
                break;
            memcpy(nb, buffer, len);
            kfree(buffer);
            buffer = nb;
            cap = nc;
        }
        memcpy(buffer + len, line, ll);
        len += ll;
        buffer[len++] = '\n';
    }
    fs_write(file, buffer, len);
    kfree(buffer);
    api_print("saved %u bytes to %s\n", len, file->name);
    return 0;
}

static int cmd_ps(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    api_print("  PID  OWNER  STATE     NAME\n");
    for (int i = 0; i < proc_count(); i++) {
        proc_t* p = proc_at(i);
        api_print("%5d  %5u  %-8s  %s\n", p->pid, p->owner,
                  proc_state_name(p->state), p->name);
    }
    return 0;
}

static int cmd_kill(int argc, char** argv)
{
    if (argc < 2) {
        api_print("usage: kill <pid>\n");
        return 1;
    }
    int pid = atoi(argv[1]);
    int r = proc_kill(pid);
    if (r == -1)
        api_print("kill: cannot kill protected pid %d\n", pid);
    else if (r == -2)
        api_print("kill: no such process %d\n", pid);
    else
        api_print("killed %d\n", pid);
    return 0;
}

static int cmd_uptime(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    uint32_t ms = api_uptime_ms();
    uint32_t s = ms / 1000;
    uint32_t m = s / 60;
    uint32_t h = m / 60;
    api_print("up %u:%02u:%02u (%u ticks)\n", h, m % 60, s % 60, pit_ticks());
    return 0;
}

static int cmd_meminfo(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    unsigned used = (unsigned)heap_used();
    unsigned total = (unsigned)heap_total();
    api_print("heap: %u / %u bytes used, %u free\n", used, total, total - used);
    return 0;
}

static uint8_t cmos_read(uint8_t reg)
{
    outb(0x70, reg);
    return inb(0x71);
}

static uint8_t from_bcd(uint8_t value)
{
    return (uint8_t)((value & 0x0F) + ((value >> 4) * 10));
}

static int cmd_date(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    while (cmos_read(0x0A) & 0x80)
        ;
    uint8_t sec = cmos_read(0x00);
    uint8_t min = cmos_read(0x02);
    uint8_t hour = cmos_read(0x04);
    uint8_t day = cmos_read(0x07);
    uint8_t month = cmos_read(0x08);
    uint8_t year = cmos_read(0x09);
    uint8_t reg_b = cmos_read(0x0B);

    if (!(reg_b & 0x04)) {
        sec = from_bcd(sec);
        min = from_bcd(min);
        hour = from_bcd(hour);
        day = from_bcd(day);
        month = from_bcd(month);
        year = from_bcd(year);
    }
    api_print("%u-%02u-%02u %02u:%02u:%02u UTC\n",
              2000 + year, month, day, hour, min, sec);
    return 0;
}

static int cmd_users(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    api_print("  UID  NAME        HOME\n");
    for (int i = 0; i < user_count(); i++) {
        user_t* u = user_at(i);
        api_print("%5u  %-10s  %s\n", u->uid, u->name, u->home);
    }
    return 0;
}

static int cmd_adduser(int argc, char** argv)
{
    if (!user_is_root(api_user())) {
        api_print("adduser: permission denied (root only)\n");
        return 1;
    }
    if (argc < 2) {
        api_print("usage: adduser <name>\n");
        return 1;
    }
    if (user_find(argv[1])) {
        api_print("adduser: user %s already exists\n", argv[1]);
        return 1;
    }
    char p1[ORBIT_MAX_NAME];
    char p2[ORBIT_MAX_NAME];
    api_print("password: ");
    api_read_secret(p1, sizeof(p1));
    api_print("retype password: ");
    api_read_secret(p2, sizeof(p2));
    if (strcmp(p1, p2) != 0) {
        api_print("adduser: passwords do not match\n");
        return 1;
    }
    if (user_add(argv[1], p1) < 0) {
        api_print("adduser: cannot create user\n");
        return 1;
    }
    user_t* u = user_find(argv[1]);
    fs_mkdir(fs_root(), u->home, u->uid);
    klog("USER", "added user=%s uid=%u", u->name, u->uid);
    api_print("user %s created (uid %u, home %s)\n", u->name, u->uid, u->home);
    return 0;
}

static int cmd_deluser(int argc, char** argv)
{
    if (!user_is_root(api_user())) {
        api_print("deluser: permission denied (root only)\n");
        return 1;
    }
    if (argc < 2) {
        api_print("usage: deluser <name>\n");
        return 1;
    }
    if (strcmp(argv[1], "root") == 0) {
        api_print("deluser: cannot remove root\n");
        return 1;
    }
    if (user_remove(argv[1]) < 0) {
        api_print("deluser: no such user %s\n", argv[1]);
        return 1;
    }
    klog("USER", "removed user=%s", argv[1]);
    api_print("user %s removed\n", argv[1]);
    return 0;
}

static int cmd_history(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    for (int i = 0; i < shell_history_count(); i++)
        api_print("%4d  %s\n", i + 1, shell_history_at(i));
    return 0;
}

static int cmd_shutdown(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    api_print("Orbit shutting down...\n");
    klog("SYS", "shutdown requested");
    power_off();
    return 0;
}

static int cmd_reboot(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    api_print("Orbit rebooting...\n");
    klog("SYS", "reboot requested");
    power_reboot();
    return 0;
}

static const app_t builtin_apps[] = {
    { "help", "list commands or show command help", cmd_help },
    { "version", "show Orbit version", cmd_version },
    { "clear", "clear the terminal", cmd_clear },
    { "mode", "change text resolution (80x25|80x50)", cmd_mode },
    { "echo", "print text", cmd_echo },
    { "pwd", "print working directory", cmd_pwd },
    { "ls", "list directory (-l for details)", cmd_ls },
    { "cd", "change directory", cmd_cd },
    { "mkdir", "create directory", cmd_mkdir },
    { "rm", "remove file or directory (-r)", cmd_rm },
    { "cp", "copy file", cmd_cp },
    { "mv", "move or rename", cmd_mv },
    { "cat", "show file contents", cmd_cat },
    { "touch", "create empty file", cmd_touch },
    { "nano", "simple text editor", cmd_nano },
    { "whoami", "show current user", cmd_whoami },
    { "login", "start a session", cmd_login },
    { "logout", "end the session", cmd_logout },
    { "passwd", "change password", cmd_passwd },
    { "users", "list users", cmd_users },
    { "adduser", "create a user (root)", cmd_adduser },
    { "deluser", "remove a user (root)", cmd_deluser },
    { "ps", "list processes", cmd_ps },
    { "kill", "terminate a process", cmd_kill },
    { "uptime", "show uptime", cmd_uptime },
    { "meminfo", "show heap usage", cmd_meminfo },
    { "date", "show date and time", cmd_date },
    { "history", "show command history", cmd_history },
    { "shutdown", "power off the machine", cmd_shutdown },
    { "reboot", "restart the machine", cmd_reboot },
};

void builtins_register(void)
{
    int count = (int)(sizeof(builtin_apps) / sizeof(builtin_apps[0]));
    for (int i = 0; i < count; i++)
        app_register(&builtin_apps[i]);
}
