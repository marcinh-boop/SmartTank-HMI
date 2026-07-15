/*
 * Moduł weather_geocoding.h należy do warstwy głównej programu SmartTank.
 * Udostępnia typy, stałe i funkcje używane przez pozostałe części aplikacji.
 * Oddzielenie tej odpowiedzialności ułatwia diagnostykę, testy i dalszą rozbudowę.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "weather_location.h"

#define WEATHER_GEOCODING_MAX_RESULTS  5U
#define WEATHER_GEOCODING_QUERY_MAX_LEN 64U

typedef struct {
    bool started;
    bool searching;
    bool waiting_for_wifi;
    char query[WEATHER_GEOCODING_QUERY_MAX_LEN + 1U];
    weather_location_t results[WEATHER_GEOCODING_MAX_RESULTS];
    uint8_t result_count;
    int http_status;
    esp_err_t last_error;
    uint32_t success_count;
    uint32_t error_count;
    uint32_t revision;
} weather_geocoding_snapshot_t;

esp_err_t weather_geocoding_start(void);
esp_err_t weather_geocoding_request(const char *query);
void weather_geocoding_get_snapshot(weather_geocoding_snapshot_t *snapshot);
