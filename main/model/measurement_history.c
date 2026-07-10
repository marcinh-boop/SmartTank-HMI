#include "measurement_history.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static SemaphoreHandle_t s_history_mutex;
static measurement_history_sample_t s_samples[MEASUREMENT_HISTORY_CAPACITY];
static uint32_t s_count;
static uint32_t s_next_index;
static uint32_t s_revision;

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
    return ESP_OK;
}

void measurement_history_add(
    int tank_percent,
    float well_water_column_m,
    float well_depth_m,
    uint32_t uptime_seconds)
{
    if (s_history_mutex == NULL || well_depth_m <= 0.0f) {
        return;
    }

    int well_percent = (int)((well_water_column_m / well_depth_m) * 100.0f + 0.5f);

    measurement_history_sample_t sample = {
        .uptime_seconds = uptime_seconds,
        .tank_percent = clamp_percent(tank_percent),
        .well_percent = clamp_percent(well_percent),
    };

    if (xSemaphoreTake(s_history_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }

    s_samples[s_next_index] = sample;
    s_next_index = (s_next_index + 1U) % MEASUREMENT_HISTORY_CAPACITY;

    if (s_count < MEASUREMENT_HISTORY_CAPACITY) {
        s_count++;
    }

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
