/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "esp_lcd_types.h"
#include "esp_lcd_touch.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_PANEL_USE_1024_600_LCD           (0)
#define CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_GT911 1

#if ESP_PANEL_USE_1024_600_LCD
#define LVGL_PORT_H_RES             (1024)
#define LVGL_PORT_V_RES             (600)
#else
#define LVGL_PORT_H_RES             (800)
#define LVGL_PORT_V_RES             (480)
#endif

#define LVGL_PORT_TICK_PERIOD_MS    (CONFIG_EXAMPLE_LVGL_PORT_TICK)
#define LVGL_PORT_TASK_MAX_DELAY_MS (CONFIG_EXAMPLE_LVGL_PORT_TASK_MAX_DELAY_MS)
#define LVGL_PORT_TASK_MIN_DELAY_MS (CONFIG_EXAMPLE_LVGL_PORT_TASK_MIN_DELAY_MS)
#define LVGL_PORT_TASK_STACK_SIZE   (CONFIG_EXAMPLE_LVGL_PORT_TASK_STACK_SIZE_KB * 1024)
#define LVGL_PORT_TASK_PRIORITY     (CONFIG_EXAMPLE_LVGL_PORT_TASK_PRIORITY)
#define LVGL_PORT_TASK_CORE         (CONFIG_EXAMPLE_LVGL_PORT_TASK_CORE)

/*
 * Bandwidth-safe RGB mode:
 * - one panel frame buffer in PSRAM,
 * - two small bounce buffers in internal RAM,
 * - one 40-line LVGL draw buffer in internal RAM,
 * - partial refresh only.
 *
 * This avoids writing a complete 800x480 frame to PSRAM on every touch.
 */
#define LVGL_PORT_BUFFER_HEIGHT          (40)
#define LVGL_PORT_LCD_RGB_BUFFER_NUMS    (1)
#define LVGL_PORT_FULL_REFRESH           (0)
#define LVGL_PORT_DIRECT_MODE            (0)
#define LVGL_PORT_AVOID_TEAR_ENABLE      (0)
#define LVGL_PORT_AVOID_TEAR_MODE        (0)
#define EXAMPLE_LVGL_PORT_ROTATION_DEGREE (0)
#define EXAMPLE_LVGL_PORT_ROTATION_0      (1)
#define EXAMPLE_LVGL_PORT_ROTATION_90     (0)
#define EXAMPLE_LVGL_PORT_ROTATION_180    (0)
#define EXAMPLE_LVGL_PORT_ROTATION_270    (0)

esp_err_t lvgl_port_init(esp_lcd_panel_handle_t lcd_handle, esp_lcd_touch_handle_t tp_handle);
bool lvgl_port_lock(int timeout_ms);
void lvgl_port_unlock(void);
bool lvgl_port_notify_rgb_vsync(void);

#ifdef __cplusplus
}
#endif
