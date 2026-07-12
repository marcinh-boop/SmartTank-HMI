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

static lv_obj_t *s_root;
static lv_timer_t *s_refresh_timer;

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
        0,
        &lv_font_montserrat_14
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

    lv_obj_t *rs485_panel = create_panel(s_root, 20, 245);
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

    lv_obj_t *modbus_panel = create_panel(s_root, 278, 245);
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

    lv_obj_t *module_panel = create_panel(s_root, 536, 244);
    create_label(
        module_panel,
        "WAVESHARE 8CH",
        SERVICE_YELLOW,
        LV_ALIGN_TOP_LEFT,
        0,
        0,
        &lv_font_montserrat_14
    );
    s_module_state_value = create_value_row(module_panel, "Stan", "--", 34);
    s_driver_test_value = create_value_row(module_panel, "Test sterownika", "--", 66);
    s_slave_baud_value = create_value_row(module_panel, "Slave / baud", "1 / 9600", 98);
    s_ai1_raw_value = create_value_row(module_panel, "AI1 surowe", "--", 130);
    s_ai1_ma_value = create_value_row(module_panel, "AI1 prad", "--", 162);
    s_ai2_raw_value = create_value_row(module_panel, "AI2 surowe", "--", 194);
    s_rtc_value = create_value_row(module_panel, "RTC", "--", 226);
    s_wifi_value = create_value_row(module_panel, "Wi-Fi", "--", 258);
    s_ntp_value = create_value_row(module_panel, "NTP", "--", 290);

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
