/*
 * Generator danych demonstracyjnych dla elementów bez aktywnego sprzętu.
 * Gdy moduł analogowy jest uruchomiony, nie nadpisuje AI1 ani AI2; pozostawia
 * je serwisowi rzeczywistych pomiarów. Nadal może zasilać pozostałe funkcje,
 * aby interfejs był użyteczny podczas rozwoju bez kompletu czujników.
 */
#include "data_simulator.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "app_model.h"
#include "analog_module_service.h"
#include "measurement_history.h"
#include "well_settings.h"

static const char *TAG = "data_sim";
static TaskHandle_t s_simulator_task;

static uint32_t triangle_wave(uint32_t step, uint32_t amplitude)
{
    const uint32_t period = amplitude * 2U;
    const uint32_t position = step % period;
    return position <= amplitude ? position : period - position;
}

static void publish_simulated_state(uint32_t tick)
{
    smarttank_state_t state;
    app_model_get_snapshot(&state);

    analog_module_service_snapshot_t analog;
    analog_module_service_get_snapshot(&analog);
    const bool live_tank_source = analog.started && analog.hardware_enabled;
    const bool live_well_source = analog.started && analog.hardware_enabled;

    const tank_channel_config_t *config = &state.tank_config;
    const int tank_percent = 65 + (int)triangle_wave(tick / 2U, 20U);
    const float tank_span_mm = config->distance_empty_mm - config->distance_full_mm;
    const float tank_distance_mm =
        config->distance_empty_mm - tank_span_mm * (float)tank_percent / 100.0f;

    tank_measurement_t tank = {
        .level_percent = tank_percent,
        .volume_m3 = config->capacity_m3 * (float)tank_percent / 100.0f,
        .capacity_m3 = config->capacity_m3,
        .distance_mm = tank_distance_mm,
        .current_ma = 4.0f + (float)tank_percent * 0.16f,
        .valid = true,
        .health = SENSOR_HEALTH_OK,
        .sample_counter = tick + 1U,
    };

    if (tank_percent >= config->critical_percent) {
        tank.health = SENSOR_HEALTH_CRITICAL;
    } else if (tank_percent >= config->warning_percent) {
        tank.health = SENSOR_HEALTH_WARNING;
    }

    well_settings_t well_config;
    well_settings_get(&well_config);

    const int well_percent = 55 + (int)triangle_wave(tick / 3U, 20U);
    well_measurement_t well = {
        .water_column_m = well_config.well_depth_m * (float)well_percent / 100.0f,
        .well_depth_m = well_config.well_depth_m,
        .distance_mm = well_config.distance_empty_mm -
            (well_config.distance_empty_mm - well_config.distance_full_mm) *
            (float)well_percent / 100.0f,
        .current_ma = 4.0f + (float)well_percent * 0.16f,
        .valid = true,
        .health = SENSOR_HEALTH_OK,
        .sample_counter = tick + 1U,
    };

    if (well_percent <= well_config.critical_percent) {
        well.health = SENSOR_HEALTH_CRITICAL;
    } else if (well_percent <= well_config.warning_percent) {
        well.health = SENSOR_HEALTH_WARNING;
    }

    system_status_t system = {
        .simulation_active = true,
        .modbus_connected = analog.modbus_initialized,
        .analog_module_connected = analog.online,
        .uptime_seconds = tick,
    };

    if (!live_tank_source) {
        app_model_update_tank(&tank);
    }
    if (!live_well_source) {
        app_model_update_well(&well);
    }
    app_model_update_system(&system);

    measurement_history_add(
        live_tank_source ? state.tank.level_percent : tank.level_percent,
        live_tank_source ? state.tank.valid : tank.valid,
        live_well_source ? state.well.water_column_m : well.water_column_m,
        live_well_source ? state.well.well_depth_m : well.well_depth_m,
        live_well_source ? state.well.valid : well.valid,
        tick
    );
}

static void simulator_task(void *arg)
{
    (void)arg;

    TickType_t last_wake = xTaskGetTickCount();
    uint32_t tick = 0;

    ESP_LOGI(TAG, "SmartTank measurement simulator started");

    while (true) {
        publish_simulated_state(tick++);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000));
    }
}

esp_err_t data_simulator_start(void)
{
    if (s_simulator_task != NULL) {
        return ESP_OK;
    }

    const BaseType_t result = xTaskCreate(
        simulator_task,
        "data_sim",
        3072,
        NULL,
        2,
        &s_simulator_task
    );

    return result == pdPASS ? ESP_OK : ESP_FAIL;
}
