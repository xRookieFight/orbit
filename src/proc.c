#include "proc.h"
#include "string.h"

#define MAX_PROCS 64

static proc_t table[MAX_PROCS];
static int next_pid;

void proc_init(void)
{
    memset(table, 0, sizeof(table));
    next_pid = 0;
    proc_spawn("[kernel]", 0);
    proc_spawn("init", 0);
}

int proc_spawn(const char* name, uint16_t owner)
{
    for (int i = 0; i < MAX_PROCS; i++) {
        if (!table[i].used) {
            table[i].pid = next_pid++;
            strlcpy(table[i].name, name, ORBIT_MAX_NAME);
            table[i].state = PROC_RUNNING;
            table[i].owner = owner;
            table[i].used = 1;
            return table[i].pid;
        }
    }
    return -1;
}

void proc_exit(int pid)
{
    for (int i = 0; i < MAX_PROCS; i++)
        if (table[i].used && table[i].pid == pid)
            table[i].used = 0;
}

int proc_kill(int pid)
{
    if (pid <= 1)
        return -1;
    for (int i = 0; i < MAX_PROCS; i++) {
        if (table[i].used && table[i].pid == pid) {
            table[i].used = 0;
            return 0;
        }
    }
    return -2;
}

int proc_count(void)
{
    int n = 0;
    for (int i = 0; i < MAX_PROCS; i++)
        if (table[i].used)
            n++;
    return n;
}

proc_t* proc_at(int index)
{
    int n = 0;
    for (int i = 0; i < MAX_PROCS; i++) {
        if (table[i].used) {
            if (n == index)
                return &table[i];
            n++;
        }
    }
    return NULL;
}

const char* proc_state_name(proc_state_t state)
{
    switch (state) {
    case PROC_RUNNING: return "running";
    case PROC_SLEEPING: return "sleeping";
    case PROC_ZOMBIE: return "zombie";
    }
    return "unknown";
}
