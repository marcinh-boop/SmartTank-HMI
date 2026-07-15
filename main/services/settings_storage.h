/*
 * Publiczne API pamięci trwałej konfiguracji szamba.
 * Oddziela model aplikacji od szczegółów kluczy i przestrzeni nazw NVS.
 */
#pragma once

#include "app_model.h"
#include "esp_err.h"

#define SETTINGS_WIFI_SSID_MAX_LEN      32U
#define SETTINGS_WIFI_PASSWORD_MAX_LEN  64U

typedef struct {
    char ssid[SETTINGS_WIFI_SSID_MAX_LEN + 1U];
    char password[SETTINGS_WIFI_PASSWORD_MAX_LEN + 1U];
} wifi_credentials_t;

esp_err_t settings_storage_init(void);
esp_err_t settings_storage_load_tank_config(tank_channel_config_t *config);
esp_err_t settings_storage_save_tank_config(const tank_channel_config_t *config);
esp_err_t settings_storage_load_wifi_credentials(wifi_credentials_t *credentials);
esp_err_t settings_storage_save_wifi_credentials(const wifi_credentials_t *credentials);
esp_err_t settings_storage_clear_wifi_credentials(void);
