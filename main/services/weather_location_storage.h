/*
 * Moduł weather_location_storage.h należy do warstwy głównej programu SmartTank.
 * Udostępnia typy, stałe i funkcje używane przez pozostałe części aplikacji.
 * Oddzielenie tej odpowiedzialności ułatwia diagnostykę, testy i dalszą rozbudowę.
 */
#pragma once

#include "esp_err.h"
#include "nvs.h"
#include "weather_location.h"

esp_err_t weather_location_storage_load(weather_location_t *location);
esp_err_t weather_location_storage_save(const weather_location_t *location);
esp_err_t weather_location_storage_clear(void);
