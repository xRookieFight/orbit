#include "appreg.h"

extern const gui_app_t __gui_apps_start[];
extern const gui_app_t __gui_apps_end[];

#define MAX_GUI_APPS 32

static window_t* wins[MAX_GUI_APPS];
static const gui_app_t* defs[MAX_GUI_APPS];
static int n;

void appreg_init_all(void)
{
    n = 0;
    for (const gui_app_t* a = __gui_apps_start; a < __gui_apps_end && n < MAX_GUI_APPS; a++) {
        defs[n] = a;
        wins[n] = a->create();
        n++;
    }
}

int appreg_count(void)
{
    return n;
}

const char* appreg_name(int i)
{
    return (i >= 0 && i < n) ? defs[i]->name : "";
}

window_t* appreg_window(int i)
{
    return (i >= 0 && i < n) ? wins[i] : 0;
}
