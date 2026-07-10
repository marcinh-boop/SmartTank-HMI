#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "esp_err.h"

#define NTP_SERVICE_SERVER_MAX_LEN 64U

typedef struct {
    bool started;
    bool initialized;
    bool waiting_for_wifi;
    bool synchronizing;
    bool synchronized;
    bool rtc_updated;
    char server[NTP_SERVICE_SERVER_MAX_LEN];
    uint32_t sync_count;
    time_t last_sync_epoch;
    esp_err_t last_error;
    esp_err_t rtc_error;
} ntp_service_snapshot_t;

esp_err_t ntp_service_start(void);
void ntp_service_get_snapshot(ntp_service_snapshot_t *snapshot);
