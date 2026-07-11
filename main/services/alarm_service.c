#include "alarm_service.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "analog_module_service.h"
#include "app_model.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define ALARM_TASK_STACK       4096U
#define ALARM_TASK_PRIORITY    2U
#define ALARM_EVALUATION_MS    500U
#define ALARM_MODBUS_FAILURES  3U
#define VALID_EPOCH_MINIMUM    1704067200LL

_Static_assert(ALARM_ID_COUNT == ALARM_SERVICE_ITEM_COUNT, "Alarm item count mismatch");

static const char *TAG = "alarm_service";
static SemaphoreHandle_t s_mutex;
static TaskHandle_t s_task;
static alarm_service_snapshot_t s_snapshot;

static alarm_event_t s_event_log[ALARM_EVENT_LOG_CAPACITY];
static uint8_t s_event_start;
static uint8_t s_event_count;
static uint32_t s_event_revision;
static uint32_t s_event_total_count;
static uint32_t s_event_sequence;

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

static int64_t current_epoch(void)
{
    const time_t now = time(NULL);
    return (int64_t)now >= VALID_EPOCH_MINIMUM ? (int64_t)now : 0LL;
}

static uint32_t current_uptime(void)
{
    smarttank_state_t state;
    app_model_get_snapshot(&state);
    return state.system.uptime_seconds;
}

static void initialize_item(
    alarm_item_t *item,
    alarm_id_t id,
    alarm_severity_t severity,
    const char *title)
{
    if (item == NULL) {
        return;
    }

    memset(item, 0, sizeof(*item));
    item->id = id;
    item->severity = severity;
    copy_text(item->title, sizeof(item->title), title);
}

static void record_event_locked(
    const alarm_item_t *item,
    alarm_event_type_t type,
    int64_t epoch,
    uint32_t uptime)
{
    if (item == NULL) {
        return;
    }

    uint8_t slot;
    if (s_event_count < ALARM_EVENT_LOG_CAPACITY) {
        slot = (uint8_t)((s_event_start + s_event_count) % ALARM_EVENT_LOG_CAPACITY);
        s_event_count++;
    } else {
        slot = s_event_start;
        s_event_start = (uint8_t)((s_event_start + 1U) % ALARM_EVENT_LOG_CAPACITY);
    }

    alarm_event_t *event = &s_event_log[slot];
    memset(event, 0, sizeof(*event));
    event->sequence = ++s_event_sequence;
    event->id = item->id;
    event->severity = item->severity;
    event->type = type;
    event->epoch = epoch;
    event->uptime = uptime;
    copy_text(event->title, sizeof(event->title), item->title);
    copy_text(event->message, sizeof(event->message), item->message);

    s_event_total_count++;
    s_event_revision++;

    ESP_LOGI(
        TAG,
        "Alarm event #%lu: %s / %s",
        (unsigned long)event->sequence,
        item->title,
        alarm_service_event_name(type)
    );
}

static bool set_message(alarm_item_t *item, const char *message)
{
    if (item == NULL || message == NULL) {
        return false;
    }

    if (strncmp(item->message, message, sizeof(item->message)) == 0) {
        return false;
    }

    copy_text(item->message, sizeof(item->message), message);
    return true;
}

static bool set_condition(
    alarm_item_t *item,
    bool condition,
    int64_t epoch,
    uint32_t uptime)
{
    if (item == NULL) {
        return false;
    }

    if (condition && !item->active) {
        item->active = true;
        item->acknowledged = false;
        item->active_since_epoch = epoch;
        item->active_since_uptime = uptime;
        item->cleared_epoch = 0LL;
        item->cleared_uptime = 0U;
        item->occurrence_count++;
        record_event_locked(item, ALARM_EVENT_ACTIVATED, epoch, uptime);
        return true;
    }

    if (!condition && item->active) {
        item->active = false;
        item->cleared_epoch = epoch;
        item->cleared_uptime = uptime;
        record_event_locked(item, ALARM_EVENT_CLEARED, epoch, uptime);
        return true;
    }

    return false;
}

static void recalculate_counts(void)
{
    s_snapshot.active_count = 0U;
    s_snapshot.unacknowledged_count = 0U;

    for (uint8_t index = 0U; index < ALARM_SERVICE_ITEM_COUNT; index++) {
        const alarm_item_t *item = &s_snapshot.items[index];
        if (!item->active) {
            continue;
        }

        s_snapshot.active_count++;
        if (!item->acknowledged) {
            s_snapshot.unacknowledged_count++;
        }
    }
}

static void evaluate_alarms(void)
{
    smarttank_state_t state;
    app_model_get_snapshot(&state);

    analog_module_service_snapshot_t analog;
    analog_module_service_get_snapshot(&analog);

    const bool tank_sensor_fault =
        !state.tank.valid ||
        state.tank.health == SENSOR_HEALTH_OFFLINE ||
        state.tank.health == SENSOR_HEALTH_FAULT;

    const bool tank_warning =
        !tank_sensor_fault &&
        state.tank.level_percent >= state.tank_config.warning_percent &&
        state.tank.level_percent < state.tank_config.critical_percent;

    const bool tank_critical =
        !tank_sensor_fault &&
        state.tank.level_percent >= state.tank_config.critical_percent;

    const bool modbus_fault =
        analog.started &&
        analog.hardware_enabled &&
        analog.consecutive_failures >= ALARM_MODBUS_FAILURES &&
        (analog.state == ANALOG_MODULE_STATE_OFFLINE ||
         analog.state == ANALOG_MODULE_STATE_ERROR);

    char warning_message[ALARM_MESSAGE_MAX_LEN + 1U];
    char critical_message[ALARM_MESSAGE_MAX_LEN + 1U];
    char sensor_message[ALARM_MESSAGE_MAX_LEN + 1U];
    char modbus_message[ALARM_MESSAGE_MAX_LEN + 1U];

    snprintf(
        warning_message,
        sizeof(warning_message),
        "Poziom %d%%; prog ostrzegawczy %d%%",
        state.tank.level_percent,
        state.tank_config.warning_percent
    );
    snprintf(
        critical_message,
        sizeof(critical_message),
        "Poziom %d%%; prog krytyczny %d%%",
        state.tank.level_percent,
        state.tank_config.critical_percent
    );
    snprintf(
        sensor_message,
        sizeof(sensor_message),
        "Brak poprawnego pomiaru poziomu; stan czujnika %d",
        (int)state.tank.health
    );
    snprintf(
        modbus_message,
        sizeof(modbus_message),
        "Brak odpowiedzi: %lu kolejnych bledow; slave %u",
        (unsigned long)analog.consecutive_failures,
        (unsigned int)analog.slave_address
    );

    const int64_t epoch = current_epoch();
    const uint32_t uptime = state.system.uptime_seconds;

    if (!state_lock()) {
        return;
    }

    bool changed = false;

    changed |= set_message(
        &s_snapshot.items[ALARM_ID_TANK_WARNING],
        warning_message
    );
    changed |= set_condition(
        &s_snapshot.items[ALARM_ID_TANK_WARNING],
        tank_warning,
        epoch,
        uptime
    );

    changed |= set_message(
        &s_snapshot.items[ALARM_ID_TANK_CRITICAL],
        critical_message
    );
    changed |= set_condition(
        &s_snapshot.items[ALARM_ID_TANK_CRITICAL],
        tank_critical,
        epoch,
        uptime
    );

    changed |= set_message(
        &s_snapshot.items[ALARM_ID_TANK_SENSOR],
        sensor_message
    );
    changed |= set_condition(
        &s_snapshot.items[ALARM_ID_TANK_SENSOR],
        tank_sensor_fault,
        epoch,
        uptime
    );

    changed |= set_message(
        &s_snapshot.items[ALARM_ID_MODBUS_COMMUNICATION],
        modbus_message
    );
    changed |= set_condition(
        &s_snapshot.items[ALARM_ID_MODBUS_COMMUNICATION],
        modbus_fault,
        epoch,
        uptime
    );

    const uint8_t previous_active = s_snapshot.active_count;
    const uint8_t previous_unacknowledged = s_snapshot.unacknowledged_count;
    recalculate_counts();

    if (previous_active != s_snapshot.active_count ||
        previous_unacknowledged != s_snapshot.unacknowledged_count) {
        changed = true;
    }

    if (changed) {
        s_snapshot.revision++;
    }

    state_unlock();
}

static void alarm_task(void *argument)
{
    (void)argument;

    TickType_t last_wake = xTaskGetTickCount();
    ESP_LOGI(TAG, "Alarm evaluation service started");

    while (true) {
        evaluate_alarms();
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(ALARM_EVALUATION_MS));
    }
}

esp_err_t alarm_service_start(void)
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
    memset(s_event_log, 0, sizeof(s_event_log));
    s_event_start = 0U;
    s_event_count = 0U;
    s_event_revision = 1U;
    s_event_total_count = 0U;
    s_event_sequence = 0U;

    initialize_item(
        &s_snapshot.items[ALARM_ID_TANK_WARNING],
        ALARM_ID_TANK_WARNING,
        ALARM_SEVERITY_WARNING,
        "Wysoki poziom szamba"
    );
    initialize_item(
        &s_snapshot.items[ALARM_ID_TANK_CRITICAL],
        ALARM_ID_TANK_CRITICAL,
        ALARM_SEVERITY_CRITICAL,
        "Krytyczny poziom szamba"
    );
    initialize_item(
        &s_snapshot.items[ALARM_ID_TANK_SENSOR],
        ALARM_ID_TANK_SENSOR,
        ALARM_SEVERITY_CRITICAL,
        "Czujnik poziomu szamba"
    );
    initialize_item(
        &s_snapshot.items[ALARM_ID_MODBUS_COMMUNICATION],
        ALARM_ID_MODBUS_COMMUNICATION,
        ALARM_SEVERITY_WARNING,
        "Komunikacja Modbus"
    );
    s_snapshot.started = true;
    s_snapshot.revision = 1U;

    const BaseType_t result = xTaskCreate(
        alarm_task,
        "alarm_service",
        ALARM_TASK_STACK,
        NULL,
        ALARM_TASK_PRIORITY,
        &s_task
    );

    if (result != pdPASS) {
        s_task = NULL;
        s_snapshot.started = false;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

void alarm_service_get_snapshot(alarm_service_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return;
    }

    if (!state_lock()) {
        memset(snapshot, 0, sizeof(*snapshot));
        return;
    }

    *snapshot = s_snapshot;
    state_unlock();
}

void alarm_service_get_event_log(alarm_event_log_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return;
    }

    memset(snapshot, 0, sizeof(*snapshot));
    if (!state_lock()) {
        return;
    }

    snapshot->count = s_event_count;
    snapshot->revision = s_event_revision;
    snapshot->total_count = s_event_total_count;

    for (uint8_t index = 0U; index < s_event_count; index++) {
        const uint8_t source_index = (uint8_t)(
            (s_event_start + s_event_count - 1U - index) % ALARM_EVENT_LOG_CAPACITY
        );
        snapshot->events[index] = s_event_log[source_index];
    }

    state_unlock();
}

esp_err_t alarm_service_acknowledge(alarm_id_t id)
{
    if ((unsigned int)id >= (unsigned int)ALARM_ID_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    const int64_t epoch = current_epoch();
    const uint32_t uptime = current_uptime();

    if (!state_lock()) {
        return ESP_ERR_INVALID_STATE;
    }

    alarm_item_t *item = &s_snapshot.items[id];
    if (!item->active) {
        state_unlock();
        return ESP_ERR_INVALID_STATE;
    }

    if (!item->acknowledged) {
        item->acknowledged = true;
        record_event_locked(item, ALARM_EVENT_ACKNOWLEDGED, epoch, uptime);
        recalculate_counts();
        s_snapshot.revision++;
        ESP_LOGI(TAG, "Alarm acknowledged: %s", item->title);
    }

    state_unlock();
    return ESP_OK;
}

esp_err_t alarm_service_acknowledge_all(void)
{
    const int64_t epoch = current_epoch();
    const uint32_t uptime = current_uptime();

    if (!state_lock()) {
        return ESP_ERR_INVALID_STATE;
    }

    bool changed = false;
    for (uint8_t index = 0U; index < ALARM_SERVICE_ITEM_COUNT; index++) {
        alarm_item_t *item = &s_snapshot.items[index];
        if (item->active && !item->acknowledged) {
            item->acknowledged = true;
            record_event_locked(item, ALARM_EVENT_ACKNOWLEDGED, epoch, uptime);
            changed = true;
        }
    }

    if (changed) {
        recalculate_counts();
        s_snapshot.revision++;
        ESP_LOGI(TAG, "All active alarms acknowledged");
    }

    state_unlock();
    return ESP_OK;
}

const char *alarm_service_severity_name(alarm_severity_t severity)
{
    return severity == ALARM_SEVERITY_CRITICAL ? "KRYTYCZNY" : "OSTRZEZENIE";
}

const char *alarm_service_event_name(alarm_event_type_t type)
{
    switch (type) {
        case ALARM_EVENT_ACTIVATED:
            return "AKTYWACJA";
        case ALARM_EVENT_ACKNOWLEDGED:
            return "POTWIERDZONO";
        case ALARM_EVENT_CLEARED:
            return "USTAPIL";
        default:
            return "NIEZNANE";
    }
}
