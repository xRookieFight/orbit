#pragma once
#include "wm.h"

typedef struct {
    const char* name;
    window_t* (*create)(void);
} gui_app_t;

#define REGISTER_GUI_APP(nm, fn)                                  \
    static const gui_app_t __gui_app_##fn                         \
        __attribute__((used, section("gui_apps"), aligned(8))) =  \
            { nm, fn }

void appreg_init_all(void);
int appreg_count(void);
const char* appreg_name(int i);
window_t* appreg_window(int i);
