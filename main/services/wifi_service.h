#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define WIFI_SERVICE_MAX_APS       8U
#define WIFI_SERVICE_SSID_MAX_LEN  32U

typedef struct {
    char ssid[WIFI_SERVICE_SSID_MAX_LEN + 1U];
    int8_t rssi;
    uint8_t channel;
    bool secured;
} wifi_service_ap_t;

typedef struct {
    bool started;
    bool radio_ready;
    bool scanning;
    bool configured;
    bool connected;
    uint8_t mac[6];
    char ip_address[16];
    uint16_t total_ap_count;
    uint8_t ap_count;
    uint32_t scan_revision;
    esp_err_t last_error;
    wifi_service_ap_t aps[WIFI_SERVICE_MAX_APS];
} wifi_service_snapshot_t;

esp_err_t wifi_service_start(void);
esp_err_t wifi_service_request_scan(void);
void wifi_service_get_snapshot(wifi_service_snapshot_t *snapshot);
