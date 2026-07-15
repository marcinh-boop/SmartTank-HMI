/*
 * Definicje centralnego modelu danych SmartTank.
 * Struktury opisują bieżące pomiary, konfigurację szamba, studnię, pogodę
 * oraz stan systemu. Ekrany pobierają spójną migawkę, a serwisy pomiarowe
 * aktualizują wyłącznie należące do nich fragmenty modelu.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define WEATHER_FORECAST_DAYS 4U

typedef enum {
    SENSOR_HEALTH_OK = 0,
    SENSOR_HEALTH_WARNING,
    SENSOR_HEALTH_CRITICAL,
    SENSOR_HEALTH_OFFLINE,
    SENSOR_HEALTH_FAULT,
} sensor_health_t;

typedef struct {
    int level_percent;
    float volume_m3;
    float capacity_m3;
    float distance_mm;
    float current_ma;
    bool valid;
    sensor_health_t health;
    uint32_t sample_counter;
} tank_measurement_t;

typedef struct {
    char sensor_model[32];
    char input_mode[16];
    uint8_t analog_channel;
    float distance_empty_mm;
    float distance_full_mm;
    float capacity_m3;
    int warning_percent;
    int critical_percent;
} tank_channel_config_t;

typedef struct {
    float water_column_m;
    float well_depth_m;
    float distance_mm;
    float current_ma;
    bool valid;
    sensor_health_t health;
    uint32_t sample_counter;
} well_measurement_t;

typedef struct {
    char day[4];
    int weather_code;
    float temperature_max_c;
    float temperature_min_c;
    int rain_percent;
    bool valid;
} weather_forecast_day_t;

typedef struct {
    float temperature_c;
    int rain_percent;
    float wind_kmh;
    int humidity_percent;
    int weather_code;
    char description[32];
    bool valid;
    bool stale;
    int64_t updated_epoch;
    uint8_t forecast_count;
    weather_forecast_day_t forecast[WEATHER_FORECAST_DAYS];
    uint32_t sample_counter;
} weather_measurement_t;

typedef struct {
    bool simulation_active;
    bool modbus_connected;
    bool analog_module_connected;
    uint32_t uptime_seconds;
} system_status_t;

typedef struct {
    tank_measurement_t tank;
    tank_channel_config_t tank_config;
    well_measurement_t well;
    weather_measurement_t weather;
    system_status_t system;
    uint32_t revision;
} smarttank_state_t;

esp_err_t app_model_init(void);
void app_model_get_snapshot(smarttank_state_t *snapshot);
void app_model_update_tank(const tank_measurement_t *measurement);
void app_model_restore_tank_config(const tank_channel_config_t *config);
void app_model_update_tank_config(const tank_channel_config_t *config);
void app_model_update_well(const well_measurement_t *measurement);
void app_model_update_weather(const weather_measurement_t *measurement);
void app_model_update_system(const system_status_t *status);
