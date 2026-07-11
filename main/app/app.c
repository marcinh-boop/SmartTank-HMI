#include "app.h"

#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"

#include "alarm_service.h"
#include "analog_module_service.h"
#include "app_model.h"
#include "measurement_history.h"
#include "clock_service.h"
#include "data_simulator.h"
#include "ntp_service.h"
#include "settings_storage.h"
#include "weather_geocoding.h"
#include "weather_service.h"
#include "well_settings.h"
#include "wifi_service.h"
#include "lvgl_port.h"
#include "panel_icons.h"
#include "screen_dashboard.h"
#include "top_status_bar.h"
#include "well_detail_integration.h"

static const char *TAG = "app";

/* Zmienimy na true dopiero po sprawdzeniu przewodów A/B i zasilania modułu. */
static const bool ANALOG_MODULE_HARDWARE_ENABLED = false;

void app_start(void)
{
    ESP_ERROR_CHECK(app_model_init());
    ESP_ERROR_CHECK(settings_storage_init());
    ESP_ERROR_CHECK(well_settings_init());

    tank_channel_config_t stored_config;
    const esp_err_t load_result =
        settings_storage_load_tank_config(&stored_config);

    if (load_result == ESP_OK) {
        app_model_restore_tank_config(&stored_config);
    } else if (load_result == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved tank configuration; using defaults");
    } else {
        ESP_LOGW(
            TAG,
            "Unable to load tank configuration: %s",
            esp_err_to_name(load_result)
        );
    }

    ESP_ERROR_CHECK(measurement_history_init());
    ESP_ERROR_CHECK(clock_service_start());

    const esp_err_t wifi_result = wifi_service_start();
    if (wifi_result != ESP_OK) {
        ESP_LOGW(
            TAG,
            "Wi-Fi service unavailable: %s",
            esp_err_to_name(wifi_result)
        );
    } else {
        wifi_credentials_t credentials;
        const esp_err_t credentials_result =
            settings_storage_load_wifi_credentials(&credentials);

        if (credentials_result == ESP_OK) {
            const esp_err_t connect_result = wifi_service_connect(
                credentials.ssid,
                credentials.password
            );

            if (connect_result != ESP_OK) {
                ESP_LOGW(
                    TAG,
                    "Unable to restore Wi-Fi connection: %s",
                    esp_err_to_name(connect_result)
                );
            }
        } else if (credentials_result == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "No saved Wi-Fi network; radio remains ready");
        } else {
            ESP_LOGW(
                TAG,
                "Unable to load Wi-Fi credentials: %s",
                esp_err_to_name(credentials_result)
            );
        }
    }

    const esp_err_t ntp_result = ntp_service_start();
    if (ntp_result != ESP_OK) {
        ESP_LOGW(
            TAG,
            "NTP service unavailable: %s",
            esp_err_to_name(ntp_result)
        );
    }

    const esp_err_t geocoding_result = weather_geocoding_start();
    if (geocoding_result != ESP_OK) {
        ESP_LOGW(
            TAG,
            "Weather geocoding unavailable: %s",
            esp_err_to_name(geocoding_result)
        );
    }

    const esp_err_t weather_result = weather_service_start();
    if (weather_result != ESP_OK) {
        ESP_LOGW(
            TAG,
            "Weather service unavailable: %s",
            esp_err_to_name(weather_result)
        );
    }

    const esp_err_t analog_result = analog_module_service_start(
        ANALOG_MODULE_HARDWARE_ENABLED
    );
    if (analog_result != ESP_OK) {
        ESP_LOGW(
            TAG,
            "Analog module service unavailable: %s",
            esp_err_to_name(analog_result)
        );
    }

    ESP_ERROR_CHECK(data_simulator_start());

    const esp_err_t alarm_result = alarm_service_start();
    if (alarm_result != ESP_OK) {
        ESP_LOGW(
            TAG,
            "Alarm service unavailable: %s",
            esp_err_to_name(alarm_result)
        );
    }

    if (lvgl_port_lock(-1)) {
        screen_dashboard_create();
        panel_icons_apply(lv_scr_act());

        if (!top_status_bar_attach(lv_scr_act())) {
            ESP_LOGW(TAG, "Unable to attach top status bar");
        }

        if (!well_detail_integration_attach(lv_scr_act())) {
            ESP_LOGW(TAG, "Unable to attach well detail screen");
        }

        lvgl_port_unlock();
    }
}
