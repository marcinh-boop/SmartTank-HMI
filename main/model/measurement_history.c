/*
 * Moduł measurement_history.c należy do warstwy głównej programu SmartTank.
 * Realizuje logikę modułu i ukrywa jej szczegóły za publicznym interfejsem.
 * Oddzielenie tej odpowiedzialności ułatwia diagnostykę, testy i dalszą rozbudowę.
 */
#include "measurement_history.h"

#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define HISTORY_MIN_VALID_YEAR 2024

static SemaphoreHandle_t s_history_mutex;
static measurement_history_sample_t s_samples[MEASUREMENT_HISTORY_CAPACITY];
static uint32_t s_count;
static uint32_t s_next_index;
static uint32_t s_revision;
static uint32_t s_last_sample_uptime;
static bool s_has_sample;

static int clamp_percent(int value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 100) {
        return 100;
    }
    return value;
}

static bool capture_timestamp(int64_t *epoch_seconds)
{
    if (epoch_seconds == NULL) {
        return false;
    }

    time_t now = 0;
    time(&now);

    struct tm local_time = {0};
    if (localtime_r(&now, &local_time) == NULL ||
        local_time.tm_year + 1900 < HISTORY_MIN_VALID_YEAR) {
        *epoch_seconds = 0;
        return false;
    }

    *epoch_seconds = (int64_t)now;
    return true;
}

esp_err_t measurement_history_init(void)
{
    if (s_history_mutex != NULL) {
        return ESP_OK;
    }

    s_history_mutex = xSemaphoreCreateMutex();
    if (s_history_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    memset(s_samples, 0, sizeof(s_samples));
    s_count = 0;
    s_next_index = 0;
    s_revision = 1;
    s_last_sample_uptime = 0;
    s_has_sample = false;
    return ESP_OK;
}

void measurement_history_add(
    int tank_percent,
    bool tank_valid,
    float well_water_column_m,
    float well_depth_m,
    bool well_valid,
    uint32_t uptime_seconds)
{
    if (s_history_mutex == NULL) {
        return;
    }

    int well_percent = 0;
    if (well_depth_m > 0.0f) {
        well_percent = (int)((well_water_column_m / well_depth_m) * 100.0f + 0.5f);
    } else {
        well_valid = false;
    }

    measurement_history_sample_t sample = {
        .uptime_seconds = uptime_seconds,
        .tank_percent = clamp_percent(tank_percent),
        .well_percent = clamp_percent(well_percent),
        .tank_valid = tank_valid,
        .well_valid = well_valid,
    };
    sample.timestamp_valid = capture_timestamp(&sample.epoch_seconds);

    if (xSemaphoreTake(s_history_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }

    /* Serwis odpytuje modul co sekunde, lecz historia zapisuje punkt co 10 minut. */
    if (s_has_sample &&
        (uint32_t)(uptime_seconds - s_last_sample_uptime) <
            MEASUREMENT_HISTORY_INTERVAL_SECONDS) {
        xSemaphoreGive(s_history_mutex);
        return;
    }

    s_samples[s_next_index] = sample;
    s_next_index = (s_next_index + 1U) % MEASUREMENT_HISTORY_CAPACITY;

    if (s_count < MEASUREMENT_HISTORY_CAPACITY) {
        s_count++;
    }

    s_last_sample_uptime = uptime_seconds;
    s_has_sample = true;
    s_revision++;
    xSemaphoreGive(s_history_mutex);
}

void measurement_history_get_snapshot(measurement_history_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return;
    }

    memset(snapshot, 0, sizeof(*snapshot));

    if (s_history_mutex == NULL ||
        xSemaphoreTake(s_history_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }

    snapshot->count = s_count;
    snapshot->revision = s_revision;

    const uint32_t oldest_index =
        s_count == MEASUREMENT_HISTORY_CAPACITY ? s_next_index : 0U;

    for (uint32_t i = 0; i < s_count; i++) {
        const uint32_t source_index =
            (oldest_index + i) % MEASUREMENT_HISTORY_CAPACITY;
        snapshot->samples[i] = s_samples[source_index];
    }

    xSemaphoreGive(s_history_mutex);
}
