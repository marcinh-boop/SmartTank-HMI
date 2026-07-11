#pragma once

#include "esp_err.h"
#include "nvs.h"
#include "weather_location.h"

esp_err_t weather_location_storage_load(weather_location_t *location);
esp_err_t weather_location_storage_save(const weather_location_t *location);
esp_err_t weather_location_storage_clear(void);
