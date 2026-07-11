#include "waveshare_analog_8ch.h"

#include <string.h>

#include "modbus_rtu_client.h"

float waveshare_analog_8ch_raw_to_ma(uint16_t raw_ua)
{
    return (float)raw_ua / 1000.0f;
}

esp_err_t waveshare_analog_8ch_read_inputs(
    waveshare_analog_8ch_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t registers[WAVESHARE_ANALOG_8CH_CHANNEL_COUNT] = {0};
    const esp_err_t err = modbus_rtu_read_input_registers(
        WAVESHARE_ANALOG_REG_INPUT_BASE,
        WAVESHARE_ANALOG_8CH_CHANNEL_COUNT,
        registers
    );
    if (err != ESP_OK) {
        snapshot->inputs_valid = false;
        return err;
    }

    for (uint8_t index = 0U;
         index < WAVESHARE_ANALOG_8CH_CHANNEL_COUNT;
         index++) {
        snapshot->input_raw_ua[index] = registers[index];
        snapshot->input_ma[index] = waveshare_analog_8ch_raw_to_ma(registers[index]);
    }

    snapshot->inputs_valid = true;
    return ESP_OK;
}

esp_err_t waveshare_analog_8ch_read_modes(
    waveshare_analog_8ch_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t registers[WAVESHARE_ANALOG_8CH_CHANNEL_COUNT] = {0};
    const esp_err_t err = modbus_rtu_read_holding_registers(
        WAVESHARE_ANALOG_REG_MODE_BASE,
        WAVESHARE_ANALOG_8CH_CHANNEL_COUNT,
        registers
    );
    if (err != ESP_OK) {
        snapshot->modes_valid = false;
        return err;
    }

    memcpy(
        snapshot->channel_mode,
        registers,
        sizeof(snapshot->channel_mode)
    );
    snapshot->modes_valid = true;
    return ESP_OK;
}

esp_err_t waveshare_analog_8ch_read_identity(
    waveshare_analog_8ch_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t value = 0U;
    esp_err_t err = modbus_rtu_read_holding_registers(
        WAVESHARE_ANALOG_REG_UART_CONFIG,
        1U,
        &value
    );
    if (err != ESP_OK) {
        snapshot->identity_valid = false;
        return err;
    }
    snapshot->uart_config = value;

    err = modbus_rtu_read_holding_registers(
        WAVESHARE_ANALOG_REG_DEVICE_ADDRESS,
        1U,
        &value
    );
    if (err != ESP_OK) {
        snapshot->identity_valid = false;
        return err;
    }
    snapshot->device_address = value;

    err = modbus_rtu_read_holding_registers(
        WAVESHARE_ANALOG_REG_FIRMWARE_VERSION,
        1U,
        &value
    );
    if (err != ESP_OK) {
        snapshot->identity_valid = false;
        return err;
    }
    snapshot->firmware_version = value;
    snapshot->identity_valid = true;
    return ESP_OK;
}

esp_err_t waveshare_analog_8ch_set_channel_mode(
    uint8_t channel,
    uint16_t mode)
{
    if (channel < 1U || channel > WAVESHARE_ANALOG_8CH_CHANNEL_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint16_t register_address =
        WAVESHARE_ANALOG_REG_MODE_BASE + (uint16_t)(channel - 1U);

    return modbus_rtu_write_single_register(register_address, mode);
}

esp_err_t waveshare_analog_8ch_set_ai1_4_20ma(void)
{
    return waveshare_analog_8ch_set_channel_mode(
        1U,
        WAVESHARE_ANALOG_MODE_4_20_MA
    );
}

bool waveshare_analog_8ch_self_test(void)
{
    static const uint8_t read_all_inputs[] = {
        0x01U, 0x04U, 0x00U, 0x00U, 0x00U, 0x08U
    };
    static const uint8_t read_ai1[] = {
        0x01U, 0x04U, 0x00U, 0x00U, 0x00U, 0x01U
    };
    static const uint8_t set_ai1_mode[] = {
        0x01U, 0x06U, 0x10U, 0x00U, 0x00U, 0x03U
    };

    const bool crc_frames_ok =
        modbus_rtu_crc16(read_all_inputs, sizeof(read_all_inputs)) == 0xCCF1U &&
        modbus_rtu_crc16(read_ai1, sizeof(read_ai1)) == 0xCA31U &&
        modbus_rtu_crc16(set_ai1_mode, sizeof(set_ai1_mode)) == 0x0BCDU;

    const float current = waveshare_analog_8ch_raw_to_ma(12340U);
    const bool conversion_ok = current > 12.339f && current < 12.341f;

    return crc_frames_ok && conversion_ok;
}
