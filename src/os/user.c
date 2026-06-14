#include "user.h"
#include "fmt.h"
#include "string.h"

#define MAX_USERS 16

static user_t table[MAX_USERS];
static user_t* current;

static uint32_t hash_password(const char* s)
{
    uint32_t h = 5381;
    while (*s)
        h = ((h << 5) + h) + (uint8_t)*s++;
    return h;
}

void user_init(void)
{
    memset(table, 0, sizeof(table));
    strlcpy(table[0].name, "root", ORBIT_MAX_NAME);
    table[0].uid = 0;
    table[0].passhash = hash_password("");
    strlcpy(table[0].home, "/home/root", ORBIT_MAX_PATH);
    table[0].used = 1;
    current = &table[0];
}

user_t* user_current(void)
{
    return current;
}

void user_set_current(user_t* user)
{
    if (user)
        current = user;
}

user_t* user_find(const char* name)
{
    for (int i = 0; i < MAX_USERS; i++)
        if (table[i].used && strcmp(table[i].name, name) == 0)
            return &table[i];
    return NULL;
}

user_t* user_authenticate(const char* name, const char* password)
{
    user_t* user = user_find(name);
    if (user && user->passhash == hash_password(password))
        return user;
    return NULL;
}

int user_add(const char* name, const char* password)
{
    if (user_find(name))
        return -1;
    for (int i = 0; i < MAX_USERS; i++) {
        if (!table[i].used) {
            strlcpy(table[i].name, name, ORBIT_MAX_NAME);
            table[i].uid = (uint16_t)(i + 1000);
            table[i].passhash = hash_password(password);
            ksnprintf(table[i].home, ORBIT_MAX_PATH, "/home/%s", name);
            table[i].used = 1;
            return 0;
        }
    }
    return -2;
}

int user_remove(const char* name)
{
    user_t* user = user_find(name);
    if (!user || user->uid == 0)
        return -1;
    if (current == user)
        current = &table[0];
    memset(user, 0, sizeof(user_t));
    return 0;
}

int user_set_password(user_t* user, const char* password)
{
    if (!user)
        return -1;
    user->passhash = hash_password(password);
    return 0;
}

int user_count(void)
{
    int n = 0;
    for (int i = 0; i < MAX_USERS; i++)
        if (table[i].used)
            n++;
    return n;
}

int user_count_nonroot(void)
{
    int n = 0;
    for (int i = 0; i < MAX_USERS; i++)
        if (table[i].used && table[i].uid != 0)
            n++;
    return n;
}

user_t* user_at(int index)
{
    int n = 0;
    for (int i = 0; i < MAX_USERS; i++) {
        if (table[i].used) {
            if (n == index)
                return &table[i];
            n++;
        }
    }
    return NULL;
}

int user_is_root(user_t* user)
{
    return user && user->uid == 0;
}
