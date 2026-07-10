/*
 * SmartTank HMI LVGL port for the Waveshare ESP32-S3 RGB display.
 *
 * Two panel-owned frame buffers are used directly by LVGL. Flushes only
 * request a frame-buffer switch and never block waiting for VSYNC. The RGB
 * peripheral performs the actual scanout continuously.
 */

#include <assert.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "lvgl_port.h"

static const char *TAG = "lv_port";
static SemaphoreHandle_t s_lvgl_mutex;
static TaskHandle_t s_lvgl_task;

static void tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(LVGL_PORT_TICK_PERIOD_MS);
}

static esp_err_t tick_init(void)
{
    const esp_timer_create_args_t args = {
        .callback = tick_cb,
        .name = "lvgl_tick",
    };

    esp_timer_handle_t timer = NULL;
    ESP_RETURN_ON_ERROR(esp_timer_create(&args, &timer), TAG, "tick timer create failed");
    return esp_timer_start_periodic(timer, LVGL_PORT_TICK_PERIOD_MS * 1000U);
}

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)drv->user_data;

    esp_lcd_panel_draw_bitmap(
        panel,
        area->x1,
        area->y1,
        area->x2 + 1,
        area->y2 + 1,
        color_map
    );

    /* Do not wait here. Blocking the LVGL task on VSYNC caused watchdog
     * starvation and made touch-triggered refreshes visibly jump. */
    lv_disp_flush_ready(drv);
}

static lv_disp_t *display_init(esp_lcd_panel_handle_t panel)
{
    static lv_disp_draw_buf_t draw_buf;
    static lv_disp_drv_t disp_drv;

    void *fb0 = NULL;
    void *fb1 = NULL;
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(panel, 2, &fb0, &fb1));

    const uint32_t pixel_count = LVGL_PORT_H_RES * LVGL_PORT_V_RES;
    lv_disp_draw_buf_init(&draw_buf, fb0, fb1, pixel_count);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LVGL_PORT_H_RES;
    disp_drv.ver_res = LVGL_PORT_V_RES;
    disp_drv.flush_cb = flush_cb;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.user_data = panel;
    disp_drv.full_refresh = 1;

    return lv_disp_drv_register(&disp_drv);
}

static void touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    esp_lcd_touch_handle_t touch = (esp_lcd_touch_handle_t)drv->user_data;
    uint16_t x = 0;
    uint16_t y = 0;
    uint8_t points = 0;

    esp_lcd_touch_read_data(touch);
    const bool pressed = esp_lcd_touch_get_coordinates(touch, &x, &y, NULL, &points, 1);

    if (pressed && points > 0) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = x;
        data->point.y = y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static lv_indev_t *touch_init(esp_lcd_touch_handle_t touch)
{
    static lv_indev_drv_t indev_drv;

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read_cb;
    indev_drv.user_data = touch;

    return lv_indev_drv_register(&indev_drv);
}

static void lvgl_task(void *arg)
{
    (void)arg;

    while (true) {
        uint32_t delay_ms = LVGL_PORT_TASK_MAX_DELAY_MS;

        if (lvgl_port_lock(-1)) {
            delay_ms = lv_timer_handler();
            lvgl_port_unlock();
        }

        if (delay_ms < LVGL_PORT_TASK_MIN_DELAY_MS) {
            delay_ms = LVGL_PORT_TASK_MIN_DELAY_MS;
        }
        if (delay_ms > LVGL_PORT_TASK_MAX_DELAY_MS) {
            delay_ms = LVGL_PORT_TASK_MAX_DELAY_MS;
        }

        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

esp_err_t lvgl_port_init(esp_lcd_panel_handle_t lcd_handle, esp_lcd_touch_handle_t tp_handle)
{
    assert(lcd_handle);

    lv_init();
    ESP_RETURN_ON_ERROR(tick_init(), TAG, "LVGL tick init failed");

    lv_disp_t *display = display_init(lcd_handle);
    assert(display);

    if (tp_handle) {
        lv_indev_t *input = touch_init(tp_handle);
        assert(input);
    }

    s_lvgl_mutex = xSemaphoreCreateRecursiveMutex();
    if (!s_lvgl_mutex) {
        ESP_LOGE(TAG, "LVGL mutex allocation failed");
        return ESP_ERR_NO_MEM;
    }

    const BaseType_t core = LVGL_PORT_TASK_CORE < 0 ? tskNO_AFFINITY : LVGL_PORT_TASK_CORE;
    const BaseType_t result = xTaskCreatePinnedToCore(
        lvgl_task,
        "lvgl",
        LVGL_PORT_TASK_STACK_SIZE,
        NULL,
        LVGL_PORT_TASK_PRIORITY,
        &s_lvgl_task,
        core
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "LVGL task creation failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "LVGL ready: two full frame buffers, non-blocking flush");
    return ESP_OK;
}

bool lvgl_port_lock(int timeout_ms)
{
    assert(s_lvgl_mutex);
    const TickType_t ticks = timeout_ms < 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(s_lvgl_mutex, ticks) == pdTRUE;
}

void lvgl_port_unlock(void)
{
    assert(s_lvgl_mutex);
    xSemaphoreGiveRecursive(s_lvgl_mutex);
}

bool lvgl_port_notify_rgb_vsync(void)
{
    /* Frame switching is handled by the RGB panel driver. No task notification
     * is required by this non-blocking port. */
    return false;
}
