#pragma once

typedef int (*app_main_t)(int argc, char** argv);

typedef struct {
    const char* name;
    const char* summary;
    app_main_t main;
} app_t;

void app_registry_init(void);
int app_register(const app_t* app);
const app_t* app_find(const char* name);
const app_t* app_at(int index);
int app_count(void);
