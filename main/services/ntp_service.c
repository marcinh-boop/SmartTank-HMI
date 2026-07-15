/*
 * Moduł ntp_service.c należy do warstwy głównej programu SmartTank.
 * Realizuje logikę modułu i ukrywa jej szczegóły za publicznym interfejsem.
 * Oddzielenie tej odpowiedzialności ułatwia diagnostykę, testy i dalszą rozbudowę.
 */
#include "ntp_service.h"

#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "clock_service.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "wifi_service.h"

#define NTP_SERVER_NAME        "pool.ntp.org"
#define NTP_TASK_STACK_SIZE    4096U
#define NTP_TASK_PRIORITY      2U
#define NTP_POLL_INTERVAL_MS   1000U
#define NTP_MIN_VALID_YEAR     2024

static const char *TAG = "ntp_service";

static SemaphoreHandle_t s_mutex;
static TaskHandle_t s_task;
static ntp_service_snapshot_t s_snapshot;
static bool s_sntp_initialized;

static bool state_lock(void)
{
    return s_mutex != NULL &&
           xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE;
}

static void state_unlock(void)
{
    xSemaphoreGive(s_mutex);
}

static void ntp_sync_notification_cb(struct timeval *time_value)
{
    (void)time_value;

    if (s_task != NULL) {
        xTaskNotifyGive(s_task);
    }
}

static void set_waiting_for_wifi(bool waiting)
{
    if (!state_lock()) {
        return;
    }

    s_snapshot.waiting_for_wifi = waiting;
    if (waiting) {
        s_snapshot.synchronizing = false;
    }

    state_unlock();
}

static esp_err_t initialize_sntp(void)
{
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(NTP_SERVER_NAME);
    config.sync_cb = ntp_sync_notification_cb;

    const esp_err_t result = esp_netif_sntp_init(&config);

    if (state_lock()) {
        s_snapshot.initialized = result == ESP_OK;
        s_snapshot.synchronizing = result == ESP_OK;
        s_snapshot.waiting_for_wifi = false;
        s_snapshot.last_error = result;
        state_unlock();
    }

    if (result == ESP_OK) {
        s_sntp_initialized = true;
        ESP_LOGI(TAG, "SNTP started with server %s", NTP_SERVER_NAME);
    } else {
        ESP_LOGW(TAG, "Unable to start SNTP: %s", esp_err_to_name(result));
    }

    return result;
}

static esp_err_t process_synchronized_time(void)
{
    time_t now = 0;
    time(&now);

    struct tm local_time = {0};
    if (localtime_r(&now, &local_time) == NULL ||
        local_time.tm_year + 1900 < NTP_MIN_VALID_YEAR) {
        return ESP_ERR_INVALID_STATE;
    }

    const esp_err_t rtc_result = clock_service_accept_ntp_time(now);

    if (state_lock()) {
        s_snapshot.synchronized = true;
        s_snapshot.synchronizing = false;
        s_snapshot.waiting_for_wifi = false;
        s_snapshot.rtc_updated = rtc_result == ESP_OK;
        s_snapshot.sync_count++;
        s_snapshot.last_sync_epoch = now;
        s_snapshot.last_error = ESP_OK;
        s_snapshot.rtc_error = rtc_result;
        state_unlock();
    }

    ESP_LOGI(
        TAG,
        "Time synchronized: %04d-%02d-%02d %02d:%02d:%02d, RTC=%s",
        local_time.tm_year + 1900,
        local_time.tm_mon + 1,
        local_time.tm_mday,
        local_time.tm_hour,
        local_time.tm_min,
        local_time.tm_sec,
        esp_err_to_name(rtc_result)
    );

    return ESP_OK;
}

static void publish_sync_error(esp_err_t error)
{
    if (!state_lock()) {
        return;
    }

    s_snapshot.synchronizing = false;
    s_snapshot.last_error = error;
    state_unlock();
}

static void ntp_task(void *arg)
{
    (void)arg;

    while (true) {
        const uint32_t notifications = ulTaskNotifyTake(
            pdTRUE,
            pdMS_TO_TICKS(NTP_POLL_INTERVAL_MS)
        );

        wifi_service_snapshot_t wifi;
        wifi_service_get_snapshot(&wifi);

        if (!wifi.connected) {
            set_waiting_for_wifi(true);
            continue;
        }

        set_waiting_for_wifi(false);

        if (!s_sntp_initialized) {
            if (initialize_sntp() != ESP_OK) {
                continue;
            }
        }

        const bool first_sync_completed =
            !s_snapshot.synchronized &&
            sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED;

        if (notifications > 0U || first_sync_completed) {
            const esp_err_t result = process_synchronized_time();
            if (result != ESP_OK) {
                publish_sync_error(result);
                ESP_LOGW(TAG, "Synchronized time is invalid: %s", esp_err_to_name(result));
            }
        } else if (state_lock()) {
            if (!s_snapshot.synchronized) {
                s_snapshot.synchronizing = true;
            }
            state_unlock();
        }
    }
}

esp_err_t ntp_service_start(void)
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
    s_snapshot.waiting_for_wifi = true;
    s_snapshot.last_error = ESP_OK;
    s_snapshot.rtc_error = ESP_OK;
    strncpy(s_snapshot.server, NTP_SERVER_NAME, sizeof(s_snapshot.server) - 1U);
    s_snapshot.server[sizeof(s_snapshot.server) - 1U] = '\0';

    const BaseType_t result = xTaskCreate(
        ntp_task,
        "ntp_service",
        NTP_TASK_STACK_SIZE,
        NULL,
        NTP_TASK_PRIORITY,
        &s_task
    );

    if (result != pdPASS) {
        s_task = NULL;
        s_snapshot.started = false;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "NTP service ready; waiting for Wi-Fi");
    return ESP_OK;
}

void ntp_service_get_snapshot(ntp_service_snapshot_t *snapshot)
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
