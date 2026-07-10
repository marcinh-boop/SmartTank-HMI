#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/uart.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#define SMARTTANK_RS485_UART_NUM UART_NUM_2
#define SMARTTANK_RS485_RX_GPIO  43
#define SMARTTANK_RS485_TX_GPIO  44

typedef struct {
    uart_port_t uart_num;
    int tx_gpio;
    int rx_gpio;
    int baud_rate;
    uart_word_length_t data_bits;
    uart_parity_t parity;
    uart_stop_bits_t stop_bits;
    int rx_buffer_size;
} rs485_port_config_t;

void rs485_port_get_board_default_config(rs485_port_config_t *config);
esp_err_t rs485_port_init(const rs485_port_config_t *config);
esp_err_t rs485_port_deinit(void);
bool rs485_port_is_initialized(void);

esp_err_t rs485_port_exchange(
    const uint8_t *request,
    size_t request_len,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_len,
    TickType_t response_timeout
);
