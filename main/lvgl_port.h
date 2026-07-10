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

#define ESP_PANEL_USE_1024_600_LCD           (0)     // 0: 800x480, 1: 1024x600
#define CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_GT911 1 // 1 initiates the touch, 0 closes the touch.

/**
 * LVGL related parameters, can be adjusted by users
 *
 */
#if ESP_PANEL_USE_1024_600_LCD
    #define LVGL_PORT_H_RES             (1024)
    #define LVGL_PORT_V_RES             (600)
#else
    #define LVGL_PORT_H_RES             (800)
    #define LVGL_PORT_V_RES             (480)
#endif
#define LVGL_PORT_TICK_PERIOD_MS    (CONFIG_EXAMPLE_LVGL_PORT_TICK)

/**
 * LVGL timer handle task related parameters, can be adjusted by users
 *
 */
#define LVGL_PORT_TASK_MAX_DELAY_MS (CONFIG_EXAMPLE_LVGL_PORT_TASK_MAX_DELAY_MS)
#define LVGL_PORT_TASK_MIN_DELAY_MS (CONFIG_EXAMPLE_LVGL_PORT_TASK_MIN_DELAY_MS)
#define LVGL_PORT_TASK_STACK_SIZE   (CONFIG_EXAMPLE_LVGL_PORT_TASK_STACK_SIZE_KB * 1024)
#define LVGL_PORT_TASK_PRIORITY     (CONFIG_EXAMPLE_LVGL_PORT_TASK_PRIORITY)
#define LVGL_PORT_TASK_CORE         (CONFIG_EXAMPLE_LVGL_PORT_TASK_CORE)

/**
 * LVGL buffer related parameters, can be adjusted by users.
 * These parameters are unused when tearing prevention is enabled.
 */
#if CONFIG_EXAMPLE_LVGL_PORT_BUF_PSRAM
#define LVGL_PORT_BUFFER_MALLOC_CAPS    (MALLOC_CAP_SPIRAM)
#elif CONFIG_EXAMPLE_LVGL_PORT_BUF_INTERNAL
#define LVGL_PORT_BUFFER_MALLOC_CAPS    (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
#endif
#define LVGL_PORT_BUFFER_HEIGHT         (CONFIG_EXAMPLE_LVGL_PORT_BUF_HEIGHT)

/**
 * SmartTank HMI uses three complete RGB frame buffers and LVGL full-refresh
 * mode. Page changes replace nearly the whole 800x340 content area. Rendering
 * a complete frame off-screen and swapping a finished buffer is more stable
 * here than direct-mode dirty-area copying.
 */
#define LVGL_PORT_AVOID_TEAR_ENABLE       (1)
#define LVGL_PORT_AVOID_TEAR_MODE         (2)
#define EXAMPLE_LVGL_PORT_ROTATION_DEGREE (0)

#if LVGL_PORT_AVOID_TEAR_ENABLE

#if LVGL_PORT_AVOID_TEAR_MODE == 1
#define LVGL_PORT_LCD_RGB_BUFFER_NUMS   (2)
#define LVGL_PORT_FULL_REFRESH          (1)
#define LVGL_PORT_DIRECT_MODE           (0)
#elif LVGL_PORT_AVOID_TEAR_MODE == 2
#define LVGL_PORT_LCD_RGB_BUFFER_NUMS   (3)
#define LVGL_PORT_FULL_REFRESH          (1)
#define LVGL_PORT_DIRECT_MODE           (0)
#elif LVGL_PORT_AVOID_TEAR_MODE == 3
#define LVGL_PORT_LCD_RGB_BUFFER_NUMS   (2)
#define LVGL_PORT_FULL_REFRESH          (0)
#define LVGL_PORT_DIRECT_MODE           (1)
#else
#error "Unsupported LVGL_PORT_AVOID_TEAR_MODE"
#endif

#if EXAMPLE_LVGL_PORT_ROTATION_DEGREE == 0
#define EXAMPLE_LVGL_PORT_ROTATION_0    (1)
#define EXAMPLE_LVGL_PORT_ROTATION_90   (0)
#define EXAMPLE_LVGL_PORT_ROTATION_180  (0)
#define EXAMPLE_LVGL_PORT_ROTATION_270  (0)
#elif EXAMPLE_LVGL_PORT_ROTATION_DEGREE == 90
#define EXAMPLE_LVGL_PORT_ROTATION_0    (0)
#define EXAMPLE_LVGL_PORT_ROTATION_90   (1)
#define EXAMPLE_LVGL_PORT_ROTATION_180  (0)
#define EXAMPLE_LVGL_PORT_ROTATION_270  (0)
#elif EXAMPLE_LVGL_PORT_ROTATION_DEGREE == 180
#define EXAMPLE_LVGL_PORT_ROTATION_0    (0)
#define EXAMPLE_LVGL_PORT_ROTATION_90   (0)
#define EXAMPLE_LVGL_PORT_ROTATION_180  (1)
#define EXAMPLE_LVGL_PORT_ROTATION_270  (0)
#elif EXAMPLE_LVGL_PORT_ROTATION_DEGREE == 270
#define EXAMPLE_LVGL_PORT_ROTATION_0    (0)
#define EXAMPLE_LVGL_PORT_ROTATION_90   (0)
#define EXAMPLE_LVGL_PORT_ROTATION_180  (0)
#define EXAMPLE_LVGL_PORT_ROTATION_270  (1)
#else
#error "Unsupported EXAMPLE_LVGL_PORT_ROTATION_DEGREE"
#endif

#if EXAMPLE_LVGL_PORT_ROTATION_DEGREE != 0
#undef LVGL_PORT_LCD_RGB_BUFFER_NUMS
#define LVGL_PORT_LCD_RGB_BUFFER_NUMS   (3)
#endif

#else
#define LVGL_PORT_LCD_RGB_BUFFER_NUMS   (1)
#define LVGL_PORT_FULL_REFRESH          (0)
#define LVGL_PORT_DIRECT_MODE           (0)
#define EXAMPLE_LVGL_PORT_ROTATION_0    (1)
#define EXAMPLE_LVGL_PORT_ROTATION_90   (0)
#define EXAMPLE_LVGL_PORT_ROTATION_180  (0)
#define EXAMPLE_LVGL_PORT_ROTATION_270  (0)
#endif

/**
 * @brief Initialize LVGL port
 *
 * @param[in] lcd_handle: LCD panel handle
 * @param[in] tp_handle: Touch panel handle
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Invalid argument
 *      - Others: Fail
 */
esp_err_t lvgl_port_init(esp_lcd_panel_handle_t lcd_handle, esp_lcd_touch_handle_t tp_handle);

/**
 * @brief Take LVGL mutex
 *
 * @param[in] timeout_ms: Timeout in [ms]. 0 will block indefinitely.
 *
 * @return
 *      - true:  Mutex was taken
 *      - false: Mutex was NOT taken
 */
bool lvgl_port_lock(int timeout_ms);

/**
 * @brief Give LVGL mutex
 */
void lvgl_port_unlock(void);

/**
 * @brief Notifies the LVGL task when the transmission of the RGB frame buffer is completed.
 *
 * @return
 *      - true:  The tasks need to be re-scheduled
 *      - false: The tasks don't need to be re-scheduled
 */
bool lvgl_port_notify_rgb_vsync(void);

#ifdef __cplusplus
}
#endif
