#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define ALARM_SERVICE_ITEM_COUNT 4U
#define ALARM_TITLE_MAX_LEN      31U
#define ALARM_MESSAGE_MAX_LEN    95U

typedef enum {
    ALARM_ID_TANK_WARNING = 0,
    ALARM_ID_TANK_CRITICAL,
    ALARM_ID_TANK_SENSOR,
    ALARM_ID_MODBUS_COMMUNICATION,
    ALARM_ID_COUNT,
} alarm_id_t;

typedef enum {
    ALARM_SEVERITY_WARNING = 0,
    ALARM_SEVERITY_CRITICAL,
} alarm_severity_t;

typedef struct {
    alarm_id_t id;
    alarm_severity_t severity;
    char title[ALARM_TITLE_MAX_LEN + 1U];
    char message[ALARM_MESSAGE_MAX_LEN + 1U];
    bool active;
    bool acknowledged;
    int64_t active_since_epoch;
    int64_t cleared_epoch;
    uint32_t active_since_uptime;
    uint32_t cleared_uptime;
    uint32_t occurrence_count;
} alarm_item_t;

typedef struct {
    bool started;
    uint8_t active_count;
    uint8_t unacknowledged_count;
    uint32_t revision;
    alarm_item_t items[ALARM_SERVICE_ITEM_COUNT];
} alarm_service_snapshot_t;

esp_err_t alarm_service_start(void);
void alarm_service_get_snapshot(alarm_service_snapshot_t *snapshot);
esp_err_t alarm_service_acknowledge(alarm_id_t id);
esp_err_t alarm_service_acknowledge_all(void);
const char *alarm_service_severity_name(alarm_severity_t severity);
