#include "weather_geocoding.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "wifi_service.h"

#define GEOCODING_ENDPOINT       "https://geocoding-api.open-meteo.com/v1/search"
#define GEOCODING_TASK_STACK     8192U
#define GEOCODING_TASK_PRIORITY  2U
#define GEOCODING_RESPONSE_SIZE  12288U
#define GEOCODING_HTTP_TIMEOUT   15000
#define GEOCODING_RETRY_DELAY_MS 700U

static const char *TAG = "weather_geocoding";

typedef struct {
    char query[WEATHER_GEOCODING_QUERY_MAX_LEN + 1U];
} geocoding_request_t;

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
    bool overflow;
    esp_err_t tls_error;
    int mbedtls_error;
} http_response_buffer_t;

static SemaphoreHandle_t s_mutex;
static QueueHandle_t s_request_queue;
static TaskHandle_t s_task;
static weather_geocoding_snapshot_t s_snapshot;

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

static bool normalize_query(
    const char *query,
    char normalized[WEATHER_GEOCODING_QUERY_MAX_LEN + 1U])
{
    if (query == NULL || normalized == NULL) {
        return false;
    }

    while (*query != '\0' && isspace((unsigned char)*query)) {
        query++;
    }

    const size_t raw_length = strnlen(
        query,
        WEATHER_GEOCODING_QUERY_MAX_LEN + 2U
    );
    if (raw_length > WEATHER_GEOCODING_QUERY_MAX_LEN) {
        return false;
    }

    size_t length = raw_length;
    while (length > 0U && isspace((unsigned char)query[length - 1U])) {
        length--;
    }

    if (length < 2U) {
        return false;
    }

    memcpy(normalized, query, length);
    normalized[length] = '\0';
    return true;
}

static bool is_url_unreserved(unsigned char value)
{
    return isalnum(value) ||
           value == '-' || value == '_' || value == '.' || value == '~';
}

static esp_err_t url_encode(
    const char *input,
    char *output,
    size_t output_size)
{
    static const char hex[] = "0123456789ABCDEF";

    if (input == NULL || output == NULL || output_size == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t output_index = 0U;
    for (size_t input_index = 0U; input[input_index] != '\0'; input_index++) {
        const unsigned char value = (unsigned char)input[input_index];

        if (is_url_unreserved(value)) {
            if (output_index + 1U >= output_size) {
                return ESP_ERR_INVALID_SIZE;
            }
            output[output_index++] = (char)value;
        } else {
            if (output_index + 3U >= output_size) {
                return ESP_ERR_INVALID_SIZE;
            }
            output[output_index++] = '%';
            output[output_index++] = hex[value >> 4U];
            output[output_index++] = hex[value & 0x0FU];
        }
    }

    output[output_index] = '\0';
    return ESP_OK;
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

static void copy_json_string(
    const cJSON *object,
    const char *key,
    char *destination,
    size_t destination_size)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    copy_text(
        destination,
        destination_size,
        cJSON_IsString(item) ? item->valuestring : ""
    );
}

static esp_err_t parse_locations(
    const char *json,
    weather_location_t results[WEATHER_GEOCODING_MAX_RESULTS],
    uint8_t *result_count)
{
    if (json == NULL || results == NULL || result_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *result_count = 0U;
    memset(
        results,
        0,
        sizeof(weather_location_t) * WEATHER_GEOCODING_MAX_RESULTS
    );

    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    const cJSON *json_results = cJSON_GetObjectItemCaseSensitive(root, "results");
    if (json_results == NULL) {
        cJSON_Delete(root);
        return ESP_OK;
    }

    if (!cJSON_IsArray(json_results)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }

    const int count = cJSON_GetArraySize(json_results);
    for (int index = 0;
         index < count && *result_count < WEATHER_GEOCODING_MAX_RESULTS;
         index++) {
        const cJSON *item = cJSON_GetArrayItem(json_results, index);
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(item, "name");
        const cJSON *latitude = cJSON_GetObjectItemCaseSensitive(item, "latitude");
        const cJSON *longitude = cJSON_GetObjectItemCaseSensitive(item, "longitude");

        if (!cJSON_IsObject(item) ||
            !cJSON_IsString(name) ||
            !cJSON_IsNumber(latitude) ||
            !cJSON_IsNumber(longitude)) {
            continue;
        }

        weather_location_t location = {
            .latitude = latitude->valuedouble,
            .longitude = longitude->valuedouble,
        };

        copy_text(location.name, sizeof(location.name), name->valuestring);
        copy_json_string(
            item,
            "admin1",
            location.admin1,
            sizeof(location.admin1)
        );
        copy_json_string(
            item,
            "country",
            location.country,
            sizeof(location.country)
        );
        copy_json_string(
            item,
            "timezone",
            location.timezone,
            sizeof(location.timezone)
        );

        if (!weather_location_is_valid(&location)) {
            continue;
        }

        results[*result_count] = location;
        (*result_count)++;
    }

    cJSON_Delete(root);
    return ESP_OK;
}

static void publish_error(esp_err_t error, int http_status, bool waiting_for_wifi)
{
    if (!state_lock()) {
        return;
    }

    s_snapshot.searching = false;
    s_snapshot.waiting_for_wifi = waiting_for_wifi;
    s_snapshot.result_count = 0U;
    s_snapshot.http_status = http_status;
    s_snapshot.last_error = error;
    s_snapshot.error_count++;
    s_snapshot.revision++;
    memset(s_snapshot.results, 0, sizeof(s_snapshot.results));

    state_unlock();
}

static void publish_results(
    const weather_location_t results[WEATHER_GEOCODING_MAX_RESULTS],
    uint8_t result_count,
    int http_status)
{
    if (!state_lock()) {
        return;
    }

    s_snapshot.searching = false;
    s_snapshot.waiting_for_wifi = false;
    s_snapshot.result_count = result_count;
    s_snapshot.http_status = http_status;
    s_snapshot.last_error = ESP_OK;
    s_snapshot.success_count++;
    s_snapshot.revision++;
    memcpy(s_snapshot.results, results, sizeof(s_snapshot.results));

    state_unlock();
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
        .timeout_ms = GEOCODING_HTTP_TIMEOUT,
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

static esp_err_t perform_search(
    const char *query,
    weather_location_t results[WEATHER_GEOCODING_MAX_RESULTS],
    uint8_t *result_count,
    int *http_status)
{
    char encoded_query[(WEATHER_GEOCODING_QUERY_MAX_LEN * 3U) + 1U];
    esp_err_t err = url_encode(
        query,
        encoded_query,
        sizeof(encoded_query)
    );
    if (err != ESP_OK) {
        return err;
    }

    char url[384];
    const int written = snprintf(
        url,
        sizeof(url),
        GEOCODING_ENDPOINT "?name=%s&count=%u&language=pl&format=json",
        encoded_query,
        (unsigned int)WEATHER_GEOCODING_MAX_RESULTS
    );
    if (written < 0 || (size_t)written >= sizeof(url)) {
        return ESP_ERR_INVALID_SIZE;
    }

    char *response_data = heap_caps_malloc(
        GEOCODING_RESPONSE_SIZE,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );
    if (response_data == NULL) {
        response_data = malloc(GEOCODING_RESPONSE_SIZE);
    }
    if (response_data == NULL) {
        return ESP_ERR_NO_MEM;
    }

    http_response_buffer_t response = {
        .data = response_data,
        .capacity = GEOCODING_RESPONSE_SIZE,
    };

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

        vTaskDelay(pdMS_TO_TICKS(GEOCODING_RETRY_DELAY_MS));
    }

    if (err == ESP_OK && response.overflow) {
        err = ESP_ERR_NO_MEM;
    }

    if (err == ESP_OK && *http_status != 200) {
        err = ESP_ERR_INVALID_RESPONSE;
    }

    if (err == ESP_OK) {
        err = parse_locations(response.data, results, result_count);
    }

    free(response_data);
    return err;
}

static void geocoding_task(void *argument)
{
    (void)argument;

    geocoding_request_t request;
    while (true) {
        if (xQueueReceive(s_request_queue, &request, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        wifi_service_snapshot_t wifi;
        wifi_service_get_snapshot(&wifi);
        if (!wifi.connected) {
            publish_error(ESP_ERR_INVALID_STATE, 0, true);
            ESP_LOGW(TAG, "Location search rejected: Wi-Fi is offline");
            continue;
        }

        weather_location_t results[WEATHER_GEOCODING_MAX_RESULTS] = {0};
        uint8_t result_count = 0U;
        int http_status = 0;

        const esp_err_t err = perform_search(
            request.query,
            results,
            &result_count,
            &http_status
        );

        if (err == ESP_OK) {
            publish_results(results, result_count, http_status);
            ESP_LOGI(
                TAG,
                "Location search '%s' returned %u result(s)",
                request.query,
                (unsigned int)result_count
            );
        } else {
            publish_error(err, http_status, false);
            ESP_LOGW(
                TAG,
                "Location search '%s' failed: %s, HTTP %d",
                request.query,
                esp_err_to_name(err),
                http_status
            );
        }
    }
}

esp_err_t weather_geocoding_start(void)
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

    if (s_request_queue == NULL) {
        s_request_queue = xQueueCreate(1U, sizeof(geocoding_request_t));
        if (s_request_queue == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    memset(&s_snapshot, 0, sizeof(s_snapshot));
    s_snapshot.started = true;
    s_snapshot.last_error = ESP_OK;
    s_snapshot.revision = 1U;

    const BaseType_t task_result = xTaskCreate(
        geocoding_task,
        "weather_geo",
        GEOCODING_TASK_STACK,
        NULL,
        GEOCODING_TASK_PRIORITY,
        &s_task
    );
    if (task_result != pdPASS) {
        s_task = NULL;
        s_snapshot.started = false;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Weather geocoding service ready");
    return ESP_OK;
}

esp_err_t weather_geocoding_request(const char *query)
{
    if (s_task == NULL || s_request_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    geocoding_request_t request = {0};
    if (!normalize_query(query, request.query)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (state_lock()) {
        s_snapshot.searching = true;
        s_snapshot.waiting_for_wifi = false;
        s_snapshot.result_count = 0U;
        s_snapshot.http_status = 0;
        s_snapshot.last_error = ESP_OK;
        s_snapshot.revision++;
        copy_text(s_snapshot.query, sizeof(s_snapshot.query), request.query);
        memset(s_snapshot.results, 0, sizeof(s_snapshot.results));
        state_unlock();
    }

    return xQueueOverwrite(s_request_queue, &request) == pdPASS
        ? ESP_OK
        : ESP_FAIL;
}

void weather_geocoding_get_snapshot(weather_geocoding_snapshot_t *snapshot)
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
