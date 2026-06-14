#pragma once
#include <stdint.h>
#include "orbit.h"

typedef enum {
    PROC_RUNNING,
    PROC_SLEEPING,
    PROC_ZOMBIE
} proc_state_t;

typedef struct {
    int pid;
    char name[ORBIT_MAX_NAME];
    proc_state_t state;
    uint16_t owner;
    int used;
} proc_t;

void proc_init(void);
int proc_spawn(const char* name, uint16_t owner);
void proc_exit(int pid);
int proc_kill(int pid);
int proc_count(void);
proc_t* proc_at(int index);
const char* proc_state_name(proc_state_t state);
