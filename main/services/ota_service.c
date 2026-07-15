/*
 * Aktualizacja firmware przez przeglądarkę internetową.
 * GET /update zwraca małą stronę wybierającą plik SmartTank.bin. JavaScript
 * wysyła surową zawartość pliku do POST /api/ota, dzięki czemu ESP32 nie musi
 * analizować kodowania multipart/form-data. Po pełnej weryfikacji obrazu
 * ustawiany jest następny slot startowy, a urządzenie wykonuje restart.
 */
#include "ota_service.h"

#include <stdbool.h>
#include <string.h>

#include "esp_app_format.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "wifi_service.h"

#define OTA_RECEIVE_BUFFER_SIZE 4096U
#define OTA_WIFI_WAIT_MS        1000U

static const char *TAG = "ota_service";
static httpd_handle_t s_server;
static TaskHandle_t s_task;
static bool s_upload_active;
static uint8_t s_receive_buffer[OTA_RECEIVE_BUFFER_SIZE];

static const char OTA_PAGE[] =
    "<!doctype html><html lang='pl'><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>SmartTank OTA</title><style>body{font-family:sans-serif;background:#07131f;"
    "color:#e8f4ff;max-width:620px;margin:50px auto;padding:20px}section{background:#102334;"
    "padding:26px;border-radius:14px}button{padding:12px 22px;background:#2ea8ff;color:white;"
    "border:0;border-radius:8px}progress{width:100%;height:24px}</style></head><body><section>"
    "<h1>SmartTank - aktualizacja</h1><p>Wybierz plik <b>SmartTank.bin</b>. Nie odłączaj "
    "zasilania podczas zapisu.</p><input id='f' type='file' accept='.bin'> <button onclick='go()'>"
    "Wgraj firmware</button><p><progress id='p' max='100' value='0'></progress></p><p id='s'></p>"
    "<script>function go(){const f=document.getElementById('f').files[0],s=document.getElementById('s'),"
    "p=document.getElementById('p');if(!f){s.textContent='Najpierw wybierz plik.';return;}"
    "if(!confirm('Rozpocząć aktualizację?'))return;const x=new XMLHttpRequest();x.open('POST','/api/ota');"
    "x.setRequestHeader('Content-Type','application/octet-stream');x.upload.onprogress=e=>{if(e.lengthComputable)"
    "p.value=e.loaded*100/e.total};x.onload=()=>{s.textContent=x.responseText};x.onerror=()=>{s.textContent="
    "'Połączenie przerwane. Sprawdź, czy urządzenie uruchomiło nową wersję.'};s.textContent='Wysyłanie...';"
    "x.send(f);}</script></section></body></html>";

static esp_err_t update_page_handler(httpd_req_t *request)
{
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    return httpd_resp_send(request, OTA_PAGE, HTTPD_RESP_USE_STRLEN);
}

static void restart_task(void *argument)
{
    (void)argument;
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
}

static esp_err_t ota_upload_handler(httpd_req_t *request)
{
    if (s_upload_active) {
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Aktualizacja już trwa");
        return ESP_ERR_INVALID_STATE;
    }

    const esp_partition_t *target = esp_ota_get_next_update_partition(NULL);
    if (target == NULL || request->content_len <= 0 ||
        (size_t)request->content_len > target->size) {
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Nieprawidłowy rozmiar firmware");
        return ESP_ERR_INVALID_SIZE;
    }

    s_upload_active = true;
    esp_ota_handle_t handle = 0;
    esp_err_t result = esp_ota_begin(target, (size_t)request->content_len, &handle);
    int remaining = request->content_len;

    while (result == ESP_OK && remaining > 0) {
        const int wanted = remaining < (int)sizeof(s_receive_buffer)
            ? remaining : (int)sizeof(s_receive_buffer);
        const int received = httpd_req_recv(request, (char *)s_receive_buffer, wanted);
        if (received == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (received <= 0) {
            result = ESP_FAIL;
            break;
        }
        result = esp_ota_write(handle, s_receive_buffer, (size_t)received);
        remaining -= received;
    }

    if (result == ESP_OK && remaining == 0) {
        result = esp_ota_end(handle);
        handle = 0;
    }
    if (result == ESP_OK) {
        result = esp_ota_set_boot_partition(target);
    }
    if (handle != 0) {
        esp_ota_abort(handle);
    }

    s_upload_active = false;
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "OTA failed: %s", esp_err_to_name(result));
        httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Błąd zapisu lub weryfikacji");
        return result;
    }

    ESP_LOGI(TAG, "OTA image written to %s; restarting", target->label);
    httpd_resp_sendstr(request, "Aktualizacja poprawna. SmartTank uruchamia się ponownie...");
    xTaskCreate(restart_task, "ota_restart", 2048, NULL, 5, NULL);
    return ESP_OK;
}

static esp_err_t start_http_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 8080;
    config.stack_size = 8192;
    config.max_uri_handlers = 4;
    config.lru_purge_enable = true;
    esp_err_t result = httpd_start(&s_server, &config);
    if (result != ESP_OK) return result;

    const httpd_uri_t page = {.uri = "/update", .method = HTTP_GET, .handler = update_page_handler};
    const httpd_uri_t upload = {.uri = "/api/ota", .method = HTTP_POST, .handler = ota_upload_handler};
    if ((result = httpd_register_uri_handler(s_server, &page)) != ESP_OK ||
        (result = httpd_register_uri_handler(s_server, &upload)) != ESP_OK) {
        httpd_stop(s_server);
        s_server = NULL;
    }
    return result;
}

static void ota_task(void *argument)
{
    (void)argument;
    while (s_server == NULL) {
        wifi_service_snapshot_t wifi;
        wifi_service_get_snapshot(&wifi);
        if (wifi.connected) {
            const esp_err_t result = start_http_server();
            if (result == ESP_OK) {
                ESP_LOGI(TAG, "OTA page ready at http://%s:8080/update", wifi.ip_address);
                break;
            }
            ESP_LOGW(TAG, "Unable to start OTA server: %s", esp_err_to_name(result));
        }
        vTaskDelay(pdMS_TO_TICKS(OTA_WIFI_WAIT_MS));
    }
    s_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t ota_service_confirm_running_firmware(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running == NULL || running->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY) {
        return ESP_OK;
    }
    const esp_err_t result = esp_ota_mark_app_valid_cancel_rollback();
    return result == ESP_ERR_OTA_ROLLBACK_INVALID_STATE ? ESP_OK : result;
}

esp_err_t ota_service_start(void)
{
    if (s_server != NULL) return ESP_OK;
    const esp_err_t result = start_http_server();
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "OTA HTTP server started on port 8080; waiting for Wi-Fi clients");
    }
    return result;
}
