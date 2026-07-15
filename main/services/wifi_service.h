/*
 * Moduł wifi_service.h należy do warstwy głównej programu SmartTank.
 * Udostępnia typy, stałe i funkcje używane przez pozostałe części aplikacji.
 * Oddzielenie tej odpowiedzialności ułatwia diagnostykę, testy i dalszą rozbudowę.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define WIFI_SERVICE_MAX_APS          8U
#define WIFI_SERVICE_SSID_MAX_LEN     32U
#define WIFI_SERVICE_PASSWORD_MAX_LEN 64U

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
    bool connecting;
    bool connected;
    uint8_t mac[6];
    char ssid[WIFI_SERVICE_SSID_MAX_LEN + 1U];
    char ip_address[16];
    int8_t rssi;
    uint8_t retry_count;
    uint16_t last_disconnect_reason;
    uint16_t total_ap_count;
    uint8_t ap_count;
    uint32_t scan_revision;
    esp_err_t last_error;
    wifi_service_ap_t aps[WIFI_SERVICE_MAX_APS];
} wifi_service_snapshot_t;

esp_err_t wifi_service_start(void);
esp_err_t wifi_service_request_scan(void);
esp_err_t wifi_service_connect(const char *ssid, const char *password);
esp_err_t wifi_service_disconnect(void);
void wifi_service_get_snapshot(wifi_service_snapshot_t *snapshot);
