#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    uint8_t slave_address;
    uint32_t response_timeout_ms;
    uint8_t retry_count;
} modbus_rtu_client_config_t;

typedef struct {
    uint32_t requests;
    uint32_t responses;
    uint32_t timeouts;
    uint32_t crc_errors;
    uint32_t protocol_errors;
    uint32_t exceptions;
    uint8_t last_exception_code;
    esp_err_t last_error;
} modbus_rtu_diagnostics_t;

esp_err_t modbus_rtu_client_init(const modbus_rtu_client_config_t *config);
bool modbus_rtu_client_is_initialized(void);

esp_err_t modbus_rtu_read_holding_registers(
    uint16_t start_address,
    uint16_t register_count,
    uint16_t *registers
);

esp_err_t modbus_rtu_read_input_registers(
    uint16_t start_address,
    uint16_t register_count,
    uint16_t *registers
);

uint16_t modbus_rtu_crc16(const uint8_t *data, size_t length);
void modbus_rtu_get_diagnostics(modbus_rtu_diagnostics_t *diagnostics);
void modbus_rtu_reset_diagnostics(void);
