#include "app.h"
#include "string.h"

#define MAX_APPS 64

static const app_t* registry[MAX_APPS];
static int registry_count;

void app_registry_init(void)
{
    registry_count = 0;
}

int app_register(const app_t* app)
{
    if (registry_count >= MAX_APPS)
        return -1;
    registry[registry_count++] = app;
    return 0;
}

const app_t* app_find(const char* name)
{
    for (int i = 0; i < registry_count; i++)
        if (strcmp(registry[i]->name, name) == 0)
            return registry[i];
    return NULL;
}

const app_t* app_at(int index)
{
    if (index < 0 || index >= registry_count)
        return NULL;
    return registry[index];
}

int app_count(void)
{
    return registry_count;
}
