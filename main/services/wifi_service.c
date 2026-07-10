#include "wifi_service.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define WIFI_SCAN_FETCH_MAX  16U

static const char *TAG = "wifi_service";

static SemaphoreHandle_t s_mutex;
static TaskHandle_t s_scan_task;
static wifi_service_snapshot_t s_snapshot;

static bool state_lock(void)
{
    return s_mutex != NULL &&
           xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE;
}

static void state_unlock(void)
{
    xSemaphoreGive(s_mutex);
}

static void set_last_error(esp_err_t error)
{
    if (!state_lock()) {
        return;
    }

    s_snapshot.last_error = error;
    state_unlock();
}

static bool ssid_already_added(
    const wifi_service_ap_t *aps,
    uint8_t count,
    const char *ssid)
{
    for (uint8_t i = 0U; i < count; i++) {
        if (strncmp(aps[i].ssid, ssid, sizeof(aps[i].ssid)) == 0) {
            return true;
        }
    }

    return false;
}

static void publish_scan_results(
    const wifi_ap_record_t *records,
    uint16_t fetched_count,
    uint16_t total_count,
    esp_err_t result)
{
    wifi_service_ap_t visible[WIFI_SERVICE_MAX_APS] = {0};
    uint8_t visible_count = 0U;

    if (result == ESP_OK) {
        for (uint16_t i = 0U;
             i < fetched_count && visible_count < WIFI_SERVICE_MAX_APS;
             i++) {
            char ssid[WIFI_SERVICE_SSID_MAX_LEN + 1U] = {0};
            memcpy(ssid, records[i].ssid, WIFI_SERVICE_SSID_MAX_LEN);
            ssid[WIFI_SERVICE_SSID_MAX_LEN] = '\0';

            if (ssid[0] == '\0' ||
                ssid_already_added(visible, visible_count, ssid)) {
                continue;
            }

            wifi_service_ap_t *ap = &visible[visible_count++];
            strncpy(ap->ssid, ssid, sizeof(ap->ssid) - 1U);
            ap->ssid[sizeof(ap->ssid) - 1U] = '\0';
            ap->rssi = records[i].rssi;
            ap->channel = records[i].primary;
            ap->secured = records[i].authmode != WIFI_AUTH_OPEN;
        }
    }

    if (!state_lock()) {
        return;
    }

    s_snapshot.scanning = false;
    s_snapshot.last_error = result;
    s_snapshot.total_ap_count = result == ESP_OK ? total_count : 0U;
    s_snapshot.ap_count = result == ESP_OK ? visible_count : 0U;
    memset(s_snapshot.aps, 0, sizeof(s_snapshot.aps));

    if (visible_count > 0U) {
        memcpy(
            s_snapshot.aps,
            visible,
            (size_t)visible_count * sizeof(visible[0])
        );
    }

    s_snapshot.scan_revision++;
    state_unlock();
}

static void perform_scan(void)
{
    if (state_lock()) {
        s_snapshot.scanning = true;
        s_snapshot.last_error = ESP_OK;
        state_unlock();
    }

    ESP_LOGI(TAG, "Starting Wi-Fi scan");

    esp_err_t result = esp_wifi_scan_start(NULL, true);
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi scan failed: %s", esp_err_to_name(result));
        publish_scan_results(NULL, 0U, 0U, result);
        return;
    }

    uint16_t total_count = 0U;
    result = esp_wifi_scan_get_ap_num(&total_count);
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "Unable to read Wi-Fi AP count: %s", esp_err_to_name(result));
        publish_scan_results(NULL, 0U, 0U, result);
        return;
    }

    wifi_ap_record_t records[WIFI_SCAN_FETCH_MAX] = {0};
    uint16_t fetched_count = WIFI_SCAN_FETCH_MAX;
    result = esp_wifi_scan_get_ap_records(&fetched_count, records);

    if (result != ESP_OK) {
        ESP_LOGW(TAG, "Unable to read Wi-Fi scan results: %s", esp_err_to_name(result));
        publish_scan_results(NULL, 0U, total_count, result);
        return;
    }

    ESP_LOGI(
        TAG,
        "Wi-Fi scan complete: total=%u, fetched=%u",
        (unsigned int)total_count,
        (unsigned int)fetched_count
    );

    publish_scan_results(records, fetched_count, total_count, ESP_OK);
}

static void scan_task(void *arg)
{
    (void)arg;

    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        perform_scan();
    }
}

esp_err_t wifi_service_start(void)
{
    if (s_snapshot.started) {
        return ESP_OK;
    }

    memset(&s_snapshot, 0, sizeof(s_snapshot));
    s_snapshot.last_error = ESP_OK;
    strncpy(s_snapshot.ip_address, "0.0.0.0", sizeof(s_snapshot.ip_address) - 1U);

    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t result = esp_netif_init();
    if (result != ESP_OK) {
        set_last_error(result);
        return result;
    }

    result = esp_event_loop_create_default();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        set_last_error(result);
        return result;
    }

    if (esp_netif_create_default_wifi_sta() == NULL) {
        set_last_error(ESP_FAIL);
        return ESP_FAIL;
    }

    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    result = esp_wifi_init(&config);
    if (result != ESP_OK) {
        set_last_error(result);
        return result;
    }

    result = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (result != ESP_OK) {
        set_last_error(result);
        return result;
    }

    result = esp_wifi_set_mode(WIFI_MODE_STA);
    if (result != ESP_OK) {
        set_last_error(result);
        return result;
    }

    result = esp_wifi_start();
    if (result != ESP_OK) {
        set_last_error(result);
        return result;
    }

    uint8_t mac[6] = {0};
    result = esp_wifi_get_mac(WIFI_IF_STA, mac);
    if (result != ESP_OK) {
        set_last_error(result);
        return result;
    }

    if (state_lock()) {
        s_snapshot.started = true;
        s_snapshot.radio_ready = true;
        memcpy(s_snapshot.mac, mac, sizeof(mac));
        s_snapshot.last_error = ESP_OK;
        state_unlock();
    }

    const BaseType_t task_result = xTaskCreate(
        scan_task,
        "wifi_scan",
        4096,
        NULL,
        2,
        &s_scan_task
    );

    if (task_result != pdPASS) {
        set_last_error(ESP_ERR_NO_MEM);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(
        TAG,
        "Wi-Fi radio ready, MAC %02X:%02X:%02X:%02X:%02X:%02X",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]
    );

    return wifi_service_request_scan();
}

esp_err_t wifi_service_request_scan(void)
{
    if (!s_snapshot.started || s_scan_task == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (state_lock()) {
        const bool already_scanning = s_snapshot.scanning;
        state_unlock();

        if (already_scanning) {
            return ESP_ERR_INVALID_STATE;
        }
    }

    xTaskNotifyGive(s_scan_task);
    return ESP_OK;
}

void wifi_service_get_snapshot(wifi_service_snapshot_t *snapshot)
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
