/*
 * Sterownik sprzętowy waveshare_analog_8ch.h: izoluje rejestry, piny i operacje urządzenia od reszty aplikacji.
 * Ten nagłówek określa publiczne typy i funkcje dostępne dla innych części programu.
 * Oddzielenie odpowiedzialności ułatwia testowanie, diagnostykę i późniejszą rozbudowę urządzenia.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define WAVESHARE_ANALOG_8CH_CHANNEL_COUNT 8U

#define WAVESHARE_ANALOG_REG_INPUT_BASE       0x0000U
#define WAVESHARE_ANALOG_REG_MODE_BASE        0x1000U
#define WAVESHARE_ANALOG_REG_UART_CONFIG      0x2000U
#define WAVESHARE_ANALOG_REG_DEVICE_ADDRESS   0x4000U
#define WAVESHARE_ANALOG_REG_FIRMWARE_VERSION 0x8000U

#define WAVESHARE_ANALOG_MODE_4_20_MA 3U

typedef struct {
    uint16_t input_raw_ua[WAVESHARE_ANALOG_8CH_CHANNEL_COUNT];
    float input_ma[WAVESHARE_ANALOG_8CH_CHANNEL_COUNT];
    uint16_t channel_mode[WAVESHARE_ANALOG_8CH_CHANNEL_COUNT];
    uint16_t uart_config;
    uint16_t device_address;
    uint16_t firmware_version;
    bool inputs_valid;
    bool modes_valid;
    bool identity_valid;
} waveshare_analog_8ch_snapshot_t;

float waveshare_analog_8ch_raw_to_ma(uint16_t raw_ua);

esp_err_t waveshare_analog_8ch_read_inputs(
    waveshare_analog_8ch_snapshot_t *snapshot
);

esp_err_t waveshare_analog_8ch_read_modes(
    waveshare_analog_8ch_snapshot_t *snapshot
);

esp_err_t waveshare_analog_8ch_read_identity(
    waveshare_analog_8ch_snapshot_t *snapshot
);

esp_err_t waveshare_analog_8ch_set_channel_mode(
    uint8_t channel,
    uint16_t mode
);

esp_err_t waveshare_analog_8ch_set_ai1_4_20ma(void);

bool waveshare_analog_8ch_self_test(void);
