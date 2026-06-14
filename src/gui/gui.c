#include "gui.h"
#include "desktop.h"
#include "serial.h"
#include "splash.h"

void gui_show_splash(void)
{
    splash_show();
}

void gui_init(void)
{
    desktop_init();
    serial_write("[boot] desktop ready\n");
}
