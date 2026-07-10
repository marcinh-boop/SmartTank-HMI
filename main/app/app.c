#include "app.h"

#include "esp_err.h"

#include "app_model.h"
#include "data_simulator.h"
#include "lvgl_port.h"
#include "screen_dashboard.h"

void app_start(void)
{
    ESP_ERROR_CHECK(app_model_init());
    ESP_ERROR_CHECK(data_simulator_start());

    if (lvgl_port_lock(-1)) {
        screen_dashboard_create();
        lvgl_port_unlock();
    }
}
