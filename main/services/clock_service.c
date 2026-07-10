#include "clock_service.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define CLOCK_POLL_INTERVAL_MS 1000U
#define CLOCK_TASK_STACK_SIZE  3072U
#define CLOCK_TASK_PRIORITY    2U

static const char *TAG = "clock";

static SemaphoreHandle_t s_mutex;
static TaskHandle_t s_task;
static clock_service_snapshot_t s_snapshot;
static bool s_last_logged_valid;
static bool s_last_logged_present;

static bool lock_snapshot(void)
{
    return s_mutex != NULL &&
           xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE;
}

static void unlock_snapshot(void)
{
    xSemaphoreGive(s_mutex);
}

static void log_state_change(bool present, bool valid, const rtc_datetime_t *datetime)
{
    if (present == s_last_logged_present && valid == s_last_logged_valid) {
        return;
    }

    s_last_logged_present = present;
    s_last_logged_valid = valid;

    if (!present) {
        ESP_LOGW(TAG, "RTC PCF85063 not responding on I2C address 0x51");
    } else if (!valid) {
        ESP_LOGW(TAG, "RTC detected, but stored date/time is not valid yet");
    } else if (datetime != NULL) {
        ESP_LOGI(
            TAG,
            "RTC ready: %04u-%02u-%02u %02u:%02u:%02u",
            (unsigned int)datetime->year,
            (unsigned int)datetime->month,
            (unsigned int)datetime->day,
            (unsigned int)datetime->hour,
            (unsigned int)datetime->minute,
            (unsigned int)datetime->second
        );
    }
}

static void poll_rtc(void)
{
    rtc_datetime_t datetime = {0};
    const esp_err_t err = rtc_pcf85063_read(&datetime);

    if (!lock_snapshot()) {
        return;
    }

    s_snapshot.started = true;
    s_snapshot.last_error = err;

    if (err == ESP_OK) {
        s_snapshot.rtc_present = true;
        s_snapshot.datetime = datetime;
        s_snapshot.oscillator_stopped = datetime.oscillator_stopped;
        s_snapshot.time_valid =
            rtc_pcf85063_datetime_is_valid(&datetime) &&
            !datetime.oscillator_stopped;
        s_snapshot.successful_reads++;
    } else {
        const esp_err_t probe_err = rtc_pcf85063_probe();
        s_snapshot.rtc_present = probe_err == ESP_OK;
        s_snapshot.time_valid = false;
        s_snapshot.oscillator_stopped = false;
        s_snapshot.read_errors++;
        if (probe_err != ESP_OK) {
            s_snapshot.last_error = probe_err;
        }
    }

    const bool present = s_snapshot.rtc_present;
    const bool valid = s_snapshot.time_valid;
    const rtc_datetime_t logged_datetime = s_snapshot.datetime;
    unlock_snapshot();

    log_state_change(present, valid, &logged_datetime);
}

static void clock_task(void *arg)
{
    (void)arg;

    TickType_t last_wake = xTaskGetTickCount();

    while (true) {
        poll_rtc();
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(CLOCK_POLL_INTERVAL_MS));
    }
}

esp_err_t clock_service_start(void)
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
    s_snapshot.last_error = ESP_ERR_INVALID_STATE;

    poll_rtc();

    const BaseType_t result = xTaskCreate(
        clock_task,
        "clock_service",
        CLOCK_TASK_STACK_SIZE,
        NULL,
        CLOCK_TASK_PRIORITY,
        &s_task
    );

    if (result != pdPASS) {
        s_task = NULL;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

void clock_service_get_snapshot(clock_service_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return;
    }

    if (!lock_snapshot()) {
        memset(snapshot, 0, sizeof(*snapshot));
        snapshot->last_error = ESP_ERR_INVALID_STATE;
        return;
    }

    *snapshot = s_snapshot;
    unlock_snapshot();
}

esp_err_t clock_service_set_datetime(const rtc_datetime_t *datetime)
{
    const esp_err_t err = rtc_pcf85063_write(datetime);
    if (err == ESP_OK) {
        poll_rtc();
    }

    return err;
}
