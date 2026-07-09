#include "app.h"
#include "lvgl_port.h"
#include "theme.h"
#include "screen_boot.h"

void app_start(void)
{
    if (lvgl_port_lock(-1)) {
        theme_init();
        screen_boot_create();
        lvgl_port_unlock();
    }
}