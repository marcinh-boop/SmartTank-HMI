#include "analog_module_service.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "modbus_rtu_client.h"
#include "rs485_port.h"

#define ANALOG_MODULE_DEFAULT_SLAVE_ADDRESS 1U
#define ANALOG_MODULE_DEFAULT_BAUD_RATE     9600U
#define ANALOG_MODULE_RESPONSE_TIMEOUT_MS   350U
#define ANALOG_MODULE_RETRY_COUNT           0U
#define ANALOG_MODULE_TASK_STACK            4096U
#define ANALOG_MODULE_TASK_PRIORITY         2U
#define ANALOG_MODULE_POLL_INTERVAL_MS      1000U
#define ANALOG_MODULE_OFFLINE_POLL_MS       3000U
#define ANALOG_MODULE_OFFLINE_THRESHOLD     3U

#define ANALOG_MODULE_PROBE_TIMEOUT_MS      150U
#define ANALOG_MODULE_PROBE_SETTLE_MS       20U

#define MODBUS_FUNCTION_READ_HOLDING_REGISTERS 0x03U
#define MODBUS_FUNCTION_READ_INPUT_REGISTERS   0x04U
#define MODBUS_PROBE_REQUEST_SIZE              8U
#define MODBUS_PROBE_RESPONSE_SIZE             16U
#define WAVESHARE_DEVICE_ADDRESS_REGISTER      0x4000U

static const char *TAG = "analog_module";

static SemaphoreHandle_t s_mutex;
static TaskHandle_t s_task;
static analog_module_service_snapshot_t s_snapshot;

typedef struct {
    uint32_t baud_rate;
    uart_parity_t parity;
    const char *format_name;
} analog_module_probe_line_t;

/*
 * Protokół V2 producenta dopuszcza 4800-256000 oraz brak/parzystą/
 * nieparzystą kontrolę parzystości. Najpierw sprawdzamy ustawienia domyślne.
 */
static const analog_module_probe_line_t s_probe_lines[] = {
    {9600U, UART_PARITY_DISABLE, "8N1"},
    {19200U, UART_PARITY_DISABLE, "8N1"},
    {38400U, UART_PARITY_DISABLE, "8N1"},
    {57600U, UART_PARITY_DISABLE, "8N1"},
    {115200U, UART_PARITY_DISABLE, "8N1"},
    {4800U, UART_PARITY_DISABLE, "8N1"},
    {128000U, UART_PARITY_DISABLE, "8N1"},
    {256000U, UART_PARITY_DISABLE, "8N1"},

    {9600U, UART_PARITY_EVEN, "8E1"},
    {19200U, UART_PARITY_EVEN, "8E1"},
    {38400U, UART_PARITY_EVEN, "8E1"},
    {57600U, UART_PARITY_EVEN, "8E1"},
    {115200U, UART_PARITY_EVEN, "8E1"},
    {4800U, UART_PARITY_EVEN, "8E1"},
    {128000U, UART_PARITY_EVEN, "8E1"},
    {256000U, UART_PARITY_EVEN, "8E1"},

    {9600U, UART_PARITY_ODD, "8O1"},
    {19200U, UART_PARITY_ODD, "8O1"},
    {38400U, UART_PARITY_ODD, "8O1"},
    {57600U, UART_PARITY_ODD, "8O1"},
    {115200U, UART_PARITY_ODD, "8O1"},
    {4800U, UART_PARITY_ODD, "8O1"},
    {128000U, UART_PARITY_ODD, "8O1"},
    {256000U, UART_PARITY_ODD, "8O1"},
};

static bool state_lock(void)
{
    return s_mutex != NULL &&
           xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE;
}

static void state_unlock(void)
{
    xSemaphoreGive(s_mutex);
}

const char *analog_module_service_state_name(analog_module_state_t state)
{
    switch (state) {
        case ANALOG_MODULE_STATE_DISABLED:
            return "NIEAKTYWNY";
        case ANALOG_MODULE_STATE_READY:
            return "GOTOWY";
        case ANALOG_MODULE_STATE_STARTING:
            return "START";
        case ANALOG_MODULE_STATE_ONLINE:
            return "ONLINE";
        case ANALOG_MODULE_STATE_OFFLINE:
            return "NIEPODLACZONY";
        case ANALOG_MODULE_STATE_ERROR:
            return "BLAD";
        default:
            return "?";
    }
}

static void publish_start_state(
    bool hardware_enabled,
    bool self_test_ok,
    analog_module_state_t state,
    esp_err_t last_error)
{
    if (!state_lock()) {
        return;
    }

    memset(&s_snapshot, 0, sizeof(s_snapshot));
    s_snapshot.started = true;
    s_snapshot.hardware_enabled = hardware_enabled;
    s_snapshot.self_test_ok = self_test_ok;
    s_snapshot.state = state;
    s_snapshot.slave_address = ANALOG_MODULE_DEFAULT_SLAVE_ADDRESS;
    s_snapshot.baud_rate = ANALOG_MODULE_DEFAULT_BAUD_RATE;
    s_snapshot.last_error = last_error;
    s_snapshot.revision = 1U;

    state_unlock();
}

static void publish_rs485_ready(void)
{
    if (!state_lock()) {
        return;
    }

    s_snapshot.rs485_initialized = rs485_port_is_initialized();
    s_snapshot.modbus_initialized = false;
    s_snapshot.state = ANALOG_MODULE_STATE_STARTING;
    s_snapshot.last_error = ESP_OK;
    s_snapshot.revision++;

    state_unlock();
}

static void publish_transport_ready(uint8_t slave_address, uint32_t baud_rate)
{
    if (!state_lock()) {
        return;
    }

    s_snapshot.rs485_initialized = rs485_port_is_initialized();
    s_snapshot.modbus_initialized = modbus_rtu_client_is_initialized();
    s_snapshot.slave_address = slave_address;
    s_snapshot.baud_rate = baud_rate;
    s_snapshot.state = ANALOG_MODULE_STATE_STARTING;
    s_snapshot.last_error = ESP_OK;
    s_snapshot.revision++;

    state_unlock();
}

static void publish_success(
    const waveshare_analog_8ch_snapshot_t *module,
    bool identity_updated,
    bool modes_updated)
{
    if (module == NULL || !state_lock()) {
        return;
    }

    if (identity_updated) {
        s_snapshot.module.uart_config = module->uart_config;
        s_snapshot.module.device_address = module->device_address;
        s_snapshot.module.firmware_version = module->firmware_version;
        s_snapshot.module.identity_valid = module->identity_valid;
    }

    if (modes_updated) {
        memcpy(
            s_snapshot.module.channel_mode,
            module->channel_mode,
            sizeof(s_snapshot.module.channel_mode)
        );
        s_snapshot.module.modes_valid = module->modes_valid;
    }

    memcpy(
        s_snapshot.module.input_raw_ua,
        module->input_raw_ua,
        sizeof(s_snapshot.module.input_raw_ua)
    );
    memcpy(
        s_snapshot.module.input_ma,
        module->input_ma,
        sizeof(s_snapshot.module.input_ma)
    );
    s_snapshot.module.inputs_valid = module->inputs_valid;

    s_snapshot.online = true;
    s_snapshot.state = ANALOG_MODULE_STATE_ONLINE;
    s_snapshot.successful_polls++;
    s_snapshot.consecutive_failures = 0U;
    s_snapshot.last_error = ESP_OK;
    s_snapshot.revision++;

    state_unlock();
}

static void publish_failure(esp_err_t error)
{
    if (!state_lock()) {
        return;
    }

    s_snapshot.failed_polls++;
    s_snapshot.consecutive_failures++;
    s_snapshot.last_error = error;
    s_snapshot.online = false;
    s_snapshot.state =
        s_snapshot.consecutive_failures >= ANALOG_MODULE_OFFLINE_THRESHOLD
            ? ANALOG_MODULE_STATE_OFFLINE
            : ANALOG_MODULE_STATE_STARTING;
    s_snapshot.revision++;

    state_unlock();
}

static bool frame_crc_is_valid(const uint8_t *frame, size_t frame_len)
{
    if (frame == NULL || frame_len < 4U) {
        return false;
    }

    const uint16_t received_crc =
        (uint16_t)frame[frame_len - 2U] |
        ((uint16_t)frame[frame_len - 1U] << 8U);
    const uint16_t calculated_crc = modbus_rtu_crc16(frame, frame_len - 2U);

    return received_crc == calculated_crc;
}

/*
 * Waveshare udostępnia producentowską komendę rozgłoszeniową:
 * 00 03 40 00 00 01 90 1B
 * Moduł odpowiada własnym adresem w pierwszym bajcie oraz w danych rejestru.
 * Pozwala to wykryć adres bez zgadywania zakresu 1-255 i bez zapisu ustawień.
 */
static bool probe_device_address(uint8_t *detected_address)
{
    if (detected_address == NULL) {
        return false;
    }

    uint8_t request[MODBUS_PROBE_REQUEST_SIZE] = {
        0x00U,
        MODBUS_FUNCTION_READ_HOLDING_REGISTERS,
        (uint8_t)(WAVESHARE_DEVICE_ADDRESS_REGISTER >> 8U),
        (uint8_t)(WAVESHARE_DEVICE_ADDRESS_REGISTER & 0xFFU),
        0x00U,
        0x01U,
        0x00U,
        0x00U,
    };

    const uint16_t crc = modbus_rtu_crc16(request, 6U);
    request[6] = (uint8_t)(crc & 0xFFU);
    request[7] = (uint8_t)(crc >> 8U);

    uint8_t response[MODBUS_PROBE_RESPONSE_SIZE] = {0};
    size_t response_len = 0U;
    const esp_err_t err = rs485_port_exchange(
        request,
        sizeof(request),
        response,
        sizeof(response),
        &response_len,
        pdMS_TO_TICKS(ANALOG_MODULE_PROBE_TIMEOUT_MS)
    );

    if (err != ESP_OK ||
        response_len != 7U ||
        !frame_crc_is_valid(response, response_len) ||
        response[1] != MODBUS_FUNCTION_READ_HOLDING_REGISTERS ||
        response[2] != 0x02U ||
        response[3] != 0x00U) {
        return false;
    }

    const uint8_t address = response[4];
    if (address == 0U || response[0] != address) {
        return false;
    }

    *detected_address = address;
    return true;
}

/* Awaryjny test zgodności ze starszym firmware modułu przy adresie 1. */
static bool probe_default_address(void)
{
    uint8_t request[MODBUS_PROBE_REQUEST_SIZE] = {
        ANALOG_MODULE_DEFAULT_SLAVE_ADDRESS,
        MODBUS_FUNCTION_READ_INPUT_REGISTERS,
        0x00U,
        0x00U,
        0x00U,
        0x01U,
        0x00U,
        0x00U,
    };

    const uint16_t crc = modbus_rtu_crc16(request, 6U);
    request[6] = (uint8_t)(crc & 0xFFU);
    request[7] = (uint8_t)(crc >> 8U);

    uint8_t response[MODBUS_PROBE_RESPONSE_SIZE] = {0};
    size_t response_len = 0U;
    const esp_err_t err = rs485_port_exchange(
        request,
        sizeof(request),
        response,
        sizeof(response),
        &response_len,
        pdMS_TO_TICKS(ANALOG_MODULE_PROBE_TIMEOUT_MS)
    );

    if (err != ESP_OK ||
        response_len < 5U ||
        !frame_crc_is_valid(response, response_len) ||
        response[0] != ANALOG_MODULE_DEFAULT_SLAVE_ADDRESS) {
        return false;
    }

    return response[1] == MODBUS_FUNCTION_READ_INPUT_REGISTERS ||
           response[1] ==
               (uint8_t)(MODBUS_FUNCTION_READ_INPUT_REGISTERS | 0x80U);
}

static bool detect_module(
    uint8_t *slave_address,
    uint32_t *baud_rate,
    uart_parity_t *parity,
    const char **format_name)
{
    if (slave_address == NULL ||
        baud_rate == NULL ||
        parity == NULL ||
        format_name == NULL) {
        return false;
    }

    ESP_LOGI(TAG, "Starting 8CH broadcast parameter detection");

    for (size_t line_index = 0U;
         line_index < sizeof(s_probe_lines) / sizeof(s_probe_lines[0]);
         line_index++) {
        const analog_module_probe_line_t *line = &s_probe_lines[line_index];
        const esp_err_t config_err = rs485_port_set_line_config(
            (int)line->baud_rate,
            line->parity,
            UART_STOP_BITS_1
        );
        if (config_err != ESP_OK) {
            ESP_LOGW(
                TAG,
                "Unable to set scan line %u %s: %s",
                (unsigned int)line->baud_rate,
                line->format_name,
                esp_err_to_name(config_err)
            );
            continue;
        }

        ESP_LOGI(
            TAG,
            "Probing 8CH address at %u %s",
            (unsigned int)line->baud_rate,
            line->format_name
        );
        vTaskDelay(pdMS_TO_TICKS(ANALOG_MODULE_PROBE_SETTLE_MS));

        uint8_t address = 0U;
        if (!probe_device_address(&address)) {
            if (!probe_default_address()) {
                continue;
            }
            address = ANALOG_MODULE_DEFAULT_SLAVE_ADDRESS;
        }

        *slave_address = address;
        *baud_rate = line->baud_rate;
        *parity = line->parity;
        *format_name = line->format_name;

        ESP_LOGI(
            TAG,
            "8CH detected: slave=%u, baud=%u, format=%s",
            (unsigned int)address,
            (unsigned int)line->baud_rate,
            line->format_name
        );
        return true;
    }

    return false;
}

static esp_err_t initialize_modbus(uint8_t slave_address)
{
    const modbus_rtu_client_config_t modbus_config = {
        .slave_address = slave_address,
        .response_timeout_ms = ANALOG_MODULE_RESPONSE_TIMEOUT_MS,
        .retry_count = ANALOG_MODULE_RETRY_COUNT,
    };

    return modbus_rtu_client_init(&modbus_config);
}

static void analog_module_task(void *argument)
{
    (void)argument;

    uint8_t slave_address = ANALOG_MODULE_DEFAULT_SLAVE_ADDRESS;
    uint32_t baud_rate = ANALOG_MODULE_DEFAULT_BAUD_RATE;
    uart_parity_t parity = UART_PARITY_DISABLE;
    const char *format_name = "8N1";

    const bool detected = detect_module(
        &slave_address,
        &baud_rate,
        &parity,
        &format_name
    );

    if (!detected) {
        slave_address = ANALOG_MODULE_DEFAULT_SLAVE_ADDRESS;
        baud_rate = ANALOG_MODULE_DEFAULT_BAUD_RATE;
        parity = UART_PARITY_DISABLE;
        format_name = "8N1";

        ESP_LOGW(
            TAG,
            "8CH broadcast detection found no response; restoring slave=1, 9600 8N1"
        );
    }

    esp_err_t err = rs485_port_set_line_config(
        (int)baud_rate,
        parity,
        UART_STOP_BITS_1
    );
    if (err != ESP_OK) {
        publish_failure(err);
        ESP_LOGE(
            TAG,
            "Unable to apply detected RS485 parameters: %s",
            esp_err_to_name(err)
        );
        vTaskDelete(NULL);
        return;
    }

    err = initialize_modbus(slave_address);
    if (err != ESP_OK) {
        publish_failure(err);
        ESP_LOGE(
            TAG,
            "Unable to initialize Modbus client: %s",
            esp_err_to_name(err)
        );
        vTaskDelete(NULL);
        return;
    }

    publish_transport_ready(slave_address, baud_rate);
    ESP_LOGI(
        TAG,
        "Waveshare 8CH polling enabled: slave=%u, %u baud, %s",
        (unsigned int)slave_address,
        (unsigned int)baud_rate,
        format_name
    );

    bool identity_attempted = false;
    bool modes_attempted = false;
    waveshare_analog_8ch_snapshot_t module = {0};

    while (true) {
        bool identity_updated = false;
        bool modes_updated = false;

        err = waveshare_analog_8ch_read_inputs(&module);

        if (err == ESP_OK) {
            if (!identity_attempted) {
                identity_attempted = true;
                const esp_err_t identity_err =
                    waveshare_analog_8ch_read_identity(&module);
                if (identity_err == ESP_OK) {
                    identity_updated = true;
                } else {
                    ESP_LOGW(
                        TAG,
                        "8CH identity registers unavailable: %s",
                        esp_err_to_name(identity_err)
                    );
                }
            }

            if (!modes_attempted) {
                modes_attempted = true;
                const esp_err_t modes_err =
                    waveshare_analog_8ch_read_modes(&module);
                if (modes_err == ESP_OK) {
                    modes_updated = true;
                } else {
                    ESP_LOGW(
                        TAG,
                        "8CH mode registers unavailable: %s",
                        esp_err_to_name(modes_err)
                    );
                }
            }

            publish_success(&module, identity_updated, modes_updated);
            ESP_LOGI(
                TAG,
                "8CH online: AI1=%u uA (%.3f mA), AI2=%u uA",
                (unsigned int)module.input_raw_ua[0],
                module.input_ma[0],
                (unsigned int)module.input_raw_ua[1]
            );
        } else {
            publish_failure(err);
            ESP_LOGW(TAG, "8CH input poll failed: %s", esp_err_to_name(err));
        }

        analog_module_service_snapshot_t snapshot;
        analog_module_service_get_snapshot(&snapshot);
        const uint32_t delay_ms = snapshot.online
            ? ANALOG_MODULE_POLL_INTERVAL_MS
            : ANALOG_MODULE_OFFLINE_POLL_MS;

        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(delay_ms));
    }
}

esp_err_t analog_module_service_start(bool enable_hardware)
{
    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutex();
        if (s_mutex == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (s_snapshot.started) {
        return ESP_OK;
    }

    const bool self_test_ok = waveshare_analog_8ch_self_test();
    if (!self_test_ok) {
        publish_start_state(
            enable_hardware,
            false,
            ANALOG_MODULE_STATE_ERROR,
            ESP_FAIL
        );
        ESP_LOGE(TAG, "Waveshare 8CH driver self-test failed");
        return ESP_FAIL;
    }

    if (!enable_hardware) {
        publish_start_state(
            false,
            true,
            ANALOG_MODULE_STATE_READY,
            ESP_OK
        );
        ESP_LOGI(
            TAG,
            "Waveshare 8CH driver ready; hardware polling disabled"
        );
        return ESP_OK;
    }

    publish_start_state(
        true,
        true,
        ANALOG_MODULE_STATE_STARTING,
        ESP_OK
    );

    rs485_port_config_t port_config;
    rs485_port_get_board_default_config(&port_config);
    port_config.baud_rate = ANALOG_MODULE_DEFAULT_BAUD_RATE;
    port_config.data_bits = UART_DATA_8_BITS;
    port_config.parity = UART_PARITY_DISABLE;
    port_config.stop_bits = UART_STOP_BITS_1;

    const esp_err_t err = rs485_port_init(&port_config);
    if (err != ESP_OK) {
        publish_failure(err);
        return err;
    }

    publish_rs485_ready();

    const BaseType_t task_result = xTaskCreate(
        analog_module_task,
        "analog_8ch",
        ANALOG_MODULE_TASK_STACK,
        NULL,
        ANALOG_MODULE_TASK_PRIORITY,
        &s_task
    );
    if (task_result != pdPASS) {
        s_task = NULL;
        publish_failure(ESP_ERR_NO_MEM);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Waveshare 8CH broadcast Modbus detection started");
    return ESP_OK;
}

esp_err_t analog_module_service_request_refresh(void)
{
    if (s_task == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xTaskNotifyGive(s_task);
    return ESP_OK;
}

esp_err_t analog_module_service_set_ai1_4_20ma(void)
{
    analog_module_service_snapshot_t snapshot;
    analog_module_service_get_snapshot(&snapshot);

    if (!snapshot.hardware_enabled ||
        !snapshot.modbus_initialized ||
        !snapshot.online) {
        return ESP_ERR_INVALID_STATE;
    }

    const esp_err_t err = waveshare_analog_8ch_set_ai1_4_20ma();
    if (err == ESP_OK) {
        (void)analog_module_service_request_refresh();
    }
    return err;
}

void analog_module_service_get_snapshot(
    analog_module_service_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return;
    }

    if (!state_lock()) {
        memset(snapshot, 0, sizeof(*snapshot));
        snapshot->last_error = ESP_ERR_INVALID_STATE;
        return;
    }

    *snapshot = s_snapshot;
    state_unlock();
}
