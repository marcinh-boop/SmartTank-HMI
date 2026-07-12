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
#define ANALOG_MODULE_RETRY_COUNT           1U
#define ANALOG_MODULE_TASK_STACK            4096U
#define ANALOG_MODULE_TASK_PRIORITY         2U
#define ANALOG_MODULE_POLL_INTERVAL_MS      1000U
#define ANALOG_MODULE_OFFLINE_THRESHOLD     3U

static const char *TAG = "analog_module";

static SemaphoreHandle_t s_mutex;
static TaskHandle_t s_task;
static analog_module_service_snapshot_t s_snapshot;

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

static void publish_transport_ready(void)
{
    if (!state_lock()) {
        return;
    }

    s_snapshot.rs485_initialized = rs485_port_is_initialized();
    s_snapshot.modbus_initialized = modbus_rtu_client_is_initialized();
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

static void analog_module_task(void *argument)
{
    (void)argument;

    bool identity_attempted = false;
    bool modes_attempted = false;
    waveshare_analog_8ch_snapshot_t module = {0};

    while (true) {
        bool identity_updated = false;
        bool modes_updated = false;

        /*
         * Odczyt wejść 0x0000-0x0007 funkcją 04 jest potwierdzony
         * w przykładach producenta, dlatego stan ONLINE zależy właśnie od niego.
         * Rejestry identyfikacji i trybów są odczytywane pomocniczo tylko raz
         * i ich brak nie blokuje podstawowej komunikacji z modułem.
         */
        const esp_err_t err = waveshare_analog_8ch_read_inputs(&module);

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

        (void)ulTaskNotifyTake(
            pdTRUE,
            pdMS_TO_TICKS(ANALOG_MODULE_POLL_INTERVAL_MS)
        );
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

    esp_err_t err = rs485_port_init(&port_config);
    if (err != ESP_OK) {
        publish_failure(err);
        return err;
    }

    const modbus_rtu_client_config_t modbus_config = {
        .slave_address = ANALOG_MODULE_DEFAULT_SLAVE_ADDRESS,
        .response_timeout_ms = ANALOG_MODULE_RESPONSE_TIMEOUT_MS,
        .retry_count = ANALOG_MODULE_RETRY_COUNT,
    };

    err = modbus_rtu_client_init(&modbus_config);
    if (err != ESP_OK) {
        publish_failure(err);
        return err;
    }

    publish_transport_ready();

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

    ESP_LOGI(
        TAG,
        "Waveshare 8CH polling enabled: slave=%u, %u baud, 8N1",
        (unsigned int)ANALOG_MODULE_DEFAULT_SLAVE_ADDRESS,
        (unsigned int)ANALOG_MODULE_DEFAULT_BAUD_RATE
    );
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
