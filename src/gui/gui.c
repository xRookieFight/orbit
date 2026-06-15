#include "gui.h"
#include "desktop.h"
#include "orbit.h"

#if ORBIT_DEBUG_SERIAL
#include "serial.h"
#endif

#include "splash.h"

void gui_show_splash(void)
{
    splash_show();
}

void gui_init(void)
{
    desktop_init();
#if ORBIT_DEBUG_SERIAL
    serial_write("[boot] desktop ready\n");
#endif
}

void gui_pump(void)
{
    desktop_pump();
}

int gui_shell_requested(void)
{
    return desktop_shell_requested();
}
