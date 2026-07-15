/*
 * Sterownik sprzętowy rtc_pcf85063.c: izoluje rejestry, piny i operacje urządzenia od reszty aplikacji.
 * Implementacja ukrywa szczegóły działania; inne moduły powinny korzystać z odpowiadającego jej API.
 * Oddzielenie odpowiedzialności ułatwia testowanie, diagnostykę i późniejszą rozbudowę urządzenia.
 */
#include "rtc_pcf85063.h"

#include <stddef.h>

#include "freertos/FreeRTOS.h"

#define RTC_REGISTER_CONTROL_1 0x00U
#define RTC_REGISTER_SECONDS   0x04U
#define RTC_CONTROL_1_STOP     0x20U
#define RTC_IO_TIMEOUT_MS      100U

static bool decode_bcd(uint8_t value, uint8_t *decoded)
{
    const uint8_t tens = (uint8_t)(value >> 4U);
    const uint8_t units = (uint8_t)(value & 0x0FU);

    if (decoded == NULL || tens > 9U || units > 9U) {
        return false;
    }

    *decoded = (uint8_t)(tens * 10U + units);
    return true;
}

static uint8_t encode_bcd(uint8_t value)
{
    return (uint8_t)(((value / 10U) << 4U) | (value % 10U));
}

static bool is_leap_year(uint16_t year)
{
    return (year % 4U == 0U && year % 100U != 0U) ||
           (year % 400U == 0U);
}

static uint8_t days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t days[] = {
        31U, 28U, 31U, 30U, 31U, 30U,
        31U, 31U, 30U, 31U, 30U, 31U,
    };

    if (month < 1U || month > 12U) {
        return 0U;
    }

    if (month == 2U && is_leap_year(year)) {
        return 29U;
    }

    return days[month - 1U];
}

bool rtc_pcf85063_datetime_is_valid(const rtc_datetime_t *datetime)
{
    if (datetime == NULL ||
        datetime->year < 2000U || datetime->year > 2099U ||
        datetime->month < 1U || datetime->month > 12U ||
        datetime->hour > 23U ||
        datetime->minute > 59U ||
        datetime->second > 59U ||
        datetime->weekday > 6U) {
        return false;
    }

    const uint8_t maximum_day = days_in_month(datetime->year, datetime->month);
    return datetime->day >= 1U && datetime->day <= maximum_day;
}

static esp_err_t read_registers(uint8_t start_register, uint8_t *data, size_t length)
{
    if (data == NULL || length == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    return i2c_master_write_read_device(
        SMARTTANK_RTC_I2C_PORT,
        SMARTTANK_RTC_I2C_ADDRESS,
        &start_register,
        1U,
        data,
        length,
        pdMS_TO_TICKS(RTC_IO_TIMEOUT_MS)
    );
}

static esp_err_t write_registers(const uint8_t *data, size_t length)
{
    if (data == NULL || length == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    return i2c_master_write_to_device(
        SMARTTANK_RTC_I2C_PORT,
        SMARTTANK_RTC_I2C_ADDRESS,
        data,
        length,
        pdMS_TO_TICKS(RTC_IO_TIMEOUT_MS)
    );
}

esp_err_t rtc_pcf85063_probe(void)
{
    uint8_t control_1 = 0U;
    return read_registers(RTC_REGISTER_CONTROL_1, &control_1, 1U);
}

esp_err_t rtc_pcf85063_read(rtc_datetime_t *datetime)
{
    if (datetime == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t registers[7] = {0};
    esp_err_t err = read_registers(
        RTC_REGISTER_SECONDS,
        registers,
        sizeof(registers)
    );
    if (err != ESP_OK) {
        return err;
    }

    rtc_datetime_t decoded = {
        .oscillator_stopped = (registers[0] & 0x80U) != 0U,
    };

    uint8_t year = 0U;
    if (!decode_bcd((uint8_t)(registers[0] & 0x7FU), &decoded.second) ||
        !decode_bcd((uint8_t)(registers[1] & 0x7FU), &decoded.minute) ||
        !decode_bcd((uint8_t)(registers[2] & 0x3FU), &decoded.hour) ||
        !decode_bcd((uint8_t)(registers[3] & 0x3FU), &decoded.day) ||
        !decode_bcd((uint8_t)(registers[4] & 0x07U), &decoded.weekday) ||
        !decode_bcd((uint8_t)(registers[5] & 0x1FU), &decoded.month) ||
        !decode_bcd(registers[6], &year)) {
        return ESP_ERR_INVALID_STATE;
    }

    decoded.year = (uint16_t)(2000U + year);
    *datetime = decoded;

    return rtc_pcf85063_datetime_is_valid(&decoded)
        ? ESP_OK
        : ESP_ERR_INVALID_STATE;
}

esp_err_t rtc_pcf85063_write(const rtc_datetime_t *datetime)
{
    if (!rtc_pcf85063_datetime_is_valid(datetime)) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t control_1 = 0U;
    esp_err_t err = read_registers(RTC_REGISTER_CONTROL_1, &control_1, 1U);
    if (err != ESP_OK) {
        return err;
    }

    const uint8_t stop_command[] = {
        RTC_REGISTER_CONTROL_1,
        (uint8_t)(control_1 | RTC_CONTROL_1_STOP),
    };
    err = write_registers(stop_command, sizeof(stop_command));
    if (err != ESP_OK) {
        return err;
    }

    const uint8_t time_command[] = {
        RTC_REGISTER_SECONDS,
        encode_bcd(datetime->second),
        encode_bcd(datetime->minute),
        encode_bcd(datetime->hour),
        encode_bcd(datetime->day),
        encode_bcd(datetime->weekday),
        encode_bcd(datetime->month),
        encode_bcd((uint8_t)(datetime->year - 2000U)),
    };

    err = write_registers(time_command, sizeof(time_command));

    const uint8_t start_command[] = {
        RTC_REGISTER_CONTROL_1,
        (uint8_t)(control_1 & (uint8_t)~RTC_CONTROL_1_STOP),
    };
    const esp_err_t start_err = write_registers(start_command, sizeof(start_command));

    return err != ESP_OK ? err : start_err;
}
