#pragma once
#include "wm.h"

void desktop_init(void);
void desktop_pump(void);
void desktop_mark_dirty(void);
int desktop_active(void);
window_t* desktop_terminal(void);
