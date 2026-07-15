/*
 * Publiczny interfejs serwisu wejść analogowych.
 * Udostępnia migawkę stanu modułu 8CH dla ekranów diagnostycznych, alarmów
 * i symulatora oraz funkcję startującą zadanie okresowego odczytu Modbus.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "waveshare_analog_8ch.h"

typedef enum {
    ANALOG_MODULE_STATE_DISABLED = 0,
    ANALOG_MODULE_STATE_READY,
    ANALOG_MODULE_STATE_STARTING,
    ANALOG_MODULE_STATE_ONLINE,
    ANALOG_MODULE_STATE_OFFLINE,
    ANALOG_MODULE_STATE_ERROR,
} analog_module_state_t;

typedef struct {
    bool started;
    bool hardware_enabled;
    bool self_test_ok;
    bool rs485_initialized;
    bool modbus_initialized;
    bool online;
    analog_module_state_t state;
    uint8_t slave_address;
    uint32_t baud_rate;
    waveshare_analog_8ch_snapshot_t module;
    uint32_t successful_polls;
    uint32_t failed_polls;
    uint32_t consecutive_failures;
    esp_err_t last_error;
    uint32_t revision;
} analog_module_service_snapshot_t;

esp_err_t analog_module_service_start(bool enable_hardware);
esp_err_t analog_module_service_request_refresh(void);
esp_err_t analog_module_service_set_ai1_4_20ma(void);
void analog_module_service_get_snapshot(
    analog_module_service_snapshot_t *snapshot
);
const char *analog_module_service_state_name(analog_module_state_t state);
