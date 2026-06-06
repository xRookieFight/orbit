#include "shell.h"
#include "console.h"
#include "api.h"
#include "app.h"
#include "fs.h"
#include "user.h"
#include "proc.h"
#include "log.h"
#include "fmt.h"
#include "string.h"
#include "orbit.h"

#define HISTORY_SIZE 16

static char history[HISTORY_SIZE][ORBIT_MAX_LINE];
static int history_count;
static int logout_requested;

static void history_push(const char* line)
{
    if (line[0] == '\0')
        return;
    if (history_count < HISTORY_SIZE) {
        strlcpy(history[history_count++], line, ORBIT_MAX_LINE);
        return;
    }
    for (int i = 1; i < HISTORY_SIZE; i++)
        strlcpy(history[i - 1], history[i], ORBIT_MAX_LINE);
    strlcpy(history[HISTORY_SIZE - 1], line, ORBIT_MAX_LINE);
}

int shell_history_count(void)
{
    return history_count;
}

const char* shell_history_at(int index)
{
    if (index < 0 || index >= history_count)
        return "";
    return history[index];
}

void shell_request_logout(void)
{
    logout_requested = 1;
}

static int tokenize(char* line, char** argv)
{
    int argc = 0;
    char* p = line;
    while (*p && argc < ORBIT_MAX_ARGS - 1) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (!*p)
            break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t')
            p++;
        if (*p)
            *p++ = '\0';
    }
    argv[argc] = NULL;
    return argc;
}

static void build_prompt(char* buf, uint32_t size)
{
    char path[ORBIT_MAX_PATH];
    fs_abspath(api_cwd(), path, sizeof(path));
    user_t* u = api_user();
    char symbol = user_is_root(u) ? '#' : '$';
    ksnprintf(buf, size, "orbit:%s %s%c ", path, u->name, symbol);
}

static void run_line(char* line)
{
    char* argv[ORBIT_MAX_ARGS];
    int argc = tokenize(line, argv);
    if (argc == 0)
        return;

    fs_node_t* redirect = NULL;
    int append = 0;
    int effective_argc = argc;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], ">") == 0 || strcmp(argv[i], ">>") == 0) {
            append = (argv[i][1] == '>');
            if (i + 1 >= argc) {
                console_printf("orbit: syntax error near redirection\n");
                return;
            }
            const char* filename = argv[i + 1];
            fs_node_t* file = fs_resolve(api_cwd(), filename);
            if (!file)
                file = fs_create(api_cwd(), filename, FS_FILE, api_user()->uid);
            if (!file) {
                console_printf("orbit: cannot open %s\n", filename);
                return;
            }
            redirect = file;
            effective_argc = i;
            break;
        }
    }
    argv[effective_argc] = NULL;
    if (effective_argc == 0)
        return;

    const app_t* app = app_find(argv[0]);
    if (!app) {
        console_printf("orbit: %s: command not found\n", argv[0]);
        return;
    }

    if (redirect) {
        if (!append)
            fs_write(redirect, "", 0);
        api_redirect_begin(redirect, append);
    }
    app->main(effective_argc, argv);
    if (redirect)
        api_redirect_end();
}

void shell_login(void)
{
    for (;;) {
        char name[ORBIT_MAX_NAME];
        console_write("login: ");
        console_readline(name, sizeof(name));
        if (name[0] == '\0')
            continue;

        char password[ORBIT_MAX_NAME];
        console_write("password: ");
        api_read_secret(password, sizeof(password));

        user_t* user = user_authenticate(name, password);
        if (user) {
            user_set_current(user);
            fs_node_t* home = fs_resolve(fs_root(), user->home);
            api_set_cwd(home ? home : fs_root());
            klog("AUTH", "login user=%s", user->name);
            console_printf("\n");
            return;
        }
        klog("AUTH", "failed login user=%s", name);
        console_printf("Login incorrect\n");
    }
}

static void session_banner(void)
{
    fs_node_t* motd = fs_resolve(fs_root(), "/etc/motd");
    if (motd && motd->data)
        console_write(motd->data);
}

void shell_run(void)
{
    proc_spawn("shell", api_user()->uid);
    session_banner();
    console_printf("Logged in as root (auto session, no users configured).\n");
    console_printf("Type 'help' for commands, 'shutdown' to power off.\n\n");

    for (;;) {
        logout_requested = 0;
        while (!logout_requested) {
            char prompt[ORBIT_MAX_PATH + 64];
            build_prompt(prompt, sizeof(prompt));
            console_write(prompt);

            char line[ORBIT_MAX_LINE];
            console_readline(line, sizeof(line));
            history_push(line);
            run_line(line);
        }

        console_printf("logged out.\n");
        if (user_count_nonroot() > 0) {
            shell_login();
        } else {
            user_set_current(user_find("root"));
            fs_node_t* home = fs_resolve(fs_root(), "/home/root");
            api_set_cwd(home ? home : fs_root());
        }
    }
}
