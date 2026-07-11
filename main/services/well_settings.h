#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    char sensor_model[32];
    char input_mode[16];
    uint8_t analog_channel;
    float distance_empty_mm;
    float distance_full_mm;
    float well_depth_m;
    int warning_percent;
    int critical_percent;
} well_settings_t;

esp_err_t well_settings_init(void);
void well_settings_get(well_settings_t *settings);
esp_err_t well_settings_save(const well_settings_t *settings);
bool well_settings_is_valid(const well_settings_t *settings);
