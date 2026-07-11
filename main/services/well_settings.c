#include "well_settings.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"

#define WELL_SETTINGS_NAMESPACE "smartwell"
#define WELL_SETTINGS_VERSION   1U

#define KEY_VERSION       "version"
#define KEY_SENSOR        "sensor"
#define KEY_INPUT         "input"
#define KEY_CHANNEL       "channel"
#define KEY_EMPTY_MM      "empty_mm"
#define KEY_FULL_MM       "full_mm"
#define KEY_DEPTH_CM      "depth_cm"
#define KEY_WARNING       "warning"
#define KEY_CRITICAL      "critical"

static const char *TAG = "well_settings";
static SemaphoreHandle_t s_mutex;
static well_settings_t s_settings;

static void load_defaults(well_settings_t *settings)
{
    memset(settings, 0, sizeof(*settings));
    strncpy(settings->sensor_model, "mic+130/IU/TC", sizeof(settings->sensor_model) - 1U);
    strncpy(settings->input_mode, "4-20 mA", sizeof(settings->input_mode) - 1U);
    settings->analog_channel = 2U;
    settings->distance_empty_mm = 4000.0f;
    settings->distance_full_mm = 250.0f;
    settings->well_depth_m = 4.00f;
    settings->warning_percent = 30;
    settings->critical_percent = 15;
}

bool well_settings_is_valid(const well_settings_t *settings)
{
    return settings != NULL &&
           settings->sensor_model[0] != '\0' &&
           settings->input_mode[0] != '\0' &&
           settings->analog_channel >= 1U &&
           settings->analog_channel <= 8U &&
           settings->distance_empty_mm > settings->distance_full_mm &&
           settings->distance_empty_mm <= 10000.0f &&
           settings->distance_full_mm >= 0.0f &&
           settings->well_depth_m >= 0.10f &&
           settings->well_depth_m <= 100.0f &&
           settings->critical_percent >= 0 &&
           settings->critical_percent < settings->warning_percent &&
           settings->warning_percent <= 100;
}

static esp_err_t read_from_nvs(well_settings_t *settings)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(WELL_SETTINGS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    well_settings_t loaded = {0};
    uint8_t version = 0U;
    uint8_t channel = 0U;
    uint8_t warning = 0U;
    uint8_t critical = 0U;
    uint32_t empty_mm = 0U;
    uint32_t full_mm = 0U;
    uint32_t depth_cm = 0U;
    size_t sensor_size = sizeof(loaded.sensor_model);
    size_t input_size = sizeof(loaded.input_mode);

    err = nvs_get_u8(handle, KEY_VERSION, &version);
    if (err == ESP_OK && version != WELL_SETTINGS_VERSION) {
        err = ESP_ERR_INVALID_VERSION;
    }
    if (err == ESP_OK) {
        err = nvs_get_str(handle, KEY_SENSOR, loaded.sensor_model, &sensor_size);
    }
    if (err == ESP_OK) {
        err = nvs_get_str(handle, KEY_INPUT, loaded.input_mode, &input_size);
    }
    if (err == ESP_OK) {
        err = nvs_get_u8(handle, KEY_CHANNEL, &channel);
    }
    if (err == ESP_OK) {
        err = nvs_get_u32(handle, KEY_EMPTY_MM, &empty_mm);
    }
    if (err == ESP_OK) {
        err = nvs_get_u32(handle, KEY_FULL_MM, &full_mm);
    }
    if (err == ESP_OK) {
        err = nvs_get_u32(handle, KEY_DEPTH_CM, &depth_cm);
    }
    if (err == ESP_OK) {
        err = nvs_get_u8(handle, KEY_WARNING, &warning);
    }
    if (err == ESP_OK) {
        err = nvs_get_u8(handle, KEY_CRITICAL, &critical);
    }

    nvs_close(handle);

    if (err != ESP_OK) {
        return err;
    }

    loaded.sensor_model[sizeof(loaded.sensor_model) - 1U] = '\0';
    loaded.input_mode[sizeof(loaded.input_mode) - 1U] = '\0';
    loaded.analog_channel = channel;
    loaded.distance_empty_mm = (float)empty_mm;
    loaded.distance_full_mm = (float)full_mm;
    loaded.well_depth_m = (float)depth_cm / 100.0f;
    loaded.warning_percent = (int)warning;
    loaded.critical_percent = (int)critical;

    if (!well_settings_is_valid(&loaded)) {
        return ESP_ERR_INVALID_STATE;
    }

    *settings = loaded;
    return ESP_OK;
}

esp_err_t well_settings_init(void)
{
    if (s_mutex != NULL) {
        return ESP_OK;
    }

    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    load_defaults(&s_settings);

    well_settings_t stored;
    const esp_err_t err = read_from_nvs(&stored);
    if (err == ESP_OK) {
        s_settings = stored;
        ESP_LOGI(TAG, "Well calibration loaded from NVS");
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved well calibration; using defaults");
    } else {
        ESP_LOGW(TAG, "Unable to load well calibration: %s", esp_err_to_name(err));
    }

    return ESP_OK;
}

void well_settings_get(well_settings_t *settings)
{
    if (settings == NULL) {
        return;
    }

    if (s_mutex == NULL || xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        load_defaults(settings);
        return;
    }

    *settings = s_settings;
    xSemaphoreGive(s_mutex);
}

esp_err_t well_settings_save(const well_settings_t *settings)
{
    if (!well_settings_is_valid(settings)) {
        return ESP_ERR_INVALID_ARG;
    }

    well_settings_t safe = *settings;
    safe.sensor_model[sizeof(safe.sensor_model) - 1U] = '\0';
    safe.input_mode[sizeof(safe.input_mode) - 1U] = '\0';

    nvs_handle_t handle;
    esp_err_t err = nvs_open(WELL_SETTINGS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_u8(handle, KEY_VERSION, WELL_SETTINGS_VERSION);
    if (err == ESP_OK) err = nvs_set_str(handle, KEY_SENSOR, safe.sensor_model);
    if (err == ESP_OK) err = nvs_set_str(handle, KEY_INPUT, safe.input_mode);
    if (err == ESP_OK) err = nvs_set_u8(handle, KEY_CHANNEL, safe.analog_channel);
    if (err == ESP_OK) err = nvs_set_u32(handle, KEY_EMPTY_MM, (uint32_t)(safe.distance_empty_mm + 0.5f));
    if (err == ESP_OK) err = nvs_set_u32(handle, KEY_FULL_MM, (uint32_t)(safe.distance_full_mm + 0.5f));
    if (err == ESP_OK) err = nvs_set_u32(handle, KEY_DEPTH_CM, (uint32_t)(safe.well_depth_m * 100.0f + 0.5f));
    if (err == ESP_OK) err = nvs_set_u8(handle, KEY_WARNING, (uint8_t)safe.warning_percent);
    if (err == ESP_OK) err = nvs_set_u8(handle, KEY_CRITICAL, (uint8_t)safe.critical_percent);
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        return err;
    }

    if (xSemaphoreTake(s_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }
    s_settings = safe;
    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "Well calibration saved to NVS");
    return ESP_OK;
}
