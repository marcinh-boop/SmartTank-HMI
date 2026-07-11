#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define WEATHER_SERVICE_LOCATION_NAME_MAX_LEN 95U

typedef struct {
    bool started;
    bool fetching;
    bool waiting_for_wifi;
    bool waiting_for_location;
    char location[WEATHER_SERVICE_LOCATION_NAME_MAX_LEN + 1U];
    int http_status;
    esp_err_t last_error;
    int64_t last_update_epoch;
    uint32_t success_count;
    uint32_t error_count;
    uint32_t revision;
} weather_service_snapshot_t;

esp_err_t weather_service_start(void);
esp_err_t weather_service_request_refresh(void);
void weather_service_get_snapshot(weather_service_snapshot_t *snapshot);
