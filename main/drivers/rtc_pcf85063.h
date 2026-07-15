/*
 * Sterownik sprzętowy rtc_pcf85063.h: izoluje rejestry, piny i operacje urządzenia od reszty aplikacji.
 * Ten nagłówek określa publiczne typy i funkcje dostępne dla innych części programu.
 * Oddzielenie odpowiedzialności ułatwia testowanie, diagnostykę i późniejszą rozbudowę urządzenia.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c.h"
#include "esp_err.h"

#define SMARTTANK_RTC_I2C_PORT    I2C_NUM_0
#define SMARTTANK_RTC_I2C_ADDRESS 0x51U

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t weekday;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    bool oscillator_stopped;
} rtc_datetime_t;

esp_err_t rtc_pcf85063_probe(void);
esp_err_t rtc_pcf85063_read(rtc_datetime_t *datetime);
esp_err_t rtc_pcf85063_write(const rtc_datetime_t *datetime);
bool rtc_pcf85063_datetime_is_valid(const rtc_datetime_t *datetime);
