#include "data_simulator.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "app_model.h"
#include "measurement_history.h"

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

    const float well_column_m = 2.40f + (float)triangle_wave(tick / 3U, 12U) * 0.05f;
    well_measurement_t well = {
        .water_column_m = well_column_m,
        .well_depth_m = 4.00f,
        .valid = true,
        .health = SENSOR_HEALTH_OK,
        .sample_counter = tick + 1U,
    };

    const float temperature_c = 17.0f + (float)triangle_wave(tick / 5U, 10U) * 0.20f;
    const int rain_percent = 10 + (int)triangle_wave(tick / 4U, 20U);
    weather_measurement_t weather = {
        .temperature_c = temperature_c,
        .rain_percent = rain_percent,
        .wind_kmh = 8.0f + (float)triangle_wave(tick / 3U, 8U),
        .humidity_percent = 55 + (int)triangle_wave(tick / 4U, 12U),
        .valid = true,
        .sample_counter = tick + 1U,
    };

    const char *description = rain_percent >= 25 ? "Przelotny deszcz" : "Zachmurzenie";
    strncpy(weather.description, description, sizeof(weather.description) - 1U);
    weather.description[sizeof(weather.description) - 1U] = '\0';

    system_status_t system = {
        .simulation_active = true,
        .modbus_connected = false,
        .analog_module_connected = false,
        .uptime_seconds = tick,
    };

    app_model_update_tank(&tank);
    app_model_update_well(&well);
    app_model_update_weather(&weather);
    app_model_update_system(&system);

    measurement_history_add(
        tank.level_percent,
        well.water_column_m,
        well.well_depth_m,
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
