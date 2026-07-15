/*
 * Zapis i odczyt ustawień szamba w NVS.
 * Przechowuje kanał analogowy, kalibrację PUSTE/PEŁNE, pojemność oraz progi.
 * Numer wersji formatu pozwala bezpiecznie odrzucić niezgodne stare dane.
 */
#include "settings_storage.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#define SETTINGS_NAMESPACE       "smarttank"
#define SETTINGS_VERSION         1U

#define KEY_VERSION              "cfg_ver"
#define KEY_SENSOR_MODEL         "sensor_model"
#define KEY_INPUT_MODE           "input_mode"
#define KEY_ANALOG_CHANNEL       "ai_channel"
#define KEY_DISTANCE_EMPTY_MM    "empty_mm"
#define KEY_DISTANCE_FULL_MM     "full_mm"
#define KEY_CAPACITY_CENTI_M3    "capacity_c"
#define KEY_WARNING_PERCENT      "warn_pct"
#define KEY_CRITICAL_PERCENT     "crit_pct"
#define KEY_WIFI_SSID            "wifi_ssid"
#define KEY_WIFI_PASSWORD        "wifi_password"

static const char *TAG = "settings";

static bool tank_config_is_valid(const tank_channel_config_t *config)
{
    return config != NULL &&
           config->sensor_model[0] != '\0' &&
           config->input_mode[0] != '\0' &&
           config->analog_channel >= 1U &&
           config->analog_channel <= 8U &&
           config->distance_empty_mm > config->distance_full_mm &&
           config->distance_empty_mm <= 10000.0f &&
           config->distance_full_mm >= 0.0f &&
           config->capacity_m3 > 0.0f &&
           config->capacity_m3 <= 100.0f &&
           config->warning_percent >= 1 &&
           config->warning_percent < config->critical_percent &&
           config->critical_percent <= 100;
}

static bool wifi_credentials_are_valid(const wifi_credentials_t *credentials)
{
    if (credentials == NULL) {
        return false;
    }

    const size_t ssid_length = strnlen(
        credentials->ssid,
        sizeof(credentials->ssid)
    );
    const size_t password_length = strnlen(
        credentials->password,
        sizeof(credentials->password)
    );

    return ssid_length > 0U &&
           ssid_length <= SETTINGS_WIFI_SSID_MAX_LEN &&
           password_length <= SETTINGS_WIFI_PASSWORD_MAX_LEN;
}

esp_err_t settings_storage_init(void)
{
    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS requires erase and reinitialization");

        err = nvs_flash_erase();
        if (err != ESP_OK) {
            return err;
        }

        err = nvs_flash_init();
    }

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "NVS settings storage ready");
    }

    return err;
}

esp_err_t settings_storage_load_tank_config(tank_channel_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(SETTINGS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    tank_channel_config_t loaded = {0};
    uint8_t version = 0U;
    uint8_t analog_channel = 0U;
    uint8_t warning_percent = 0U;
    uint8_t critical_percent = 0U;
    uint32_t distance_empty_mm = 0U;
    uint32_t distance_full_mm = 0U;
    uint32_t capacity_centi_m3 = 0U;
    size_t sensor_model_size = sizeof(loaded.sensor_model);
    size_t input_mode_size = sizeof(loaded.input_mode);

    err = nvs_get_u8(handle, KEY_VERSION, &version);
    if (err != ESP_OK) {
        goto finish;
    }

    if (version != SETTINGS_VERSION) {
        err = ESP_ERR_INVALID_VERSION;
        goto finish;
    }

    err = nvs_get_str(
        handle,
        KEY_SENSOR_MODEL,
        loaded.sensor_model,
        &sensor_model_size
    );
    if (err != ESP_OK) {
        goto finish;
    }

    err = nvs_get_str(
        handle,
        KEY_INPUT_MODE,
        loaded.input_mode,
        &input_mode_size
    );
    if (err != ESP_OK) {
        goto finish;
    }

    err = nvs_get_u8(handle, KEY_ANALOG_CHANNEL, &analog_channel);
    if (err != ESP_OK) {
        goto finish;
    }

    err = nvs_get_u32(handle, KEY_DISTANCE_EMPTY_MM, &distance_empty_mm);
    if (err != ESP_OK) {
        goto finish;
    }

    err = nvs_get_u32(handle, KEY_DISTANCE_FULL_MM, &distance_full_mm);
    if (err != ESP_OK) {
        goto finish;
    }

    err = nvs_get_u32(handle, KEY_CAPACITY_CENTI_M3, &capacity_centi_m3);
    if (err != ESP_OK) {
        goto finish;
    }

    err = nvs_get_u8(handle, KEY_WARNING_PERCENT, &warning_percent);
    if (err != ESP_OK) {
        goto finish;
    }

    err = nvs_get_u8(handle, KEY_CRITICAL_PERCENT, &critical_percent);
    if (err != ESP_OK) {
        goto finish;
    }

    loaded.analog_channel = analog_channel;
    loaded.distance_empty_mm = (float)distance_empty_mm;
    loaded.distance_full_mm = (float)distance_full_mm;
    loaded.capacity_m3 = (float)capacity_centi_m3 / 100.0f;
    loaded.warning_percent = (int)warning_percent;
    loaded.critical_percent = (int)critical_percent;

    loaded.sensor_model[sizeof(loaded.sensor_model) - 1U] = '\0';
    loaded.input_mode[sizeof(loaded.input_mode) - 1U] = '\0';

    if (!tank_config_is_valid(&loaded)) {
        err = ESP_ERR_INVALID_STATE;
        goto finish;
    }

    *config = loaded;
    ESP_LOGI(TAG, "Tank configuration loaded from NVS");

finish:
    nvs_close(handle);
    return err;
}

esp_err_t settings_storage_save_tank_config(const tank_channel_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    tank_channel_config_t safe = *config;
    safe.sensor_model[sizeof(safe.sensor_model) - 1U] = '\0';
    safe.input_mode[sizeof(safe.input_mode) - 1U] = '\0';

    if (!tank_config_is_valid(&safe)) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint32_t distance_empty_mm =
        (uint32_t)(safe.distance_empty_mm + 0.5f);
    const uint32_t distance_full_mm =
        (uint32_t)(safe.distance_full_mm + 0.5f);
    const uint32_t capacity_centi_m3 =
        (uint32_t)(safe.capacity_m3 * 100.0f + 0.5f);

    nvs_handle_t handle;
    esp_err_t err = nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_u8(handle, KEY_VERSION, SETTINGS_VERSION);
    if (err != ESP_OK) {
        goto finish;
    }

    err = nvs_set_str(handle, KEY_SENSOR_MODEL, safe.sensor_model);
    if (err != ESP_OK) {
        goto finish;
    }

    err = nvs_set_str(handle, KEY_INPUT_MODE, safe.input_mode);
    if (err != ESP_OK) {
        goto finish;
    }

    err = nvs_set_u8(handle, KEY_ANALOG_CHANNEL, safe.analog_channel);
    if (err != ESP_OK) {
        goto finish;
    }

    err = nvs_set_u32(handle, KEY_DISTANCE_EMPTY_MM, distance_empty_mm);
    if (err != ESP_OK) {
        goto finish;
    }

    err = nvs_set_u32(handle, KEY_DISTANCE_FULL_MM, distance_full_mm);
    if (err != ESP_OK) {
        goto finish;
    }

    err = nvs_set_u32(handle, KEY_CAPACITY_CENTI_M3, capacity_centi_m3);
    if (err != ESP_OK) {
        goto finish;
    }

    err = nvs_set_u8(
        handle,
        KEY_WARNING_PERCENT,
        (uint8_t)safe.warning_percent
    );
    if (err != ESP_OK) {
        goto finish;
    }

    err = nvs_set_u8(
        handle,
        KEY_CRITICAL_PERCENT,
        (uint8_t)safe.critical_percent
    );
    if (err != ESP_OK) {
        goto finish;
    }

    err = nvs_commit(handle);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Tank configuration saved to NVS");
    }

finish:
    nvs_close(handle);
    return err;
}

esp_err_t settings_storage_load_wifi_credentials(wifi_credentials_t *credentials)
{
    if (credentials == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(credentials, 0, sizeof(*credentials));

    nvs_handle_t handle;
    esp_err_t err = nvs_open(SETTINGS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    size_t ssid_size = sizeof(credentials->ssid);
    size_t password_size = sizeof(credentials->password);

    err = nvs_get_str(handle, KEY_WIFI_SSID, credentials->ssid, &ssid_size);
    if (err != ESP_OK) {
        goto finish;
    }

    err = nvs_get_str(
        handle,
        KEY_WIFI_PASSWORD,
        credentials->password,
        &password_size
    );
    if (err != ESP_OK) {
        goto finish;
    }

    credentials->ssid[sizeof(credentials->ssid) - 1U] = '\0';
    credentials->password[sizeof(credentials->password) - 1U] = '\0';

    if (!wifi_credentials_are_valid(credentials)) {
        memset(credentials, 0, sizeof(*credentials));
        err = ESP_ERR_INVALID_STATE;
        goto finish;
    }

    ESP_LOGI(TAG, "Wi-Fi credentials loaded for SSID '%s'", credentials->ssid);

finish:
    nvs_close(handle);
    return err;
}

esp_err_t settings_storage_save_wifi_credentials(const wifi_credentials_t *credentials)
{
    if (!wifi_credentials_are_valid(credentials)) {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_credentials_t safe = *credentials;
    safe.ssid[sizeof(safe.ssid) - 1U] = '\0';
    safe.password[sizeof(safe.password) - 1U] = '\0';

    nvs_handle_t handle;
    esp_err_t err = nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_str(handle, KEY_WIFI_SSID, safe.ssid);
    if (err != ESP_OK) {
        goto finish;
    }

    err = nvs_set_str(handle, KEY_WIFI_PASSWORD, safe.password);
    if (err != ESP_OK) {
        goto finish;
    }

    err = nvs_commit(handle);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Wi-Fi credentials saved for SSID '%s'", safe.ssid);
    }

finish:
    nvs_close(handle);
    return err;
}

esp_err_t settings_storage_clear_wifi_credentials(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    const esp_err_t ssid_err = nvs_erase_key(handle, KEY_WIFI_SSID);
    if (ssid_err != ESP_OK && ssid_err != ESP_ERR_NVS_NOT_FOUND) {
        err = ssid_err;
        goto finish;
    }

    const esp_err_t password_err = nvs_erase_key(handle, KEY_WIFI_PASSWORD);
    if (password_err != ESP_OK && password_err != ESP_ERR_NVS_NOT_FOUND) {
        err = password_err;
        goto finish;
    }

    err = nvs_commit(handle);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Wi-Fi credentials cleared");
    }

finish:
    nvs_close(handle);
    return err;
}
