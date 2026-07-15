/*
 * Publiczny interfejs aktualizacji OTA przez sieć lokalną.
 * Serwis uruchamia stronę WWW dopiero po uzyskaniu połączenia Wi-Fi i zapisuje
 * przesłany obraz firmware do nieaktywnej partycji ota_0 albo ota_1.
 */
#pragma once

#include "esp_err.h"

esp_err_t ota_service_start(void);
esp_err_t ota_service_confirm_running_firmware(void);
