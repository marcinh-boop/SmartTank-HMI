#include "app.h"
#include "lvgl_port.h"
#include "screen_dashboard.h"

void app_start(void)
{
    if (lvgl_port_lock(-1)) {
        screen_dashboard_create();
        lvgl_port_unlock();
    }
}
