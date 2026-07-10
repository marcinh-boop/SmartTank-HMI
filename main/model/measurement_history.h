#pragma once

#include <stdint.h>

#include "esp_err.h"

#define MEASUREMENT_HISTORY_CAPACITY 24U

typedef struct {
    uint32_t uptime_seconds;
    int tank_percent;
    int well_percent;
} measurement_history_sample_t;

typedef struct {
    measurement_history_sample_t samples[MEASUREMENT_HISTORY_CAPACITY];
    uint32_t count;
    uint32_t revision;
} measurement_history_snapshot_t;

esp_err_t measurement_history_init(void);
void measurement_history_add(
    int tank_percent,
    float well_water_column_m,
    float well_depth_m,
    uint32_t uptime_seconds
);
void measurement_history_get_snapshot(measurement_history_snapshot_t *snapshot);
