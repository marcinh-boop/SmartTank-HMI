/*
 * Moduł measurement_history.h należy do warstwy głównej programu SmartTank.
 * Udostępnia typy, stałe i funkcje używane przez pozostałe części aplikacji.
 * Oddzielenie tej odpowiedzialności ułatwia diagnostykę, testy i dalszą rozbudowę.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/* 144 probki co 10 minut daja pelna dobe historii. */
#define MEASUREMENT_HISTORY_CAPACITY 144U
#define MEASUREMENT_HISTORY_INTERVAL_SECONDS 600U

typedef struct {
    uint32_t uptime_seconds;
    int64_t epoch_seconds;
    bool timestamp_valid;
    int tank_percent;
    int well_percent;
    bool tank_valid;
    bool well_valid;
} measurement_history_sample_t;

typedef struct {
    measurement_history_sample_t samples[MEASUREMENT_HISTORY_CAPACITY];
    uint32_t count;
    uint32_t revision;
} measurement_history_snapshot_t;

esp_err_t measurement_history_init(void);
void measurement_history_add(
    int tank_percent,
    bool tank_valid,
    float well_water_column_m,
    float well_depth_m,
    bool well_valid,
    uint32_t uptime_seconds
);
void measurement_history_get_snapshot(measurement_history_snapshot_t *snapshot);
