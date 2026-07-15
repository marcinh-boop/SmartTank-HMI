/*
 * Moduł screen_service.c należy do warstwy głównej programu SmartTank.
 * Realizuje logikę modułu i ukrywa jej szczegóły za publicznym interfejsem.
 * Oddzielenie tej odpowiedzialności ułatwia diagnostykę, testy i dalszą rozbudowę.
 */
#include "screen_service.h"

#include <stdio.h>

#include "analog_module_service.h"
#include "clock_service.h"
#include "modbus_rtu_client.h"
#include "ntp_service.h"
#include "rs485_port.h"
#include "theme.h"
#include "wifi_service.h"

#define SERVICE_BLUE    lv_color_hex(0x2EA8FF)
#define SERVICE_GREEN   lv_color_hex(0x39D12F)
#define SERVICE_YELLOW  lv_color_hex(0xFFC247)
#define SERVICE_RED     lv_color_hex(0xFF3333)
#define SERVICE_PANEL   lv_color_hex(0x0B1825)
#define SERVICE_BORDER  lv_color_hex(0x24384A)

#define SERVICE_FRAME_PREVIEW_BYTES 8U
#define SERVICE_PAGE_COUNT 2U
#define SERVICE_SENSOR_MIN_MA 3.5F
#define SERVICE_SENSOR_MAX_MA 22.0F
#define SERVICE_SENSOR_ZERO_MA 4.0F
#define SERVICE_SENSOR_SPAN_MA 16.0F
#define SERVICE_SENSOR_MIN_MM 200.0F
#define SERVICE_SENSOR_MAX_MM 2000.0F

static lv_obj_t *s_root;
static lv_timer_t *s_refresh_timer;
static lv_obj_t *s_diagnostics_page;
static lv_obj_t *s_channels_page;
static lv_obj_t *s_page_label;
static uint8_t s_current_page;

static lv_obj_t *s_rs485_status;
static lv_obj_t *s_baud_value;
static lv_obj_t *s_hardware_value;

static lv_obj_t *s_modbus_status;
static lv_obj_t *s_crc_status;
static lv_obj_t *s_requests_value;
static lv_obj_t *s_responses_value;
static lv_obj_t *s_timeouts_value;
static lv_obj_t *s_crc_errors_value;
static lv_obj_t *s_last_error_value;
static lv_obj_t *s_last_tx_value;
static lv_obj_t *s_last_rx_value;

static lv_obj_t *s_module_state_value;
static lv_obj_t *s_driver_test_value;
static lv_obj_t *s_slave_baud_value;
static lv_obj_t *s_ai1_raw_value;
static lv_obj_t *s_ai1_ma_value;
static lv_obj_t *s_ai2_raw_value;
static lv_obj_t *s_rtc_value;
static lv_obj_t *s_wifi_value;
static lv_obj_t *s_ntp_value;

static lv_obj_t *s_channel_led[WAVESHARE_ANALOG_8CH_CHANNEL_COUNT];
static lv_obj_t *s_channel_state[WAVESHARE_ANALOG_8CH_CHANNEL_COUNT];
static lv_obj_t *s_channel_current[WAVESHARE_ANALOG_8CH_CHANNEL_COUNT];
static lv_obj_t *s_channel_raw[WAVESHARE_ANALOG_8CH_CHANNEL_COUNT];
static lv_obj_t *s_channel_mode[WAVESHARE_ANALOG_8CH_CHANNEL_COUNT];
static lv_obj_t *s_channel_summary;

static lv_obj_t *create_label(
    lv_obj_t *parent,
    const char *text,
    lv_color_t color,
    lv_align_t align,
    int x,
    int y,
    const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_align(label, align, x, y);
    return label;
}

static lv_obj_t *create_panel(lv_obj_t *parent, int x, int width)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, width, 320);
    lv_obj_align(panel, LV_ALIGN_TOP_LEFT, x, 68);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(panel, SERVICE_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, SERVICE_BORDER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 12, LV_PART_MAIN);
    return panel;
}

static void show_page(uint8_t page)
{
    if (page >= SERVICE_PAGE_COUNT || s_diagnostics_page == NULL ||
        s_channels_page == NULL) {
        return;
    }

    s_current_page = page;
    if (page == 0U) {
        lv_obj_clear_flag(s_diagnostics_page, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_channels_page, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_page_label, "1 / 2");
    } else {
        lv_obj_add_flag(s_diagnostics_page, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_channels_page, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_page_label, "2 / 2");
    }
}

static void previous_page_cb(lv_event_t *event)
{
    (void)event;
    show_page(s_current_page == 0U ? SERVICE_PAGE_COUNT - 1U : s_current_page - 1U);
}

static void next_page_cb(lv_event_t *event)
{
    (void)event;
    show_page((uint8_t)((s_current_page + 1U) % SERVICE_PAGE_COUNT));
}

static void page_gesture_cb(lv_event_t *event)
{
    lv_indev_t *indev = lv_indev_get_act();
    if (indev == NULL) {
        return;
    }

    const lv_dir_t direction = lv_indev_get_gesture_dir(indev);
    if (direction == LV_DIR_LEFT) {
        next_page_cb(event);
    } else if (direction == LV_DIR_RIGHT) {
        previous_page_cb(event);
    }
}

static lv_obj_t *create_panel_nav_button(
    lv_obj_t *parent,
    const char *text,
    lv_event_cb_t callback)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_size(button, 88, 34);
    lv_obj_align(button, LV_ALIGN_TOP_RIGHT, 0, -7);
    lv_obj_set_style_bg_color(button, SERVICE_BLUE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(button, 8, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(button, ST_COLOR_TEXT, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x147FC4), LV_STATE_PRESSED);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, NULL);

    create_label(
        button,
        text,
        ST_COLOR_TEXT,
        LV_ALIGN_CENTER,
        0,
        0,
        &lv_font_montserrat_12
    );
    return button;
}

static lv_obj_t *create_value_row(
    lv_obj_t *parent,
    const char *name,
    const char *value,
    int y)
{
    create_label(
        parent,
        name,
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_TOP_LEFT,
        0,
        y,
        &lv_font_montserrat_12
    );

    lv_obj_t *label = create_label(
        parent,
        value,
        ST_COLOR_TEXT,
        LV_ALIGN_TOP_RIGHT,
        0,
        y,
        &lv_font_montserrat_12
    );
    lv_obj_set_width(label, 135);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    return label;
}

static void set_u32(lv_obj_t *label, uint32_t value)
{
    char buffer[24];
    snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)value);
    lv_label_set_text(label, buffer);
}

static void format_frame_preview(
    const uint8_t *frame,
    size_t frame_len,
    char *buffer,
    size_t buffer_size)
{
    if (buffer == NULL || buffer_size == 0U) {
        return;
    }

    if (frame == NULL || frame_len == 0U) {
        snprintf(buffer, buffer_size, "--");
        return;
    }

    const size_t shown = frame_len < SERVICE_FRAME_PREVIEW_BYTES
        ? frame_len
        : SERVICE_FRAME_PREVIEW_BYTES;
    size_t offset = 0U;

    for (size_t index = 0U; index < shown; index++) {
        const int written = snprintf(
            buffer + offset,
            buffer_size - offset,
            index == 0U ? "%02X" : " %02X",
            frame[index]
        );
        if (written < 0 || (size_t)written >= buffer_size - offset) {
            break;
        }
        offset += (size_t)written;
    }

    if (frame_len > shown && offset + 4U < buffer_size) {
        snprintf(buffer + offset, buffer_size - offset, " ...");
    }
}

static void refresh_rs485_status(
    const analog_module_service_snapshot_t *module)
{
    const bool ready = rs485_port_is_initialized();

    lv_label_set_text(
        s_rs485_status,
        ready ? "AKTYWNY" : "GOTOWY / OFF"
    );
    lv_obj_set_style_text_color(
        s_rs485_status,
        ready ? SERVICE_GREEN : SERVICE_YELLOW,
        LV_PART_MAIN
    );

    char buffer[32];
    snprintf(
        buffer,
        sizeof(buffer),
        "%lu 8N1",
        (unsigned long)module->baud_rate
    );
    lv_label_set_text(s_baud_value, buffer);

    lv_label_set_text(
        s_hardware_value,
        module->hardware_enabled ? "WLACZONY" : "WYLACZONY"
    );
    lv_obj_set_style_text_color(
        s_hardware_value,
        module->hardware_enabled ? SERVICE_GREEN : SERVICE_YELLOW,
        LV_PART_MAIN
    );
}

static void refresh_modbus_status(void)
{
    const bool ready = modbus_rtu_client_is_initialized();
    lv_label_set_text(
        s_modbus_status,
        ready ? "AKTYWNY" : "TESTY LOKALNE"
    );
    lv_obj_set_style_text_color(
        s_modbus_status,
        ready ? SERVICE_GREEN : SERVICE_BLUE,
        LV_PART_MAIN
    );

    static const uint8_t crc_test_frame[] = {
        0x01U, 0x04U, 0x00U, 0x00U, 0x00U, 0x08U
    };
    const bool crc_ok =
        modbus_rtu_crc16(crc_test_frame, sizeof(crc_test_frame)) == 0xCCF1U;

    lv_label_set_text(s_crc_status, crc_ok ? "OK" : "BLAD");
    lv_obj_set_style_text_color(
        s_crc_status,
        crc_ok ? SERVICE_GREEN : SERVICE_RED,
        LV_PART_MAIN
    );

    modbus_rtu_diagnostics_t diagnostics;
    modbus_rtu_get_diagnostics(&diagnostics);

    set_u32(s_requests_value, diagnostics.requests);
    set_u32(s_responses_value, diagnostics.responses);
    set_u32(s_timeouts_value, diagnostics.timeouts);
    set_u32(s_crc_errors_value, diagnostics.crc_errors);

    if (diagnostics.last_error == ESP_OK) {
        lv_label_set_text(s_last_error_value, "OK");
    } else {
        char buffer[24];
        snprintf(buffer, sizeof(buffer), "0x%X", (unsigned int)diagnostics.last_error);
        lv_label_set_text(s_last_error_value, buffer);
    }

    char frame_buffer[48];
    format_frame_preview(
        diagnostics.last_request,
        diagnostics.last_request_len,
        frame_buffer,
        sizeof(frame_buffer)
    );
    lv_label_set_text(s_last_tx_value, frame_buffer);

    format_frame_preview(
        diagnostics.last_response,
        diagnostics.last_response_len,
        frame_buffer,
        sizeof(frame_buffer)
    );
    lv_label_set_text(s_last_rx_value, frame_buffer);
}

static void refresh_module_status(
    const analog_module_service_snapshot_t *module)
{
    lv_label_set_text(
        s_module_state_value,
        analog_module_service_state_name(module->state)
    );

    lv_color_t state_color = SERVICE_YELLOW;
    if (module->state == ANALOG_MODULE_STATE_ONLINE) {
        state_color = SERVICE_GREEN;
    } else if (module->state == ANALOG_MODULE_STATE_ERROR) {
        state_color = SERVICE_RED;
    } else if (module->state == ANALOG_MODULE_STATE_STARTING) {
        state_color = SERVICE_BLUE;
    }
    lv_obj_set_style_text_color(
        s_module_state_value,
        state_color,
        LV_PART_MAIN
    );

    lv_label_set_text(
        s_driver_test_value,
        module->self_test_ok ? "OK" : "BLAD"
    );
    lv_obj_set_style_text_color(
        s_driver_test_value,
        module->self_test_ok ? SERVICE_GREEN : SERVICE_RED,
        LV_PART_MAIN
    );

    char buffer[48];
    snprintf(
        buffer,
        sizeof(buffer),
        "%u / %lu",
        (unsigned int)module->slave_address,
        (unsigned long)module->baud_rate
    );
    lv_label_set_text(s_slave_baud_value, buffer);

    if (!module->module.inputs_valid) {
        lv_label_set_text(s_ai1_raw_value, "--");
        lv_label_set_text(s_ai1_ma_value, "--");
        lv_label_set_text(s_ai2_raw_value, "--");
        return;
    }

    snprintf(
        buffer,
        sizeof(buffer),
        "%u uA",
        (unsigned int)module->module.input_raw_ua[0]
    );
    lv_label_set_text(s_ai1_raw_value, buffer);

    snprintf(
        buffer,
        sizeof(buffer),
        "%.3f mA",
        module->module.input_ma[0]
    );
    lv_label_set_text(s_ai1_ma_value, buffer);

    snprintf(
        buffer,
        sizeof(buffer),
        "%u uA",
        (unsigned int)module->module.input_raw_ua[1]
    );
    lv_label_set_text(s_ai2_raw_value, buffer);
}

static void refresh_channel_status(
    const analog_module_service_snapshot_t *module)
{
    const bool data_ok = module->online && module->module.inputs_valid;

    for (uint8_t index = 0U;
         index < WAVESHARE_ANALOG_8CH_CHANNEL_COUNT;
         index++) {
        const float current_ma = module->module.input_ma[index];
        const bool sensor_present = data_ok &&
            current_ma >= SERVICE_SENSOR_MIN_MA &&
            current_ma <= SERVICE_SENSOR_MAX_MA;
        const bool over_range = data_ok &&
            current_ma > SERVICE_SENSOR_MAX_MA;

        lv_obj_set_style_bg_color(
            s_channel_led[index],
            sensor_present ? SERVICE_GREEN : SERVICE_RED,
            LV_PART_MAIN
        );
        lv_obj_set_style_shadow_color(
            s_channel_led[index],
            sensor_present ? SERVICE_GREEN : SERVICE_RED,
            LV_PART_MAIN
        );
        lv_label_set_text(
            s_channel_state[index],
            sensor_present ? "CZUJNIK" : (over_range ? "BLAD" : "BRAK")
        );
        lv_obj_set_style_text_color(
            s_channel_state[index],
            sensor_present ? SERVICE_GREEN : SERVICE_RED,
            LV_PART_MAIN
        );

        if (!data_ok) {
            lv_label_set_text(s_channel_current[index], "--");
            lv_label_set_text(s_channel_raw[index], "--");
        } else {
            char buffer[24];
            const float distance_mm = SERVICE_SENSOR_MIN_MM +
                ((current_ma - SERVICE_SENSOR_ZERO_MA) / SERVICE_SENSOR_SPAN_MA) *
                (SERVICE_SENSOR_MAX_MM - SERVICE_SENSOR_MIN_MM);

            if (!sensor_present) {
                snprintf(buffer, sizeof(buffer), "--");
            } else if (current_ma < SERVICE_SENSOR_ZERO_MA) {
                snprintf(buffer, sizeof(buffer), "< 200 mm");
            } else if (current_ma > 20.0F) {
                snprintf(buffer, sizeof(buffer), "> 2000 mm");
            } else {
                snprintf(buffer, sizeof(buffer), "%.0f mm", distance_mm);
            }
            lv_label_set_text(s_channel_current[index], buffer);
            snprintf(
                buffer,
                sizeof(buffer),
                "%.3f mA",
                current_ma
            );
            lv_label_set_text(s_channel_raw[index], buffer);
        }

        if (!module->module.modes_valid) {
            lv_label_set_text(s_channel_mode[index], "TRYB --");
        } else if (module->module.channel_mode[index] == WAVESHARE_ANALOG_MODE_4_20_MA) {
            lv_label_set_text(s_channel_mode[index], "4-20 mA");
        } else {
            char buffer[20];
            snprintf(
                buffer,
                sizeof(buffer),
                "TRYB %u",
                (unsigned int)module->module.channel_mode[index]
            );
            lv_label_set_text(s_channel_mode[index], buffer);
        }
    }

    char summary[96];
    snprintf(
        summary,
        sizeof(summary),
        "%s  |  Odczyty OK: %lu  |  Bledy: %lu",
        data_ok ? "MODUL ONLINE" : "BRAK DANYCH Z MODULU",
        (unsigned long)module->successful_polls,
        (unsigned long)module->failed_polls
    );
    lv_label_set_text(s_channel_summary, summary);
    lv_obj_set_style_text_color(
        s_channel_summary,
        data_ok ? SERVICE_GREEN : SERVICE_RED,
        LV_PART_MAIN
    );
}

static void refresh_rtc_status(void)
{
    clock_service_snapshot_t clock;
    clock_service_get_snapshot(&clock);

    if (!clock.started) {
        lv_label_set_text(s_rtc_value, "START");
        lv_obj_set_style_text_color(s_rtc_value, SERVICE_YELLOW, LV_PART_MAIN);
        return;
    }

    if (!clock.rtc_present) {
        lv_label_set_text(s_rtc_value, "BLAD I2C");
        lv_obj_set_style_text_color(s_rtc_value, SERVICE_RED, LV_PART_MAIN);
        return;
    }

    if (!clock.time_valid) {
        lv_label_set_text(s_rtc_value, "CZAS NIEWAZNY");
        lv_obj_set_style_text_color(s_rtc_value, SERVICE_YELLOW, LV_PART_MAIN);
        return;
    }

    char buffer[32];
    snprintf(
        buffer,
        sizeof(buffer),
        "OK %02u:%02u",
        (unsigned int)clock.datetime.hour,
        (unsigned int)clock.datetime.minute
    );
    lv_label_set_text(s_rtc_value, buffer);
    lv_obj_set_style_text_color(s_rtc_value, SERVICE_GREEN, LV_PART_MAIN);
}

static void refresh_wifi_status(void)
{
    wifi_service_snapshot_t wifi;
    wifi_service_get_snapshot(&wifi);

    if (!wifi.started || !wifi.radio_ready || wifi.last_error != ESP_OK) {
        lv_label_set_text(s_wifi_value, "BLAD");
        lv_obj_set_style_text_color(s_wifi_value, SERVICE_RED, LV_PART_MAIN);
    } else if (wifi.connected) {
        lv_label_set_text(s_wifi_value, "ONLINE");
        lv_obj_set_style_text_color(s_wifi_value, SERVICE_GREEN, LV_PART_MAIN);
    } else if (wifi.scanning) {
        lv_label_set_text(s_wifi_value, "SKANOWANIE");
        lv_obj_set_style_text_color(s_wifi_value, SERVICE_BLUE, LV_PART_MAIN);
    } else {
        lv_label_set_text(s_wifi_value, "RADIO OK");
        lv_obj_set_style_text_color(s_wifi_value, SERVICE_YELLOW, LV_PART_MAIN);
    }
}

static void refresh_ntp_status(void)
{
    ntp_service_snapshot_t ntp;
    ntp_service_get_snapshot(&ntp);

    if (!ntp.started) {
        lv_label_set_text(s_ntp_value, "OFF");
        lv_obj_set_style_text_color(s_ntp_value, SERVICE_YELLOW, LV_PART_MAIN);
    } else if (ntp.last_error != ESP_OK) {
        lv_label_set_text(s_ntp_value, "BLAD");
        lv_obj_set_style_text_color(s_ntp_value, SERVICE_RED, LV_PART_MAIN);
    } else if (ntp.synchronized) {
        lv_label_set_text(s_ntp_value, ntp.rtc_updated ? "OK" : "OK / RTC ERR");
        lv_obj_set_style_text_color(
            s_ntp_value,
            ntp.rtc_updated ? SERVICE_GREEN : SERVICE_YELLOW,
            LV_PART_MAIN
        );
    } else if (ntp.waiting_for_wifi) {
        lv_label_set_text(s_ntp_value, "CZEKA WIFI");
        lv_obj_set_style_text_color(s_ntp_value, SERVICE_YELLOW, LV_PART_MAIN);
    } else {
        lv_label_set_text(s_ntp_value, "SYNCHRONIZACJA");
        lv_obj_set_style_text_color(s_ntp_value, SERVICE_BLUE, LV_PART_MAIN);
    }
}

static void refresh_service(void)
{
    if (s_root == NULL) {
        return;
    }

    analog_module_service_snapshot_t module;
    analog_module_service_get_snapshot(&module);

    refresh_rs485_status(&module);
    refresh_modbus_status();
    refresh_module_status(&module);
    refresh_channel_status(&module);
    refresh_rtc_status();
    refresh_wifi_status();
    refresh_ntp_status();
}

static void refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (s_root != NULL && !lv_obj_has_flag(s_root, LV_OBJ_FLAG_HIDDEN)) {
        refresh_service();
    }
}

static void build_screen(lv_obj_t *parent_screen)
{
    s_root = lv_obj_create(parent_screen);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, 800, 398);
    lv_obj_align(s_root, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_root, ST_COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *top = lv_obj_create(s_root);
    lv_obj_set_size(top, 760, 50);
    lv_obj_align(top, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_clear_flag(top, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(top, lv_color_hex(0x08131F), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(top, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(top, 10, LV_PART_MAIN);
    lv_obj_set_style_border_width(top, 0, LV_PART_MAIN);

    create_label(
        top,
        "SmartTank HMI",
        ST_COLOR_TEXT,
        LV_ALIGN_LEFT_MID,
        12,
        0,
        &lv_font_montserrat_14
    );
    create_label(
        top,
        "SERWIS",
        ST_COLOR_ACCENT,
        LV_ALIGN_CENTER,
        0,
        -9,
        &lv_font_montserrat_14
    );
    s_page_label = create_label(
        top,
        "1 / 2",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_CENTER,
        0,
        17,
        &lv_font_montserrat_12
    );
    create_label(
        top,
        "RS485 / MODBUS / 8CH",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_RIGHT_MID,
        -12,
        0,
        &lv_font_montserrat_14
    );

    s_diagnostics_page = lv_obj_create(s_root);
    lv_obj_remove_style_all(s_diagnostics_page);
    lv_obj_set_size(s_diagnostics_page, 800, 330);
    lv_obj_align(s_diagnostics_page, LV_ALIGN_TOP_LEFT, 0, 60);
    lv_obj_clear_flag(s_diagnostics_page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_diagnostics_page, page_gesture_cb, LV_EVENT_GESTURE, NULL);

    lv_obj_t *rs485_panel = create_panel(s_diagnostics_page, 20, 245);
    lv_obj_set_y(rs485_panel, 8);
    create_label(
        rs485_panel,
        "PORT RS485",
        SERVICE_BLUE,
        LV_ALIGN_TOP_LEFT,
        0,
        0,
        &lv_font_montserrat_14
    );
    s_rs485_status = create_value_row(rs485_panel, "Stan", "--", 34);
    create_value_row(rs485_panel, "UART", "UART1", 66);
    create_value_row(rs485_panel, "TX", "GPIO44", 98);
    create_value_row(rs485_panel, "RX", "GPIO43", 130);
    create_value_row(rs485_panel, "Kierunek", "AUTO", 162);
    create_value_row(rs485_panel, "Transceiver", "SP3485", 194);
    s_baud_value = create_value_row(rs485_panel, "Parametry", "9600 8N1", 226);
    s_hardware_value = create_value_row(rs485_panel, "Odczyt HW", "--", 258);

    lv_obj_t *modbus_panel = create_panel(s_diagnostics_page, 278, 245);
    lv_obj_set_y(modbus_panel, 8);
    create_label(
        modbus_panel,
        "MODBUS RTU",
        SERVICE_GREEN,
        LV_ALIGN_TOP_LEFT,
        0,
        0,
        &lv_font_montserrat_14
    );
    s_modbus_status = create_value_row(modbus_panel, "Klient", "--", 34);
    s_crc_status = create_value_row(modbus_panel, "CRC ramek", "--", 66);
    create_value_row(modbus_panel, "Funkcje", "03 / 04 / 06", 98);
    s_requests_value = create_value_row(modbus_panel, "Zapytania", "0", 130);
    s_responses_value = create_value_row(modbus_panel, "Odpowiedzi", "0", 162);
    s_timeouts_value = create_value_row(modbus_panel, "Timeouty", "0", 194);
    s_crc_errors_value = create_value_row(modbus_panel, "Bledy CRC", "0", 226);
    s_last_error_value = create_value_row(modbus_panel, "Ostatni blad", "OK", 258);
    s_last_tx_value = create_value_row(modbus_panel, "TX", "--", 274);
    s_last_rx_value = create_value_row(modbus_panel, "RX", "--", 290);

    lv_obj_t *module_panel = create_panel(s_diagnostics_page, 536, 244);
    lv_obj_set_y(module_panel, 8);
    create_label(
        module_panel,
        "WAVESHARE 8CH",
        SERVICE_YELLOW,
        LV_ALIGN_TOP_LEFT,
        0,
        0,
        &lv_font_montserrat_14
    );
    create_panel_nav_button(module_panel, "8CH >>", next_page_cb);
    s_module_state_value = create_value_row(module_panel, "Stan", "--", 34);
    s_driver_test_value = create_value_row(module_panel, "Test sterownika", "--", 66);
    s_slave_baud_value = create_value_row(module_panel, "Slave / baud", "1 / 9600", 98);
    s_ai1_raw_value = create_value_row(module_panel, "AI1 surowe", "--", 130);
    s_ai1_ma_value = create_value_row(module_panel, "AI1 prad", "--", 162);
    s_ai2_raw_value = create_value_row(module_panel, "AI2 surowe", "--", 194);
    s_rtc_value = create_value_row(module_panel, "RTC", "--", 226);
    s_wifi_value = create_value_row(module_panel, "Wi-Fi", "--", 258);
    s_ntp_value = create_value_row(module_panel, "NTP", "--", 290);

    s_channels_page = lv_obj_create(s_root);
    lv_obj_remove_style_all(s_channels_page);
    lv_obj_set_size(s_channels_page, 800, 330);
    lv_obj_align(s_channels_page, LV_ALIGN_TOP_LEFT, 0, 60);
    lv_obj_clear_flag(s_channels_page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_channels_page, page_gesture_cb, LV_EVENT_GESTURE, NULL);

    lv_obj_t *channels_panel = create_panel(s_channels_page, 20, 760);
    lv_obj_set_y(channels_panel, 8);
    create_label(
        channels_panel,
        "WEJSCIA ANALOGOWE 8CH",
        SERVICE_YELLOW,
        LV_ALIGN_TOP_LEFT,
        0,
        0,
        &lv_font_montserrat_14
    );
    create_panel_nav_button(channels_panel, "< WROC", previous_page_cb);
    create_label(
        channels_panel,
        "KONTROLKA",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_TOP_LEFT,
        124,
        2,
        &lv_font_montserrat_12
    );
    create_label(
        channels_panel,
        "ODLEGLOSC",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_TOP_LEFT,
        254,
        2,
        &lv_font_montserrat_12
    );
    create_label(
        channels_panel,
        "PRAD",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_TOP_LEFT,
        404,
        2,
        &lv_font_montserrat_12
    );
    create_label(
        channels_panel,
        "TRYB",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_TOP_LEFT,
        548,
        2,
        &lv_font_montserrat_12
    );

    for (uint8_t index = 0U;
         index < WAVESHARE_ANALOG_8CH_CHANNEL_COUNT;
         index++) {
        const int row_y = 35 + (int)index * 31;
        char name[12];
        snprintf(name, sizeof(name), "PORT %u", (unsigned int)index + 1U);
        create_label(
            channels_panel,
            name,
            ST_COLOR_TEXT,
            LV_ALIGN_TOP_LEFT,
            0,
            row_y,
            &lv_font_montserrat_12
        );

        s_channel_led[index] = lv_obj_create(channels_panel);
        lv_obj_remove_style_all(s_channel_led[index]);
        lv_obj_set_size(s_channel_led[index], 16, 16);
        lv_obj_align(s_channel_led[index], LV_ALIGN_TOP_LEFT, 128, row_y - 1);
        lv_obj_set_style_radius(s_channel_led[index], LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_channel_led[index], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(s_channel_led[index], 8, LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(s_channel_led[index], LV_OPA_40, LV_PART_MAIN);

        s_channel_state[index] = create_label(
            channels_panel, "--", ST_COLOR_TEXT_DIM,
            LV_ALIGN_TOP_LEFT, 153, row_y, &lv_font_montserrat_12);
        s_channel_current[index] = create_label(
            channels_panel, "--", ST_COLOR_TEXT,
            LV_ALIGN_TOP_LEFT, 254, row_y, &lv_font_montserrat_12);
        s_channel_raw[index] = create_label(
            channels_panel, "--", ST_COLOR_TEXT_DIM,
            LV_ALIGN_TOP_LEFT, 404, row_y, &lv_font_montserrat_12);
        s_channel_mode[index] = create_label(
            channels_panel, "--", ST_COLOR_TEXT_DIM,
            LV_ALIGN_TOP_LEFT, 548, row_y, &lv_font_montserrat_12);
    }

    s_channel_summary = create_label(
        channels_panel,
        "OCZEKIWANIE NA DANE",
        SERVICE_YELLOW,
        LV_ALIGN_BOTTOM_MID,
        0,
        0,
        &lv_font_montserrat_12
    );

    show_page(0U);

    s_refresh_timer = lv_timer_create(refresh_timer_cb, 500, NULL);
    refresh_service();
}

void screen_service_open(lv_obj_t *parent_screen)
{
    if (parent_screen == NULL) {
        return;
    }

    if (s_root == NULL) {
        build_screen(parent_screen);
    }

    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_root);
    refresh_service();
}

void screen_service_hide(void)
{
    if (s_root != NULL) {
        lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    }
}
