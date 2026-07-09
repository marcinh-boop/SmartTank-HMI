#include "waveshare_rgb_lcd_port.h"
#include "lvgl_port.h"

void example_lvgl_demo_ui(void);

void app_main(void)
{
    waveshare_esp32_s3_rgb_lcd_init();

    if (lvgl_port_lock(-1)) {
        example_lvgl_demo_ui();
        lvgl_port_unlock();
    }
}