/*
 * Moduł weather_service.c należy do warstwy głównej programu SmartTank.
 * Realizuje logikę modułu i ukrywa jej szczegóły za publicznym interfejsem.
 * Oddzielenie tej odpowiedzialności ułatwia diagnostykę, testy i dalszą rozbudowę.
 */
#include "weather_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "app_model.h"
#include "weather_location_storage.h"
#include "wifi_service.h"

#define WEATHER_ENDPOINT          "https://api.open-meteo.com/v1/forecast"
#define WEATHER_TASK_STACK        6144U
#define WEATHER_TASK_PRIORITY     2U
#define WEATHER_RESPONSE_SIZE     16384U
#define WEATHER_HTTP_TIMEOUT      15000
#define WEATHER_RETRY_DELAY_MS    700U
#define WEATHER_POLL_INTERVAL_MS  5000U
#define WEATHER_RETRY_INTERVAL_MS 60000U
#define WEATHER_UPDATE_INTERVAL_MS 900000U

static const char *TAG = "weather_service";

static SemaphoreHandle_t s_mutex;
static TaskHandle_t s_task;
static weather_service_snapshot_t s_snapshot;
static uint32_t s_sample_counter;

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
    bool overflow;
    esp_err_t tls_error;
    int mbedtls_error;
} http_response_buffer_t;

static bool state_lock(void)
{
    return s_mutex != NULL &&
           xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE;
}

static void state_unlock(void)
{
    xSemaphoreGive(s_mutex);
}

static void copy_text(char *destination, size_t destination_size, const char *source)
{
    if (destination == NULL || destination_size == 0U) {
        return;
    }

    if (source == NULL) {
        destination[0] = '\0';
        return;
    }

    strncpy(destination, source, destination_size - 1U);
    destination[destination_size - 1U] = '\0';
}

static void format_location(
    const weather_location_t *location,
    char *buffer,
    size_t buffer_size)
{
    if (location == NULL || buffer == NULL || buffer_size == 0U) {
        return;
    }

    if (location->admin1[0] != '\0') {
        snprintf(buffer, buffer_size, "%s, %s", location->name, location->admin1);
    } else {
        snprintf(buffer, buffer_size, "%s", location->name);
    }
}

static const char *description_for_code(int code)
{
    switch (code) {
        case 0:
            return "Bezchmurnie";
        case 1:
        case 2:
            return "Pogodnie";
        case 3:
            return "Pochmurno";
        case 45:
        case 48:
            return "Mgla";
        case 51:
        case 53:
        case 55:
        case 56:
        case 57:
            return "Mzawka";
        case 61:
        case 63:
        case 65:
        case 66:
        case 67:
            return "Deszcz";
        case 71:
        case 73:
        case 75:
        case 77:
            return "Snieg";
        case 80:
        case 81:
        case 82:
            return "Przelotny deszcz";
        case 85:
        case 86:
            return "Przelotny snieg";
        case 95:
            return "Burza";
        case 96:
        case 99:
            return "Burza z gradem";
        default:
            return "Pogoda";
    }
}

static int weekday_from_iso_date(const char *date)
{
    int year = 0;
    int month = 0;
    int day = 0;

    if (date == NULL || sscanf(date, "%d-%d-%d", &year, &month, &day) != 3) {
        return -1;
    }

    if (month < 1 || month > 12 || day < 1 || day > 31) {
        return -1;
    }

    if (month < 3) {
        month += 12;
        year--;
    }

    const int year_of_century = year % 100;
    const int century = year / 100;
    const int zeller = (
        day +
        (13 * (month + 1)) / 5 +
        year_of_century +
        year_of_century / 4 +
        century / 4 +
        5 * century
    ) % 7;

    return (zeller + 6) % 7;
}

static void set_forecast_day_label(weather_forecast_day_t *forecast, const char *date)
{
    static const char *DAYS[] = {
        "NIE", "PON", "WTO", "SRO", "CZW", "PIA", "SOB"
    };

    if (forecast == NULL) {
        return;
    }

    const int weekday = weekday_from_iso_date(date);
    copy_text(
        forecast->day,
        sizeof(forecast->day),
        weekday >= 0 && weekday < 7 ? DAYS[weekday] : "---"
    );
}

static bool json_number(
    const cJSON *object,
    const char *key,
    double *value)
{
    if (object == NULL || key == NULL || value == NULL) {
        return false;
    }

    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!cJSON_IsNumber(item)) {
        return false;
    }

    *value = item->valuedouble;
    return true;
}

static bool json_array_number(
    const cJSON *array,
    int index,
    double *value)
{
    if (!cJSON_IsArray(array) || value == NULL) {
        return false;
    }

    const cJSON *item = cJSON_GetArrayItem(array, index);
    if (!cJSON_IsNumber(item)) {
        return false;
    }

    *value = item->valuedouble;
    return true;
}

static esp_err_t parse_weather_response(
    const char *json,
    weather_measurement_t *measurement)
{
    if (json == NULL || measurement == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    const cJSON *current = cJSON_GetObjectItemCaseSensitive(root, "current");
    const cJSON *daily = cJSON_GetObjectItemCaseSensitive(root, "daily");

    double temperature = 0.0;
    double humidity = 0.0;
    double rain_probability = 0.0;
    double weather_code = 0.0;
    double wind_speed = 0.0;

    const bool current_valid =
        cJSON_IsObject(current) &&
        json_number(current, "temperature_2m", &temperature) &&
        json_number(current, "relative_humidity_2m", &humidity) &&
        json_number(current, "weather_code", &weather_code) &&
        json_number(current, "wind_speed_10m", &wind_speed);

    if (!current_valid || !cJSON_IsObject(daily)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }

    const bool has_current_rain = json_number(
        current,
        "precipitation_probability",
        &rain_probability
    );

    const cJSON *dates = cJSON_GetObjectItemCaseSensitive(daily, "time");
    const cJSON *daily_codes = cJSON_GetObjectItemCaseSensitive(daily, "weather_code");
    const cJSON *maximums = cJSON_GetObjectItemCaseSensitive(daily, "temperature_2m_max");
    const cJSON *minimums = cJSON_GetObjectItemCaseSensitive(daily, "temperature_2m_min");
    const cJSON *rain_maximums = cJSON_GetObjectItemCaseSensitive(
        daily,
        "precipitation_probability_max"
    );

    if (!cJSON_IsArray(dates) ||
        !cJSON_IsArray(daily_codes) ||
        !cJSON_IsArray(maximums) ||
        !cJSON_IsArray(minimums) ||
        !cJSON_IsArray(rain_maximums)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }

    memset(measurement, 0, sizeof(*measurement));
    measurement->temperature_c = (float)temperature;
    measurement->humidity_percent = (int)(humidity + 0.5);
    measurement->wind_kmh = (float)wind_speed;
    measurement->weather_code = (int)(weather_code + 0.5);
    measurement->valid = true;
    measurement->stale = false;
    measurement->updated_epoch = (int64_t)time(NULL);
    measurement->sample_counter = ++s_sample_counter;
    copy_text(
        measurement->description,
        sizeof(measurement->description),
        description_for_code(measurement->weather_code)
    );

    int forecast_count = cJSON_GetArraySize(dates);
    if (forecast_count > (int)WEATHER_FORECAST_DAYS) {
        forecast_count = (int)WEATHER_FORECAST_DAYS;
    }

    for (int index = 0; index < forecast_count; index++) {
        const cJSON *date = cJSON_GetArrayItem(dates, index);
        double code = 0.0;
        double maximum = 0.0;
        double minimum = 0.0;
        double rain = 0.0;

        if (!cJSON_IsString(date) ||
            !json_array_number(daily_codes, index, &code) ||
            !json_array_number(maximums, index, &maximum) ||
            !json_array_number(minimums, index, &minimum)) {
            continue;
        }

        (void)json_array_number(rain_maximums, index, &rain);

        weather_forecast_day_t *forecast =
            &measurement->forecast[measurement->forecast_count];
        set_forecast_day_label(forecast, date->valuestring);
        forecast->weather_code = (int)(code + 0.5);
        forecast->temperature_max_c = (float)maximum;
        forecast->temperature_min_c = (float)minimum;
        forecast->rain_percent = (int)(rain + 0.5);
        forecast->valid = true;
        measurement->forecast_count++;
    }

    if (has_current_rain) {
        measurement->rain_percent = (int)(rain_probability + 0.5);
    } else if (measurement->forecast_count > 0U) {
        measurement->rain_percent = measurement->forecast[0].rain_percent;
    }

    cJSON_Delete(root);
    return measurement->forecast_count > 0U
        ? ESP_OK
        : ESP_ERR_INVALID_RESPONSE;
}

static esp_err_t http_event_handler(esp_http_client_event_t *event)
{
    if (event == NULL) {
        return ESP_OK;
    }

    http_response_buffer_t *response = event->user_data;

    if (event->event_id == HTTP_EVENT_ON_DATA) {
        if (response == NULL || response->data == NULL || response->overflow) {
            return ESP_FAIL;
        }

        const size_t incoming = event->data_len > 0
            ? (size_t)event->data_len
            : 0U;
        const size_t available = response->capacity - response->length - 1U;

        if (incoming > available) {
            response->overflow = true;
            return ESP_ERR_NO_MEM;
        }

        if (incoming > 0U) {
            memcpy(response->data + response->length, event->data, incoming);
            response->length += incoming;
            response->data[response->length] = '\0';
        }
    } else if (event->event_id == HTTP_EVENT_DISCONNECTED &&
               response != NULL && event->data != NULL) {
        response->tls_error = esp_tls_get_and_clear_last_error(
            (esp_tls_error_handle_t)event->data,
            &response->mbedtls_error,
            NULL
        );
    }

    return ESP_OK;
}

static esp_err_t perform_http_request(
    const char *url,
    http_response_buffer_t *response,
    int *http_status)
{
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .user_data = response,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = WEATHER_HTTP_TIMEOUT,
        .buffer_size = 1024,
        .buffer_size_tx = 512,
        .keep_alive_enable = false,
        .addr_type = HTTP_ADDR_TYPE_INET,
        .user_agent = "SmartTank-HMI/1.0",
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        return ESP_ERR_NO_MEM;
    }

    const esp_err_t err = esp_http_client_perform(client);
    *http_status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    return err;
}

static esp_err_t fetch_weather(
    const weather_location_t *location,
    weather_measurement_t *measurement,
    int *http_status)
{
    if (!weather_location_is_valid(location) ||
        measurement == NULL ||
        http_status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    char url[768];
    const int written = snprintf(
        url,
        sizeof(url),
        WEATHER_ENDPOINT
        "?latitude=%.6f&longitude=%.6f"
        "&current=temperature_2m,relative_humidity_2m,precipitation_probability,weather_code,wind_speed_10m"
        "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max"
        "&timezone=auto&forecast_days=%u",
        location->latitude,
        location->longitude,
        (unsigned int)WEATHER_FORECAST_DAYS
    );

    if (written < 0 || (size_t)written >= sizeof(url)) {
        return ESP_ERR_INVALID_SIZE;
    }

    char *response_data = heap_caps_malloc(
        WEATHER_RESPONSE_SIZE,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );
    if (response_data == NULL) {
        response_data = malloc(WEATHER_RESPONSE_SIZE);
    }
    if (response_data == NULL) {
        return ESP_ERR_NO_MEM;
    }

    http_response_buffer_t response = {
        .data = response_data,
        .capacity = WEATHER_RESPONSE_SIZE,
    };

    esp_err_t err = ESP_FAIL;
    for (unsigned int attempt = 1U; attempt <= 2U; attempt++) {
        response.length = 0U;
        response.overflow = false;
        response.tls_error = ESP_OK;
        response.mbedtls_error = 0;
        response.data[0] = '\0';
        *http_status = 0;

        err = perform_http_request(url, &response, http_status);
        if (err == ESP_OK) {
            break;
        }

        ESP_LOGW(
            TAG,
            "HTTPS attempt %u failed: %s (0x%X), TLS 0x%X, mbedTLS -0x%X, internal heap %u",
            attempt,
            esp_err_to_name(err),
            (unsigned int)err,
            (unsigned int)response.tls_error,
            (unsigned int)(response.mbedtls_error < 0
                ? -response.mbedtls_error
                : response.mbedtls_error),
            (unsigned int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
        );

        if (err != ESP_ERR_HTTP_CONNECT || attempt >= 2U) {
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(WEATHER_RETRY_DELAY_MS));
    }

    if (err == ESP_OK && response.overflow) {
        err = ESP_ERR_NO_MEM;
    }

    if (err == ESP_OK && *http_status != 200) {
        err = ESP_ERR_INVALID_RESPONSE;
    }

    if (err == ESP_OK) {
        err = parse_weather_response(response.data, measurement);
    }

    free(response_data);
    return err;
}

static void mark_weather_stale(void)
{
    smarttank_state_t state;
    app_model_get_snapshot(&state);

    if (state.weather.valid && !state.weather.stale) {
        state.weather.stale = true;
        app_model_update_weather(&state.weather);
    }
}

static void publish_waiting(bool wifi, bool location)
{
    if (!state_lock()) {
        return;
    }

    s_snapshot.fetching = false;
    s_snapshot.waiting_for_wifi = wifi;
    s_snapshot.waiting_for_location = location;
    s_snapshot.http_status = 0;
    s_snapshot.last_error = ESP_ERR_INVALID_STATE;
    s_snapshot.revision++;

    state_unlock();
}

static void publish_fetching(const weather_location_t *location)
{
    if (!state_lock()) {
        return;
    }

    s_snapshot.fetching = true;
    s_snapshot.waiting_for_wifi = false;
    s_snapshot.waiting_for_location = false;
    s_snapshot.http_status = 0;
    s_snapshot.last_error = ESP_OK;
    format_location(location, s_snapshot.location, sizeof(s_snapshot.location));
    s_snapshot.revision++;

    state_unlock();
}

static void publish_success(
    const weather_location_t *location,
    int http_status,
    int64_t updated_epoch)
{
    if (!state_lock()) {
        return;
    }

    s_snapshot.fetching = false;
    s_snapshot.waiting_for_wifi = false;
    s_snapshot.waiting_for_location = false;
    s_snapshot.http_status = http_status;
    s_snapshot.last_error = ESP_OK;
    s_snapshot.last_update_epoch = updated_epoch;
    s_snapshot.success_count++;
    format_location(location, s_snapshot.location, sizeof(s_snapshot.location));
    s_snapshot.revision++;

    state_unlock();
}

static void publish_error(
    const weather_location_t *location,
    esp_err_t error,
    int http_status)
{
    if (!state_lock()) {
        return;
    }

    s_snapshot.fetching = false;
    s_snapshot.waiting_for_wifi = false;
    s_snapshot.waiting_for_location = false;
    s_snapshot.http_status = http_status;
    s_snapshot.last_error = error;
    s_snapshot.error_count++;
    format_location(location, s_snapshot.location, sizeof(s_snapshot.location));
    s_snapshot.revision++;

    state_unlock();
}

static bool same_location(
    const weather_location_t *left,
    const weather_location_t *right)
{
    return left != NULL &&
           right != NULL &&
           left->latitude == right->latitude &&
           left->longitude == right->longitude &&
           strncmp(left->name, right->name, sizeof(left->name)) == 0;
}

static void weather_task(void *argument)
{
    (void)argument;

    bool force_refresh = true;
    bool have_previous_attempt = false;
    bool previous_attempt_succeeded = false;
    TickType_t previous_attempt_tick = 0;
    weather_location_t previous_location = {0};

    while (true) {
        wifi_service_snapshot_t wifi;
        wifi_service_get_snapshot(&wifi);

        if (!wifi.connected) {
            publish_waiting(true, false);
            mark_weather_stale();
            force_refresh = true;
            (void)ulTaskNotifyTake(
                pdTRUE,
                pdMS_TO_TICKS(WEATHER_POLL_INTERVAL_MS)
            );
            continue;
        }

        weather_location_t location;
        const esp_err_t location_result = weather_location_storage_load(&location);
        if (location_result == ESP_ERR_NVS_NOT_FOUND) {
            publish_waiting(false, true);
            force_refresh = true;
            (void)ulTaskNotifyTake(
                pdTRUE,
                pdMS_TO_TICKS(WEATHER_POLL_INTERVAL_MS)
            );
            continue;
        }

        if (location_result != ESP_OK) {
            publish_error(NULL, location_result, 0);
            mark_weather_stale();
            (void)ulTaskNotifyTake(
                pdTRUE,
                pdMS_TO_TICKS(WEATHER_RETRY_INTERVAL_MS)
            );
            continue;
        }

        const TickType_t now_tick = xTaskGetTickCount();
        const bool location_unchanged =
            have_previous_attempt && same_location(&location, &previous_location);
        const TickType_t required_interval = pdMS_TO_TICKS(
            previous_attempt_succeeded
                ? WEATHER_UPDATE_INTERVAL_MS
                : WEATHER_RETRY_INTERVAL_MS
        );

        if (!force_refresh &&
            location_unchanged &&
            (TickType_t)(now_tick - previous_attempt_tick) < required_interval) {
            const uint32_t notifications = ulTaskNotifyTake(
                pdTRUE,
                pdMS_TO_TICKS(WEATHER_POLL_INTERVAL_MS)
            );
            if (notifications > 0U) {
                force_refresh = true;
            }
            continue;
        }

        force_refresh = false;
        have_previous_attempt = true;
        previous_attempt_tick = now_tick;
        previous_location = location;
        publish_fetching(&location);

        weather_measurement_t measurement;
        int http_status = 0;
        const esp_err_t result = fetch_weather(
            &location,
            &measurement,
            &http_status
        );

        if (result == ESP_OK) {
            app_model_update_weather(&measurement);
            publish_success(&location, http_status, measurement.updated_epoch);
            previous_attempt_succeeded = true;
            ESP_LOGI(
                TAG,
                "Weather updated for %s: %.1f C, humidity %d%%, wind %.1f km/h, code %d",
                location.name,
                measurement.temperature_c,
                measurement.humidity_percent,
                measurement.wind_kmh,
                measurement.weather_code
            );
        } else {
            publish_error(&location, result, http_status);
            mark_weather_stale();
            previous_attempt_succeeded = false;
            ESP_LOGW(
                TAG,
                "Weather update for '%s' failed: %s, HTTP %d",
                location.name,
                esp_err_to_name(result),
                http_status
            );
        }

        const uint32_t notifications = ulTaskNotifyTake(
            pdTRUE,
            pdMS_TO_TICKS(WEATHER_POLL_INTERVAL_MS)
        );
        if (notifications > 0U) {
            force_refresh = true;
        }
    }
}

esp_err_t weather_service_start(void)
{
    if (s_task != NULL) {
        return ESP_OK;
    }

    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutex();
        if (s_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    memset(&s_snapshot, 0, sizeof(s_snapshot));
    s_snapshot.started = true;
    s_snapshot.last_error = ESP_OK;
    s_snapshot.revision = 1U;

    const BaseType_t task_result = xTaskCreate(
        weather_task,
        "weather_api",
        WEATHER_TASK_STACK,
        NULL,
        WEATHER_TASK_PRIORITY,
        &s_task
    );

    if (task_result != pdPASS) {
        s_task = NULL;
        s_snapshot.started = false;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Weather service ready");
    return ESP_OK;
}

esp_err_t weather_service_request_refresh(void)
{
    if (s_task == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xTaskNotifyGive(s_task);
    return ESP_OK;
}

void weather_service_get_snapshot(weather_service_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return;
    }

    if (!state_lock()) {
        memset(snapshot, 0, sizeof(*snapshot));
        snapshot->last_error = ESP_ERR_INVALID_STATE;
        return;
    }

    *snapshot = s_snapshot;
    state_unlock();
}
