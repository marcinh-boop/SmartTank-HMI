#include "rs485_port.h"

#include "esp_log.h"
#include "freertos/semphr.h"

#define RS485_DEFAULT_BAUD_RATE       115200
#define RS485_DEFAULT_RX_BUFFER_SIZE  512
#define RS485_INTERBYTE_TIMEOUT_MS    20

static const char *TAG = "rs485_port";
static bool s_initialized;
static uart_port_t s_uart_num = SMARTTANK_RS485_UART_NUM;
static SemaphoreHandle_t s_bus_mutex;

void rs485_port_get_board_default_config(rs485_port_config_t *config)
{
    if (config == NULL) {
        return;
    }

    *config = (rs485_port_config_t) {
        .uart_num = SMARTTANK_RS485_UART_NUM,
        .tx_gpio = SMARTTANK_RS485_TX_GPIO,
        .rx_gpio = SMARTTANK_RS485_RX_GPIO,
        .baud_rate = RS485_DEFAULT_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .rx_buffer_size = RS485_DEFAULT_RX_BUFFER_SIZE,
    };
}

esp_err_t rs485_port_init(const rs485_port_config_t *config)
{
    if (config == NULL ||
        config->baud_rate <= 0 ||
        config->rx_buffer_size < 128) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_initialized) {
        return ESP_OK;
    }

    s_bus_mutex = xSemaphoreCreateMutex();
    if (s_bus_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    const uart_config_t uart_config = {
        .baud_rate = config->baud_rate,
        .data_bits = config->data_bits,
        .parity = config->parity,
        .stop_bits = config->stop_bits,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_driver_install(
        config->uart_num,
        config->rx_buffer_size,
        0,
        0,
        NULL,
        0
    );
    if (err != ESP_OK) {
        vSemaphoreDelete(s_bus_mutex);
        s_bus_mutex = NULL;
        return err;
    }

    err = uart_param_config(config->uart_num, &uart_config);
    if (err != ESP_OK) {
        uart_driver_delete(config->uart_num);
        vSemaphoreDelete(s_bus_mutex);
        s_bus_mutex = NULL;
        return err;
    }

    err = uart_set_pin(
        config->uart_num,
        config->tx_gpio,
        config->rx_gpio,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE
    );
    if (err != ESP_OK) {
        uart_driver_delete(config->uart_num);
        vSemaphoreDelete(s_bus_mutex);
        s_bus_mutex = NULL;
        return err;
    }

    err = uart_set_mode(config->uart_num, UART_MODE_UART);
    if (err != ESP_OK) {
        uart_driver_delete(config->uart_num);
        vSemaphoreDelete(s_bus_mutex);
        s_bus_mutex = NULL;
        return err;
    }

    s_uart_num = config->uart_num;
    s_initialized = true;

    ESP_LOGI(
        TAG,
        "RS485 ready: UART%d TX=%d RX=%d baud=%d auto-direction",
        (int)config->uart_num,
        config->tx_gpio,
        config->rx_gpio,
        config->baud_rate
    );

    return ESP_OK;
}

esp_err_t rs485_port_deinit(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    const esp_err_t err = uart_driver_delete(s_uart_num);

    if (s_bus_mutex != NULL) {
        vSemaphoreDelete(s_bus_mutex);
        s_bus_mutex = NULL;
    }

    s_initialized = false;
    return err;
}

bool rs485_port_is_initialized(void)
{
    return s_initialized;
}

esp_err_t rs485_port_exchange(
    const uint8_t *request,
    size_t request_len,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_len,
    TickType_t response_timeout)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (request == NULL ||
        request_len == 0U ||
        response == NULL ||
        response_capacity == 0U ||
        response_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_bus_mutex, response_timeout) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t result = ESP_OK;
    *response_len = 0U;

    uart_flush_input(s_uart_num);

    const int written = uart_write_bytes(
        s_uart_num,
        (const char *)request,
        request_len
    );
    if (written < 0 || (size_t)written != request_len) {
        result = ESP_FAIL;
        goto finish;
    }

    result = uart_wait_tx_done(s_uart_num, response_timeout);
    if (result != ESP_OK) {
        goto finish;
    }

    int received = uart_read_bytes(
        s_uart_num,
        response,
        response_capacity,
        response_timeout
    );
    if (received <= 0) {
        result = ESP_ERR_TIMEOUT;
        goto finish;
    }

    size_t total = (size_t)received;

    while (total < response_capacity) {
        received = uart_read_bytes(
            s_uart_num,
            response + total,
            response_capacity - total,
            pdMS_TO_TICKS(RS485_INTERBYTE_TIMEOUT_MS)
        );

        if (received <= 0) {
            break;
        }

        total += (size_t)received;
    }

    *response_len = total;

finish:
    xSemaphoreGive(s_bus_mutex);
    return result;
}
