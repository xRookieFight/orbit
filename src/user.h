#pragma once
#include <stdint.h>
#include "orbit.h"

typedef struct {
    char name[ORBIT_MAX_NAME];
    uint16_t uid;
    uint32_t passhash;
    char home[ORBIT_MAX_PATH];
    int used;
} user_t;

void user_init(void);
user_t* user_current(void);
void user_set_current(user_t* user);
user_t* user_find(const char* name);
user_t* user_authenticate(const char* name, const char* password);
int user_add(const char* name, const char* password);
int user_remove(const char* name);
int user_set_password(user_t* user, const char* password);
int user_count(void);
int user_count_nonroot(void);
user_t* user_at(int index);
int user_is_root(user_t* user);
