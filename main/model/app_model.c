#include "app_model.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static SemaphoreHandle_t s_model_mutex;
static smarttank_state_t s_state;

static bool model_lock(void)
{
    return s_model_mutex != NULL &&
           xSemaphoreTake(s_model_mutex, portMAX_DELAY) == pdTRUE;
}

static void model_unlock(void)
{
    xSemaphoreGive(s_model_mutex);
}

esp_err_t app_model_init(void)
{
    if (s_model_mutex != NULL) {
        return ESP_OK;
    }

    s_model_mutex = xSemaphoreCreateMutex();
    if (s_model_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    memset(&s_state, 0, sizeof(s_state));

    s_state.tank.level_percent = 72;
    s_state.tank.capacity_m3 = 10.50f;
    s_state.tank.volume_m3 = 7.56f;
    s_state.tank.distance_mm = 780.0f;
    s_state.tank.current_ma = 15.52f;
    s_state.tank.valid = true;
    s_state.tank.health = SENSOR_HEALTH_OK;

    strncpy(
        s_state.tank_config.sensor_model,
        "mic+130/IU/TC",
        sizeof(s_state.tank_config.sensor_model) - 1U
    );
    strncpy(
        s_state.tank_config.input_mode,
        "4-20 mA",
        sizeof(s_state.tank_config.input_mode) - 1U
    );
    s_state.tank_config.analog_channel = 1U;
    s_state.tank_config.distance_empty_mm = 1500.0f;
    s_state.tank_config.distance_full_mm = 250.0f;
    s_state.tank_config.warning_percent = 80;
    s_state.tank_config.critical_percent = 90;

    s_state.well.water_column_m = 2.81f;
    s_state.well.well_depth_m = 4.00f;
    s_state.well.valid = true;
    s_state.well.health = SENSOR_HEALTH_OK;

    s_state.weather.temperature_c = 18.6f;
    s_state.weather.rain_percent = 10;
    s_state.weather.wind_kmh = 12.0f;
    s_state.weather.humidity_percent = 62;
    strncpy(s_state.weather.description, "Zachmurzenie", sizeof(s_state.weather.description) - 1U);
    s_state.weather.valid = true;

    s_state.system.simulation_active = true;
    s_state.system.modbus_connected = false;
    s_state.system.analog_module_connected = false;
    s_state.revision = 1;

    return ESP_OK;
}

void app_model_get_snapshot(smarttank_state_t *snapshot)
{
    if (snapshot == NULL) {
        return;
    }

    if (!model_lock()) {
        memset(snapshot, 0, sizeof(*snapshot));
        return;
    }

    *snapshot = s_state;
    model_unlock();
}

void app_model_update_tank(const tank_measurement_t *measurement)
{
    if (measurement == NULL || !model_lock()) {
        return;
    }

    s_state.tank = *measurement;
    s_state.revision++;
    model_unlock();
}

void app_model_update_tank_config(const tank_channel_config_t *config)
{
    if (config == NULL || !model_lock()) {
        return;
    }

    s_state.tank_config = *config;
    s_state.tank_config.sensor_model[sizeof(s_state.tank_config.sensor_model) - 1U] = '\0';
    s_state.tank_config.input_mode[sizeof(s_state.tank_config.input_mode) - 1U] = '\0';
    s_state.revision++;
    model_unlock();
}

void app_model_update_well(const well_measurement_t *measurement)
{
    if (measurement == NULL || !model_lock()) {
        return;
    }

    s_state.well = *measurement;
    s_state.revision++;
    model_unlock();
}

void app_model_update_weather(const weather_measurement_t *measurement)
{
    if (measurement == NULL || !model_lock()) {
        return;
    }

    s_state.weather = *measurement;
    s_state.weather.description[sizeof(s_state.weather.description) - 1U] = '\0';
    s_state.revision++;
    model_unlock();
}

void app_model_update_system(const system_status_t *status)
{
    if (status == NULL || !model_lock()) {
        return;
    }

    s_state.system = *status;
    s_state.revision++;
    model_unlock();
}
