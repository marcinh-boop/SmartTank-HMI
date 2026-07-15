/*
 * Moduł clock_service.h należy do warstwy głównej programu SmartTank.
 * Udostępnia typy, stałe i funkcje używane przez pozostałe części aplikacji.
 * Oddzielenie tej odpowiedzialności ułatwia diagnostykę, testy i dalszą rozbudowę.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "esp_err.h"
#include "rtc_pcf85063.h"

typedef enum {
    CLOCK_TIME_SOURCE_NONE = 0,
    CLOCK_TIME_SOURCE_RTC,
    CLOCK_TIME_SOURCE_NTP,
    CLOCK_TIME_SOURCE_MANUAL,
} clock_time_source_t;

typedef struct {
    bool started;
    bool rtc_present;
    bool time_valid;
    bool oscillator_stopped;
    bool system_time_valid;
    clock_time_source_t source;
    rtc_datetime_t datetime;
    rtc_datetime_t local_datetime;
    time_t last_ntp_sync_epoch;
    esp_err_t last_error;
    uint32_t successful_reads;
    uint32_t read_errors;
} clock_service_snapshot_t;

esp_err_t clock_service_start(void);
void clock_service_get_snapshot(clock_service_snapshot_t *snapshot);
esp_err_t clock_service_set_datetime(const rtc_datetime_t *datetime);
esp_err_t clock_service_accept_ntp_time(time_t epoch);
const char *clock_service_source_name(clock_time_source_t source);
