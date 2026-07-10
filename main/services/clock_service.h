#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "rtc_pcf85063.h"

typedef struct {
    bool started;
    bool rtc_present;
    bool time_valid;
    bool oscillator_stopped;
    rtc_datetime_t datetime;
    esp_err_t last_error;
    uint32_t successful_reads;
    uint32_t read_errors;
} clock_service_snapshot_t;

esp_err_t clock_service_start(void);
void clock_service_get_snapshot(clock_service_snapshot_t *snapshot);
esp_err_t clock_service_set_datetime(const rtc_datetime_t *datetime);
