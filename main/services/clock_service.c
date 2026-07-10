#include "clock_service.h"

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define CLOCK_POLL_INTERVAL_MS 1000U
#define CLOCK_TASK_STACK_SIZE  3072U
#define CLOCK_TASK_PRIORITY    2U
#define CLOCK_MIN_VALID_YEAR   2024U
#define CLOCK_TIMEZONE         "CET-1CEST,M3.5.0/2,M10.5.0/3"

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

static bool local_tm_to_datetime(const struct tm *local_time, rtc_datetime_t *datetime)
{
    if (local_time == NULL || datetime == NULL) {
        return false;
    }

    const int year = local_time->tm_year + 1900;
    if (year < (int)CLOCK_MIN_VALID_YEAR || year > 2099) {
        return false;
    }

    *datetime = (rtc_datetime_t) {
        .year = (uint16_t)year,
        .month = (uint8_t)(local_time->tm_mon + 1),
        .day = (uint8_t)local_time->tm_mday,
        .weekday = (uint8_t)local_time->tm_wday,
        .hour = (uint8_t)local_time->tm_hour,
        .minute = (uint8_t)local_time->tm_min,
        .second = (uint8_t)local_time->tm_sec,
        .oscillator_stopped = false,
    };

    return rtc_pcf85063_datetime_is_valid(datetime);
}

static esp_err_t datetime_to_epoch(
    const rtc_datetime_t *datetime,
    time_t *epoch)
{
    if (!rtc_pcf85063_datetime_is_valid(datetime) || epoch == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    struct tm local_time = {
        .tm_sec = datetime->second,
        .tm_min = datetime->minute,
        .tm_hour = datetime->hour,
        .tm_mday = datetime->day,
        .tm_mon = (int)datetime->month - 1,
        .tm_year = (int)datetime->year - 1900,
        .tm_isdst = -1,
    };

    const time_t converted = mktime(&local_time);
    if (converted == (time_t)-1) {
        return ESP_FAIL;
    }

    *epoch = converted;
    return ESP_OK;
}

static esp_err_t set_system_epoch(time_t epoch)
{
    const struct timeval value = {
        .tv_sec = epoch,
        .tv_usec = 0,
    };

    return settimeofday(&value, NULL) == 0 ? ESP_OK : ESP_FAIL;
}

static void update_system_snapshot_locked(void)
{
    time_t now = 0;
    time(&now);

    struct tm local_time = {0};
    rtc_datetime_t datetime = {0};

    s_snapshot.system_time_valid =
        localtime_r(&now, &local_time) != NULL &&
        local_tm_to_datetime(&local_time, &datetime);

    if (s_snapshot.system_time_valid) {
        s_snapshot.local_datetime = datetime;
    }
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
            datetime.year >= CLOCK_MIN_VALID_YEAR &&
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

    update_system_snapshot_locked();

    const bool present = s_snapshot.rtc_present;
    const bool valid = s_snapshot.time_valid;
    const rtc_datetime_t logged_datetime = s_snapshot.datetime;
    unlock_snapshot();

    log_state_change(present, valid, &logged_datetime);
}

static esp_err_t restore_system_time_from_rtc(void)
{
    clock_service_snapshot_t snapshot;
    clock_service_get_snapshot(&snapshot);

    if (!snapshot.time_valid) {
        return ESP_ERR_INVALID_STATE;
    }

    time_t epoch = 0;
    esp_err_t result = datetime_to_epoch(&snapshot.datetime, &epoch);
    if (result != ESP_OK) {
        return result;
    }

    result = set_system_epoch(epoch);
    if (result != ESP_OK) {
        return result;
    }

    if (lock_snapshot()) {
        s_snapshot.source = CLOCK_TIME_SOURCE_RTC;
        update_system_snapshot_locked();
        unlock_snapshot();
    }

    ESP_LOGI(TAG, "System clock restored from RTC");
    return ESP_OK;
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

    if (setenv("TZ", CLOCK_TIMEZONE, 1) != 0) {
        return ESP_FAIL;
    }
    tzset();

    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutex();
        if (s_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    memset(&s_snapshot, 0, sizeof(s_snapshot));
    s_snapshot.started = true;
    s_snapshot.source = CLOCK_TIME_SOURCE_NONE;
    s_snapshot.last_error = ESP_ERR_INVALID_STATE;

    poll_rtc();

    const esp_err_t restore_result = restore_system_time_from_rtc();
    if (restore_result != ESP_OK && restore_result != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(
            TAG,
            "Unable to restore system clock from RTC: %s",
            esp_err_to_name(restore_result)
        );
    }

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

    update_system_snapshot_locked();
    *snapshot = s_snapshot;
    unlock_snapshot();
}

esp_err_t clock_service_set_datetime(const rtc_datetime_t *datetime)
{
    if (!rtc_pcf85063_datetime_is_valid(datetime) ||
        datetime->year < CLOCK_MIN_VALID_YEAR) {
        return ESP_ERR_INVALID_ARG;
    }

    time_t epoch = 0;
    esp_err_t result = datetime_to_epoch(datetime, &epoch);
    if (result != ESP_OK) {
        return result;
    }

    result = rtc_pcf85063_write(datetime);
    if (result != ESP_OK) {
        return result;
    }

    result = set_system_epoch(epoch);
    if (result != ESP_OK) {
        return result;
    }

    poll_rtc();

    if (lock_snapshot()) {
        s_snapshot.source = CLOCK_TIME_SOURCE_MANUAL;
        update_system_snapshot_locked();
        unlock_snapshot();
    }

    return ESP_OK;
}

esp_err_t clock_service_accept_ntp_time(time_t epoch)
{
    struct tm local_time = {0};
    rtc_datetime_t datetime = {0};

    if (localtime_r(&epoch, &local_time) == NULL ||
        !local_tm_to_datetime(&local_time, &datetime)) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_err_t system_result = set_system_epoch(epoch);
    if (system_result != ESP_OK) {
        return system_result;
    }

    const esp_err_t rtc_result = rtc_pcf85063_write(&datetime);
    if (rtc_result == ESP_OK) {
        poll_rtc();
    }

    if (lock_snapshot()) {
        s_snapshot.source = CLOCK_TIME_SOURCE_NTP;
        s_snapshot.system_time_valid = true;
        s_snapshot.local_datetime = datetime;
        s_snapshot.last_ntp_sync_epoch = epoch;
        unlock_snapshot();
    }

    return rtc_result;
}

const char *clock_service_source_name(clock_time_source_t source)
{
    switch (source) {
    case CLOCK_TIME_SOURCE_RTC:
        return "RTC";
    case CLOCK_TIME_SOURCE_NTP:
        return "NTP";
    case CLOCK_TIME_SOURCE_MANUAL:
        return "RECZNE";
    case CLOCK_TIME_SOURCE_NONE:
    default:
        return "BRAK";
    }
}
