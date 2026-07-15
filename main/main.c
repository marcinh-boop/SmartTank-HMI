/*
 * Plik main.c: część infrastruktury SmartTank łącząca sprzęt ESP32-S3 z aplikacją i interfejsem LVGL.
 * Implementacja ukrywa szczegóły działania; inne moduły powinny korzystać z odpowiadającego jej API.
 * Oddzielenie odpowiedzialności ułatwia testowanie, diagnostykę i późniejszą rozbudowę urządzenia.
 */
#include "waveshare_rgb_lcd_port.h"
#include "app.h"

void app_main(void)
{
    waveshare_esp32_s3_rgb_lcd_init();
    app_start();
}
