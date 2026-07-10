#include "app.h"
#include "lvgl_port.h"
#include "screen_display_test.h"

void app_start(void)
{
    if (lvgl_port_lock(-1)) {
        screen_display_test_create();
        lvgl_port_unlock();
    }
}
