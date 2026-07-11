#include "weather_location_storage.h"

#include <stdint.h>
#include <string.h>

#include "esp_log.h"
#include "nvs.h"

#define WEATHER_NAMESPACE  "smartweather"
#define WEATHER_VERSION    1U

#define KEY_VERSION        "loc_ver"
#define KEY_NAME           "name"
#define KEY_ADMIN1         "admin1"
#define KEY_COUNTRY        "country"
#define KEY_TIMEZONE       "timezone"
#define KEY_LATITUDE_E6    "lat_e6"
#define KEY_LONGITUDE_E6   "lon_e6"

static const char *TAG = "weather_location";

static int32_t coordinate_to_e6(double coordinate)
{
    const double scaled = coordinate * 1000000.0;
    return (int32_t)(scaled >= 0.0 ? scaled + 0.5 : scaled - 0.5);
}

static void terminate_strings(weather_location_t *location)
{
    location->name[sizeof(location->name) - 1U] = '\0';
    location->admin1[sizeof(location->admin1) - 1U] = '\0';
    location->country[sizeof(location->country) - 1U] = '\0';
    location->timezone[sizeof(location->timezone) - 1U] = '\0';
}

esp_err_t weather_location_storage_load(weather_location_t *location)
{
    if (location == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(location, 0, sizeof(*location));

    nvs_handle_t handle;
    esp_err_t err = nvs_open(WEATHER_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t version = 0U;
    int32_t latitude_e6 = 0;
    int32_t longitude_e6 = 0;
    size_t name_size = sizeof(location->name);
    size_t admin1_size = sizeof(location->admin1);
    size_t country_size = sizeof(location->country);
    size_t timezone_size = sizeof(location->timezone);

    err = nvs_get_u8(handle, KEY_VERSION, &version);
    if (err != ESP_OK) {
        goto finish;
    }

    if (version != WEATHER_VERSION) {
        err = ESP_ERR_INVALID_VERSION;
        goto finish;
    }

    err = nvs_get_str(handle, KEY_NAME, location->name, &name_size);
    if (err != ESP_OK) {
        goto finish;
    }

    err = nvs_get_str(handle, KEY_ADMIN1, location->admin1, &admin1_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        goto finish;
    }
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        location->admin1[0] = '\0';
        err = ESP_OK;
    }

    err = nvs_get_str(handle, KEY_COUNTRY, location->country, &country_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        goto finish;
    }
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        location->country[0] = '\0';
        err = ESP_OK;
    }

    err = nvs_get_str(handle, KEY_TIMEZONE, location->timezone, &timezone_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        goto finish;
    }
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        location->timezone[0] = '\0';
        err = ESP_OK;
    }

    err = nvs_get_i32(handle, KEY_LATITUDE_E6, &latitude_e6);
    if (err != ESP_OK) {
        goto finish;
    }

    err = nvs_get_i32(handle, KEY_LONGITUDE_E6, &longitude_e6);
    if (err != ESP_OK) {
        goto finish;
    }

    terminate_strings(location);
    location->latitude = (double)latitude_e6 / 1000000.0;
    location->longitude = (double)longitude_e6 / 1000000.0;

    if (!weather_location_is_valid(location)) {
        memset(location, 0, sizeof(*location));
        err = ESP_ERR_INVALID_STATE;
        goto finish;
    }

    ESP_LOGI(
        TAG,
        "Loaded weather location '%s' (%.6f, %.6f)",
        location->name,
        location->latitude,
        location->longitude
    );

finish:
    nvs_close(handle);
    return err;
}

esp_err_t weather_location_storage_save(const weather_location_t *location)
{
    if (!weather_location_is_valid(location)) {
        return ESP_ERR_INVALID_ARG;
    }

    weather_location_t safe = *location;
    terminate_strings(&safe);

    nvs_handle_t handle;
    esp_err_t err = nvs_open(WEATHER_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_u8(handle, KEY_VERSION, WEATHER_VERSION);
    if (err != ESP_OK) {
        goto finish;
    }

    err = nvs_set_str(handle, KEY_NAME, safe.name);
    if (err != ESP_OK) {
        goto finish;
    }

    err = nvs_set_str(handle, KEY_ADMIN1, safe.admin1);
    if (err != ESP_OK) {
        goto finish;
    }

    err = nvs_set_str(handle, KEY_COUNTRY, safe.country);
    if (err != ESP_OK) {
        goto finish;
    }

    err = nvs_set_str(handle, KEY_TIMEZONE, safe.timezone);
    if (err != ESP_OK) {
        goto finish;
    }

    err = nvs_set_i32(
        handle,
        KEY_LATITUDE_E6,
        coordinate_to_e6(safe.latitude)
    );
    if (err != ESP_OK) {
        goto finish;
    }

    err = nvs_set_i32(
        handle,
        KEY_LONGITUDE_E6,
        coordinate_to_e6(safe.longitude)
    );
    if (err != ESP_OK) {
        goto finish;
    }

    err = nvs_commit(handle);
    if (err == ESP_OK) {
        ESP_LOGI(
            TAG,
            "Saved weather location '%s' (%.6f, %.6f)",
            safe.name,
            safe.latitude,
            safe.longitude
        );
    }

finish:
    nvs_close(handle);
    return err;
}

esp_err_t weather_location_storage_clear(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(WEATHER_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_erase_all(handle);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Cleared weather location");
    }

    nvs_close(handle);
    return err;
}
