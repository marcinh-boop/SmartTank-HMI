#include "rs485_port.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/semphr.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"

#define RS485_DEFAULT_BAUD_RATE       9600
#define RS485_DEFAULT_RX_BUFFER_SIZE  2048
#define RS485_RX_STREAM_SIZE          1024
#define RS485_RX_TASK_BUFFER_SIZE     256
#define RS485_RX_TASK_STACK_SIZE      3072
#define RS485_RX_TASK_PRIORITY        10
#define RS485_READ_SLICE_MS           20
#define RS485_INTERBYTE_TIMEOUT_MS    20
#define RS485_CONFIG_TIMEOUT_MS       250
#define RS485_STOP_TIMEOUT_MS         250

static const char *TAG = "rs485_port";
static bool s_initialized;
static volatile bool s_rx_task_running;
static uart_port_t s_uart_num = SMARTTANK_RS485_UART_NUM;
static SemaphoreHandle_t s_bus_mutex;
static StreamBufferHandle_t s_rx_stream;
static TaskHandle_t s_rx_task;

static void rs485_rx_task(void *arg)
{
    (void)arg;

    uint8_t data[RS485_RX_TASK_BUFFER_SIZE];

    while (s_rx_task_running) {
        const int received = uart_read_bytes(
            s_uart_num,
            data,
            sizeof(data),
            pdMS_TO_TICKS(RS485_READ_SLICE_MS)
        );

        if (received > 0 && s_rx_stream != NULL) {
            const size_t sent = xStreamBufferSend(
                s_rx_stream,
                data,
                (size_t)received,
                0
            );

            if (sent != (size_t)received) {
                ESP_LOGW(
                    TAG,
                    "UART%d RX stream overflow: received=%d stored=%u",
                    (int)s_uart_num,
                    received,
                    (unsigned int)sent
                );
            }
        }
    }

    s_rx_task = NULL;
    vTaskDelete(NULL);
}

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

    s_rx_stream = xStreamBufferCreate(RS485_RX_STREAM_SIZE, 1);
    if (s_rx_stream == NULL) {
        vSemaphoreDelete(s_bus_mutex);
        s_bus_mutex = NULL;
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
        vStreamBufferDelete(s_rx_stream);
        s_rx_stream = NULL;
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
        vStreamBufferDelete(s_rx_stream);
        s_rx_stream = NULL;
        vSemaphoreDelete(s_bus_mutex);
        s_bus_mutex = NULL;
        return err;
    }

    s_uart_num = config->uart_num;
    s_rx_task_running = true;

    if (xTaskCreate(
            rs485_rx_task,
            "rs485_rx",
            RS485_RX_TASK_STACK_SIZE,
            NULL,
            RS485_RX_TASK_PRIORITY,
            &s_rx_task) != pdPASS) {
        s_rx_task_running = false;
        uart_driver_delete(config->uart_num);
        vStreamBufferDelete(s_rx_stream);
        s_rx_stream = NULL;
        vSemaphoreDelete(s_bus_mutex);
        s_bus_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_initialized = true;

    ESP_LOGI(
        TAG,
        "RS485 ready: UART%d TX=%d RX=%d baud=%d, continuous RX task",
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

    s_rx_task_running = false;

    const TickType_t start = xTaskGetTickCount();
    while (s_rx_task != NULL &&
           (xTaskGetTickCount() - start) < pdMS_TO_TICKS(RS485_STOP_TIMEOUT_MS)) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    const esp_err_t err = uart_driver_delete(s_uart_num);

    if (s_rx_stream != NULL) {
        vStreamBufferDelete(s_rx_stream);
        s_rx_stream = NULL;
    }

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
    if (!s_initialized || s_bus_mutex == NULL || s_rx_stream == NULL) {
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
    xStreamBufferReset(s_rx_stream);

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

static size_t remove_local_echo(
    const uint8_t *request,
    size_t request_len,
    uint8_t *response,
    size_t response_len)
{
    if (response_len > request_len &&
        memcmp(response, request, request_len) == 0) {
        const size_t remaining = response_len - request_len;
        memmove(response, response + request_len, remaining);
        return remaining;
    }

    return response_len;
}

esp_err_t rs485_port_exchange(
    const uint8_t *request,
    size_t request_len,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_len,
    TickType_t response_timeout)
{
    if (!s_initialized || s_rx_stream == NULL) {
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
    xStreamBufferReset(s_rx_stream);

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

    size_t total = 0U;
    const TickType_t start = xTaskGetTickCount();

    while (total < response_capacity) {
        const TickType_t elapsed = xTaskGetTickCount() - start;
        if (elapsed >= response_timeout) {
            break;
        }

        const TickType_t wait_ticks = total == 0U
            ? response_timeout - elapsed
            : pdMS_TO_TICKS(RS485_INTERBYTE_TIMEOUT_MS);

        const size_t received = xStreamBufferReceive(
            s_rx_stream,
            response + total,
            response_capacity - total,
            wait_ticks
        );

        if (received == 0U) {
            break;
        }

        total += received;
    }

    total = remove_local_echo(request, request_len, response, total);

    if (total == 0U) {
        result = ESP_ERR_TIMEOUT;
        goto finish;
    }

    *response_len = total;
    ESP_LOGI(
        TAG,
        "UART%d received %u bytes",
        (int)s_uart_num,
        (unsigned int)total
    );

finish:
    xSemaphoreGive(s_bus_mutex);
    return result;
}
