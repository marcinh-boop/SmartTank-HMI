#include "waveshare_rgb_lcd_port.h"
#include "app.h"

void app_main(void)
{
    waveshare_esp32_s3_rgb_lcd_init();
    app_start();
}