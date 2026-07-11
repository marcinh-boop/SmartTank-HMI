#include "modbus_rtu_client.h"

#include <stdbool.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "rs485_port.h"

#define MODBUS_FUNCTION_READ_HOLDING_REGISTERS 0x03U
#define MODBUS_FUNCTION_READ_INPUT_REGISTERS   0x04U
#define MODBUS_FUNCTION_WRITE_SINGLE_REGISTER  0x06U
#define MODBUS_MAX_READ_REGISTERS              125U
#define MODBUS_REQUEST_SIZE                    8U
#define MODBUS_MAX_RESPONSE_SIZE               255U

static bool s_initialized;
static modbus_rtu_client_config_t s_config;
static modbus_rtu_diagnostics_t s_diagnostics;
static SemaphoreHandle_t s_client_mutex;

uint16_t modbus_rtu_crc16(const uint8_t *data, size_t length)
{
    if (data == NULL && length != 0U) {
        return 0U;
    }

    uint16_t crc = 0xFFFFU;

    for (size_t i = 0U; i < length; i++) {
        crc ^= data[i];

        for (uint8_t bit = 0U; bit < 8U; bit++) {
            if ((crc & 0x0001U) != 0U) {
                crc = (uint16_t)((crc >> 1U) ^ 0xA001U);
            } else {
                crc >>= 1U;
            }
        }
    }

    return crc;
}

static void copy_frame(
    uint8_t destination[MODBUS_RTU_FRAME_PREVIEW_SIZE],
    size_t *destination_len,
    const uint8_t *source,
    size_t source_len)
{
    if (destination == NULL || destination_len == NULL) {
        return;
    }

    const size_t copy_len = source_len < MODBUS_RTU_FRAME_PREVIEW_SIZE
        ? source_len
        : MODBUS_RTU_FRAME_PREVIEW_SIZE;

    memset(destination, 0, MODBUS_RTU_FRAME_PREVIEW_SIZE);
    if (source != NULL && copy_len > 0U) {
        memcpy(destination, source, copy_len);
    }
    *destination_len = copy_len;
}

static esp_err_t validate_response_crc(const uint8_t *response, size_t response_len)
{
    if (response == NULL || response_len < 4U) {
        return ESP_FAIL;
    }

    const uint16_t received_crc =
        (uint16_t)response[response_len - 2U] |
        ((uint16_t)response[response_len - 1U] << 8U);
    const uint16_t calculated_crc = modbus_rtu_crc16(
        response,
        response_len - 2U
    );

    return received_crc == calculated_crc ? ESP_OK : ESP_FAIL;
}

static bool response_is_exception(
    const uint8_t *response,
    size_t response_len,
    uint8_t function_code)
{
    if (response == NULL || response_len < 5U) {
        return false;
    }

    if (response[0] != s_config.slave_address ||
        response[1] != (uint8_t)(function_code | 0x80U)) {
        return false;
    }

    s_diagnostics.exceptions++;
    s_diagnostics.last_exception_code = response[2];
    s_diagnostics.last_error = ESP_FAIL;
    return true;
}

static esp_err_t exchange_once(
    const uint8_t *request,
    size_t request_len,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_len)
{
    s_diagnostics.requests++;
    copy_frame(
        s_diagnostics.last_request,
        &s_diagnostics.last_request_len,
        request,
        request_len
    );
    s_diagnostics.last_response_len = 0U;
    memset(
        s_diagnostics.last_response,
        0,
        sizeof(s_diagnostics.last_response)
    );

    const esp_err_t err = rs485_port_exchange(
        request,
        request_len,
        response,
        response_capacity,
        response_len,
        pdMS_TO_TICKS(s_config.response_timeout_ms)
    );

    if (response != NULL && response_len != NULL && *response_len > 0U) {
        copy_frame(
            s_diagnostics.last_response,
            &s_diagnostics.last_response_len,
            response,
            *response_len
        );
    }

    if (err == ESP_ERR_TIMEOUT) {
        s_diagnostics.timeouts++;
        s_diagnostics.last_error = err;
    } else if (err != ESP_OK) {
        s_diagnostics.protocol_errors++;
        s_diagnostics.last_error = err;
    }

    return err;
}

esp_err_t modbus_rtu_client_init(const modbus_rtu_client_config_t *config)
{
    if (config == NULL ||
        config->slave_address == 0U ||
        config->slave_address > 247U ||
        config->response_timeout_ms == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!rs485_port_is_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_client_mutex == NULL) {
        s_client_mutex = xSemaphoreCreateMutex();
        if (s_client_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (xSemaphoreTake(s_client_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    s_config = *config;
    memset(&s_diagnostics, 0, sizeof(s_diagnostics));
    s_diagnostics.last_error = ESP_OK;
    s_initialized = true;

    xSemaphoreGive(s_client_mutex);
    return ESP_OK;
}

bool modbus_rtu_client_is_initialized(void)
{
    return s_initialized;
}

static esp_err_t read_registers(
    uint8_t function_code,
    uint16_t start_address,
    uint16_t register_count,
    uint16_t *registers)
{
    if (!s_initialized || s_client_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (registers == NULL ||
        register_count == 0U ||
        register_count > MODBUS_MAX_READ_REGISTERS) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_client_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    uint8_t request[MODBUS_REQUEST_SIZE] = {
        s_config.slave_address,
        function_code,
        (uint8_t)(start_address >> 8U),
        (uint8_t)(start_address & 0xFFU),
        (uint8_t)(register_count >> 8U),
        (uint8_t)(register_count & 0xFFU),
        0U,
        0U,
    };

    const uint16_t request_crc = modbus_rtu_crc16(request, 6U);
    request[6] = (uint8_t)(request_crc & 0xFFU);
    request[7] = (uint8_t)(request_crc >> 8U);

    const uint8_t attempts = (uint8_t)(s_config.retry_count + 1U);
    esp_err_t final_error = ESP_FAIL;

    for (uint8_t attempt = 0U; attempt < attempts; attempt++) {
        uint8_t response[MODBUS_MAX_RESPONSE_SIZE] = {0};
        size_t response_len = 0U;

        esp_err_t err = exchange_once(
            request,
            sizeof(request),
            response,
            sizeof(response),
            &response_len
        );
        if (err != ESP_OK) {
            final_error = err;
            continue;
        }

        if (validate_response_crc(response, response_len) != ESP_OK) {
            s_diagnostics.crc_errors++;
            s_diagnostics.last_error = ESP_FAIL;
            final_error = ESP_FAIL;
            continue;
        }

        if (response_is_exception(response, response_len, function_code)) {
            final_error = ESP_FAIL;
            break;
        }

        if (response_len < 5U || response[0] != s_config.slave_address) {
            s_diagnostics.protocol_errors++;
            s_diagnostics.last_error = ESP_FAIL;
            final_error = ESP_FAIL;
            continue;
        }

        const uint8_t expected_byte_count = (uint8_t)(register_count * 2U);
        const size_t expected_response_len =
            (size_t)expected_byte_count + 5U;

        if (response[1] != function_code ||
            response[2] != expected_byte_count ||
            response_len != expected_response_len) {
            s_diagnostics.protocol_errors++;
            s_diagnostics.last_error = ESP_FAIL;
            final_error = ESP_FAIL;
            continue;
        }

        for (uint16_t i = 0U; i < register_count; i++) {
            const size_t offset = 3U + ((size_t)i * 2U);
            registers[i] =
                ((uint16_t)response[offset] << 8U) |
                (uint16_t)response[offset + 1U];
        }

        s_diagnostics.responses++;
        s_diagnostics.last_exception_code = 0U;
        s_diagnostics.last_error = ESP_OK;
        final_error = ESP_OK;
        break;
    }

    xSemaphoreGive(s_client_mutex);
    return final_error;
}

esp_err_t modbus_rtu_read_holding_registers(
    uint16_t start_address,
    uint16_t register_count,
    uint16_t *registers)
{
    return read_registers(
        MODBUS_FUNCTION_READ_HOLDING_REGISTERS,
        start_address,
        register_count,
        registers
    );
}

esp_err_t modbus_rtu_read_input_registers(
    uint16_t start_address,
    uint16_t register_count,
    uint16_t *registers)
{
    return read_registers(
        MODBUS_FUNCTION_READ_INPUT_REGISTERS,
        start_address,
        register_count,
        registers
    );
}

esp_err_t modbus_rtu_write_single_register(
    uint16_t register_address,
    uint16_t value)
{
    if (!s_initialized || s_client_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_client_mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    uint8_t request[MODBUS_REQUEST_SIZE] = {
        s_config.slave_address,
        MODBUS_FUNCTION_WRITE_SINGLE_REGISTER,
        (uint8_t)(register_address >> 8U),
        (uint8_t)(register_address & 0xFFU),
        (uint8_t)(value >> 8U),
        (uint8_t)(value & 0xFFU),
        0U,
        0U,
    };

    const uint16_t request_crc = modbus_rtu_crc16(request, 6U);
    request[6] = (uint8_t)(request_crc & 0xFFU);
    request[7] = (uint8_t)(request_crc >> 8U);

    const uint8_t attempts = (uint8_t)(s_config.retry_count + 1U);
    esp_err_t final_error = ESP_FAIL;

    for (uint8_t attempt = 0U; attempt < attempts; attempt++) {
        uint8_t response[MODBUS_REQUEST_SIZE] = {0};
        size_t response_len = 0U;

        esp_err_t err = exchange_once(
            request,
            sizeof(request),
            response,
            sizeof(response),
            &response_len
        );
        if (err != ESP_OK) {
            final_error = err;
            continue;
        }

        if (validate_response_crc(response, response_len) != ESP_OK) {
            s_diagnostics.crc_errors++;
            s_diagnostics.last_error = ESP_FAIL;
            final_error = ESP_FAIL;
            continue;
        }

        if (response_is_exception(
                response,
                response_len,
                MODBUS_FUNCTION_WRITE_SINGLE_REGISTER)) {
            final_error = ESP_FAIL;
            break;
        }

        if (response_len != sizeof(request) ||
            memcmp(response, request, sizeof(request)) != 0) {
            s_diagnostics.protocol_errors++;
            s_diagnostics.last_error = ESP_FAIL;
            final_error = ESP_FAIL;
            continue;
        }

        s_diagnostics.responses++;
        s_diagnostics.last_exception_code = 0U;
        s_diagnostics.last_error = ESP_OK;
        final_error = ESP_OK;
        break;
    }

    xSemaphoreGive(s_client_mutex);
    return final_error;
}

void modbus_rtu_get_diagnostics(modbus_rtu_diagnostics_t *diagnostics)
{
    if (diagnostics == NULL) {
        return;
    }

    if (s_client_mutex == NULL ||
        xSemaphoreTake(s_client_mutex, portMAX_DELAY) != pdTRUE) {
        *diagnostics = s_diagnostics;
        return;
    }

    *diagnostics = s_diagnostics;
    xSemaphoreGive(s_client_mutex);
}

void modbus_rtu_reset_diagnostics(void)
{
    if (s_client_mutex != NULL &&
        xSemaphoreTake(s_client_mutex, portMAX_DELAY) == pdTRUE) {
        memset(&s_diagnostics, 0, sizeof(s_diagnostics));
        s_diagnostics.last_error = ESP_OK;
        xSemaphoreGive(s_client_mutex);
        return;
    }

    memset(&s_diagnostics, 0, sizeof(s_diagnostics));
    s_diagnostics.last_error = ESP_OK;
}
