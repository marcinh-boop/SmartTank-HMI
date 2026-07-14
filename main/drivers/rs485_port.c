#include "rs485_port.h"

#include "esp_log.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#define RS485_DEFAULT_BAUD_RATE       9600
#define RS485_DEFAULT_RX_BUFFER_SIZE  2048
#define RS485_CONFIG_TIMEOUT_MS       250

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
        .source_clk = UART_SCLK_DEFAULT,
    };

    int intr_alloc_flags = 0;
#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    esp_err_t err = uart_driver_install(
        config->uart_num,
        config->rx_buffer_size,
        0,
        0,
        NULL,
        intr_alloc_flags
    );
    if (err != ESP_OK) {
        vSemaphoreDelete(s_bus_mutex);
        s_bus_mutex = NULL;
        return err;
    }

    err = uart_param_config(config->uart_num, &uart_config);
    if (err == ESP_OK) {
        err = uart_set_pin(
            config->uart_num,
            config->tx_gpio,
            config->rx_gpio,
            UART_PIN_NO_CHANGE,
            UART_PIN_NO_CHANGE
        );
    }

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
        "RS485 ready: UART%d TX=%d RX=%d baud=%d, Waveshare echo configuration",
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

esp_err_t rs485_port_set_line_config(
    int baud_rate,
    uart_parity_t parity,
    uart_stop_bits_t stop_bits)
{
    if (!s_initialized || s_bus_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (baud_rate <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(
            s_bus_mutex,
            pdMS_TO_TICKS(RS485_CONFIG_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    uart_flush_input(s_uart_num);

    esp_err_t err = uart_set_baudrate(s_uart_num, baud_rate);
    if (err == ESP_OK) {
        err = uart_set_parity(s_uart_num, parity);
    }
    if (err == ESP_OK) {
        err = uart_set_stop_bits(s_uart_num, stop_bits);
    }

    xSemaphoreGive(s_bus_mutex);

    if (err == ESP_OK) {
        ESP_LOGI(
            TAG,
            "RS485 line config: baud=%d parity=%d stop=%d",
            baud_rate,
            (int)parity,
            (int)stop_bits
        );
    }

    return err;
}

esp_err_t rs485_port_exchange(
    const uint8_t *request,
    size_t request_len,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_len,
    TickType_t response_timeout)
{
    if (!s_initialized || s_bus_mutex == NULL) {
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

    ESP_LOG_BUFFER_HEXDUMP(TAG, request, request_len, ESP_LOG_INFO);

    const int written = uart_write_bytes(
        s_uart_num,
        (const char *)request,
        request_len
    );
    if (written < 0 || (size_t)written != request_len) {
        result = ESP_FAIL;
        goto finish;
    }

    /*
     * Dokładnie jak w działającym przykładzie Waveshare: sterownik UART ma
     * włączony tylko bufor RX, bez kolejki zdarzeń, trybu RS485 i ręcznych
     * progów FIFO. Odczyt jest jednym blokującym uart_read_bytes().
     */
    const int received = uart_read_bytes(
        s_uart_num,
        response,
        response_capacity,
        response_timeout
    );

    if (received <= 0) {
        result = ESP_ERR_TIMEOUT;
        goto finish;
    }

    *response_len = (size_t)received;
    ESP_LOGI(
        TAG,
        "UART%d received %d bytes",
        (int)s_uart_num,
        received
    );
    ESP_LOG_BUFFER_HEXDUMP(TAG, response, (size_t)received, ESP_LOG_INFO);

finish:
    xSemaphoreGive(s_bus_mutex);
    return result;
}
