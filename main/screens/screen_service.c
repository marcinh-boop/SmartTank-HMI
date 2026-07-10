#include "screen_service.h"

#include <stdio.h>

#include "app_model.h"
#include "clock_service.h"
#include "modbus_rtu_client.h"
#include "rs485_port.h"
#include "theme.h"
#include "wifi_service.h"

#define SERVICE_BLUE    lv_color_hex(0x2EA8FF)
#define SERVICE_GREEN   lv_color_hex(0x39D12F)
#define SERVICE_YELLOW  lv_color_hex(0xFFC247)
#define SERVICE_RED     lv_color_hex(0xFF3333)
#define SERVICE_PANEL   lv_color_hex(0x0B1825)
#define SERVICE_BORDER  lv_color_hex(0x24384A)

static lv_obj_t *s_root;
static lv_timer_t *s_refresh_timer;

static lv_obj_t *s_rs485_status;
static lv_obj_t *s_modbus_status;
static lv_obj_t *s_crc_status;
static lv_obj_t *s_requests_value;
static lv_obj_t *s_responses_value;
static lv_obj_t *s_timeouts_value;
static lv_obj_t *s_crc_errors_value;
static lv_obj_t *s_exceptions_value;
static lv_obj_t *s_last_error_value;
static lv_obj_t *s_source_value;
static lv_obj_t *s_uptime_value;
static lv_obj_t *s_module_value;
static lv_obj_t *s_rtc_status_value;
static lv_obj_t *s_rtc_time_value;
static lv_obj_t *s_wifi_value;

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

    return create_label(
        parent,
        value,
        ST_COLOR_TEXT,
        LV_ALIGN_TOP_RIGHT,
        0,
        y,
        &lv_font_montserrat_12
    );
}

static void set_u32(lv_obj_t *label, uint32_t value)
{
    char buffer[24];
    snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)value);
    lv_label_set_text(label, buffer);
}

static void refresh_rtc_status(void)
{
    clock_service_snapshot_t clock;
    clock_service_get_snapshot(&clock);

    if (!clock.started) {
        lv_label_set_text(s_rtc_status_value, "START");
        lv_obj_set_style_text_color(s_rtc_status_value, SERVICE_YELLOW, LV_PART_MAIN);
        lv_label_set_text(s_rtc_time_value, "--");
        return;
    }

    if (!clock.rtc_present) {
        lv_label_set_text(s_rtc_status_value, "BLAD I2C");
        lv_obj_set_style_text_color(s_rtc_status_value, SERVICE_RED, LV_PART_MAIN);
        lv_label_set_text(s_rtc_time_value, "--");
        return;
    }

    if (!clock.time_valid) {
        lv_label_set_text(
            s_rtc_status_value,
            clock.oscillator_stopped ? "CZAS NIEWAZNY" : "DO USTAWIENIA"
        );
        lv_obj_set_style_text_color(s_rtc_status_value, SERVICE_YELLOW, LV_PART_MAIN);
        lv_label_set_text(s_rtc_time_value, "--");
        return;
    }

    lv_label_set_text(s_rtc_status_value, "OK");
    lv_obj_set_style_text_color(s_rtc_status_value, SERVICE_GREEN, LV_PART_MAIN);

    char buffer[32];
    snprintf(
        buffer,
        sizeof(buffer),
        "%02u.%02u.%04u %02u:%02u",
        (unsigned int)clock.datetime.day,
        (unsigned int)clock.datetime.month,
        (unsigned int)clock.datetime.year,
        (unsigned int)clock.datetime.hour,
        (unsigned int)clock.datetime.minute
    );
    lv_label_set_text(s_rtc_time_value, buffer);
}

static void refresh_wifi_status(void)
{
    wifi_service_snapshot_t wifi;
    wifi_service_get_snapshot(&wifi);

    if (!wifi.started) {
        lv_label_set_text(s_wifi_value, "NIEURUCHOMIONE");
        lv_obj_set_style_text_color(s_wifi_value, SERVICE_YELLOW, LV_PART_MAIN);
        return;
    }

    if (!wifi.radio_ready || wifi.last_error != ESP_OK) {
        lv_label_set_text(s_wifi_value, "BLAD");
        lv_obj_set_style_text_color(s_wifi_value, SERVICE_RED, LV_PART_MAIN);
        return;
    }

    if (wifi.scanning) {
        lv_label_set_text(s_wifi_value, "SKANOWANIE");
        lv_obj_set_style_text_color(s_wifi_value, SERVICE_BLUE, LV_PART_MAIN);
        return;
    }

    if (wifi.connected) {
        lv_label_set_text(s_wifi_value, "ONLINE");
        lv_obj_set_style_text_color(s_wifi_value, SERVICE_GREEN, LV_PART_MAIN);
        return;
    }

    lv_label_set_text(s_wifi_value, "RADIO OK");
    lv_obj_set_style_text_color(s_wifi_value, SERVICE_GREEN, LV_PART_MAIN);
}

static void refresh_service(void)
{
    if (s_root == NULL) {
        return;
    }

    const bool rs485_ready = rs485_port_is_initialized();
    const bool modbus_ready = modbus_rtu_client_is_initialized();

    lv_label_set_text(
        s_rs485_status,
        rs485_ready ? "AKTYWNY" : "GOTOWY / NIEAKTYWNY"
    );
    lv_obj_set_style_text_color(
        s_rs485_status,
        rs485_ready ? SERVICE_GREEN : SERVICE_YELLOW,
        LV_PART_MAIN
    );

    lv_label_set_text(
        s_modbus_status,
        modbus_ready ? "AKTYWNY" : "OCZEKUJE NA MODUL"
    );
    lv_obj_set_style_text_color(
        s_modbus_status,
        modbus_ready ? SERVICE_GREEN : SERVICE_YELLOW,
        LV_PART_MAIN
    );

    static const uint8_t crc_test_frame[] = {
        0x01U, 0x03U, 0x00U, 0x00U, 0x00U, 0x0AU
    };
    const bool crc_ok =
        modbus_rtu_crc16(crc_test_frame, sizeof(crc_test_frame)) == 0xCDC5U;

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
    set_u32(s_exceptions_value, diagnostics.exceptions);

    if (diagnostics.last_error == ESP_OK) {
        lv_label_set_text(s_last_error_value, "OK");
    } else {
        char buffer[24];
        snprintf(buffer, sizeof(buffer), "0x%X", (unsigned int)diagnostics.last_error);
        lv_label_set_text(s_last_error_value, buffer);
    }

    smarttank_state_t state;
    app_model_get_snapshot(&state);

    lv_label_set_text(
        s_source_value,
        state.system.simulation_active ? "SYMULACJA" : "MODBUS"
    );
    set_u32(s_uptime_value, state.system.uptime_seconds);
    lv_label_set_text(
        s_module_value,
        state.system.analog_module_connected ? "ONLINE" : "NIEPODLACZONY"
    );

    refresh_rtc_status();
    refresh_wifi_status();
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
        "RS485 / RTC / WIFI",
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
    create_value_row(rs485_panel, "UART", "UART2", 66);
    create_value_row(rs485_panel, "TX", "GPIO44", 98);
    create_value_row(rs485_panel, "RX", "GPIO43", 130);
    create_value_row(rs485_panel, "Kierunek", "AUTO", 162);
    create_value_row(rs485_panel, "Transceiver", "SP3485", 194);
    create_value_row(rs485_panel, "Baud", "DO USTALENIA", 226);
    create_value_row(rs485_panel, "Przewody", "A / B / GND", 258);

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
    s_crc_status = create_value_row(modbus_panel, "CRC16 test", "--", 66);
    create_value_row(modbus_panel, "Funkcje", "03 / 04", 98);
    s_requests_value = create_value_row(modbus_panel, "Zapytania", "0", 130);
    s_responses_value = create_value_row(modbus_panel, "Odpowiedzi", "0", 162);
    s_timeouts_value = create_value_row(modbus_panel, "Timeouty", "0", 194);
    s_crc_errors_value = create_value_row(modbus_panel, "Bledy CRC", "0", 226);
    s_exceptions_value = create_value_row(modbus_panel, "Wyjatki", "0", 258);
    s_last_error_value = create_value_row(modbus_panel, "Ostatni blad", "OK", 290);

    lv_obj_t *status_panel = create_panel(s_root, 536, 244);
    create_label(
        status_panel,
        "STATUS SYSTEMU",
        SERVICE_YELLOW,
        LV_ALIGN_TOP_LEFT,
        0,
        0,
        &lv_font_montserrat_14
    );
    s_source_value = create_value_row(status_panel, "Zrodlo danych", "--", 34);
    s_uptime_value = create_value_row(status_panel, "Uptime [s]", "0", 66);
    s_module_value = create_value_row(status_panel, "Modul 8CH", "--", 98);
    s_rtc_status_value = create_value_row(status_panel, "RTC PCF85063", "--", 130);
    s_rtc_time_value = create_value_row(status_panel, "Czas RTC", "--", 162);
    s_wifi_value = create_value_row(status_panel, "Wi-Fi", "--", 194);
    create_value_row(status_panel, "Adres slave", "DO USTALENIA", 226);
    create_value_row(status_panel, "Mapa rejestrow", "BRAK", 258);

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
