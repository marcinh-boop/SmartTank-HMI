#include "screen_settings.h"

#include <stdio.h>

#include "theme.h"
#include "wifi_service.h"

#define SETTINGS_BLUE    lv_color_hex(0x2EA8FF)
#define SETTINGS_GREEN   lv_color_hex(0x39D12F)
#define SETTINGS_YELLOW  lv_color_hex(0xFFC247)
#define SETTINGS_RED     lv_color_hex(0xFF3333)
#define SETTINGS_PANEL   lv_color_hex(0x0B1825)
#define SETTINGS_BORDER  lv_color_hex(0x24384A)

#define SETTINGS_VISIBLE_APS 7U

static lv_obj_t *s_root;
static lv_timer_t *s_refresh_timer;
static lv_obj_t *s_radio_value;
static lv_obj_t *s_mac_value;
static lv_obj_t *s_scan_value;
static lv_obj_t *s_count_value;
static lv_obj_t *s_info_label;
static lv_obj_t *s_network_labels[SETTINGS_VISIBLE_APS];
static uint32_t s_last_scan_revision;

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
    lv_obj_set_style_bg_color(panel, SETTINGS_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, SETTINGS_BORDER, LV_PART_MAIN);
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

static void scan_button_event_cb(lv_event_t *event)
{
    (void)event;

    const esp_err_t result = wifi_service_request_scan();
    if (result == ESP_OK) {
        lv_label_set_text(s_scan_value, "START");
        lv_obj_set_style_text_color(s_scan_value, SETTINGS_BLUE, LV_PART_MAIN);
    } else if (result == ESP_ERR_INVALID_STATE) {
        lv_label_set_text(s_scan_value, "ZAJETE");
        lv_obj_set_style_text_color(s_scan_value, SETTINGS_YELLOW, LV_PART_MAIN);
    } else {
        lv_label_set_text(s_scan_value, "BLAD");
        lv_obj_set_style_text_color(s_scan_value, SETTINGS_RED, LV_PART_MAIN);
    }
}

static void clear_network_rows(void)
{
    for (uint8_t i = 0U; i < SETTINGS_VISIBLE_APS; i++) {
        lv_label_set_text(s_network_labels[i], "");
    }
}

static void refresh_settings(void)
{
    if (s_root == NULL) {
        return;
    }

    wifi_service_snapshot_t wifi;
    wifi_service_get_snapshot(&wifi);

    if (!wifi.started || !wifi.radio_ready) {
        lv_label_set_text(s_radio_value, "BLAD / OFF");
        lv_obj_set_style_text_color(s_radio_value, SETTINGS_RED, LV_PART_MAIN);
    } else if (wifi.connected) {
        lv_label_set_text(s_radio_value, "ONLINE");
        lv_obj_set_style_text_color(s_radio_value, SETTINGS_GREEN, LV_PART_MAIN);
    } else {
        lv_label_set_text(s_radio_value, "GOTOWE");
        lv_obj_set_style_text_color(s_radio_value, SETTINGS_GREEN, LV_PART_MAIN);
    }

    char buffer[96];
    snprintf(
        buffer,
        sizeof(buffer),
        "%02X:%02X:%02X:%02X:%02X:%02X",
        wifi.mac[0], wifi.mac[1], wifi.mac[2],
        wifi.mac[3], wifi.mac[4], wifi.mac[5]
    );
    lv_label_set_text(s_mac_value, buffer);

    if (wifi.scanning) {
        lv_label_set_text(s_scan_value, "SKANOWANIE");
        lv_obj_set_style_text_color(s_scan_value, SETTINGS_BLUE, LV_PART_MAIN);
    } else if (wifi.last_error == ESP_OK) {
        lv_label_set_text(s_scan_value, "GOTOWE");
        lv_obj_set_style_text_color(s_scan_value, SETTINGS_GREEN, LV_PART_MAIN);
    } else {
        snprintf(buffer, sizeof(buffer), "BLAD 0x%X", (unsigned int)wifi.last_error);
        lv_label_set_text(s_scan_value, buffer);
        lv_obj_set_style_text_color(s_scan_value, SETTINGS_RED, LV_PART_MAIN);
    }

    snprintf(buffer, sizeof(buffer), "%u", (unsigned int)wifi.total_ap_count);
    lv_label_set_text(s_count_value, buffer);

    if (wifi.scanning) {
        lv_label_set_text(s_info_label, "Trwa skanowanie sieci 2.4 GHz...");
    } else if (wifi.ap_count == 0U) {
        lv_label_set_text(s_info_label, "Brak wynikow. Nacisnij SKANUJ.");
    } else {
        lv_label_set_text(
            s_info_label,
            "Najsilniejsze wykryte sieci. Logowanie dodamy w kolejnym etapie."
        );
    }

    if (wifi.scan_revision == s_last_scan_revision && !wifi.scanning) {
        return;
    }

    s_last_scan_revision = wifi.scan_revision;
    clear_network_rows();

    const uint8_t visible_count =
        wifi.ap_count < SETTINGS_VISIBLE_APS ? wifi.ap_count : SETTINGS_VISIBLE_APS;

    for (uint8_t i = 0U; i < visible_count; i++) {
        const wifi_service_ap_t *ap = &wifi.aps[i];
        snprintf(
            buffer,
            sizeof(buffer),
            "%u. %-24.24s  %4d dBm  CH%u  %s",
            (unsigned int)(i + 1U),
            ap->ssid,
            (int)ap->rssi,
            (unsigned int)ap->channel,
            ap->secured ? "SEC" : "OPEN"
        );
        lv_label_set_text(s_network_labels[i], buffer);
    }
}

static void refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (s_root != NULL && !lv_obj_has_flag(s_root, LV_OBJ_FLAG_HIDDEN)) {
        refresh_settings();
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
        "USTAWIENIA",
        ST_COLOR_ACCENT,
        LV_ALIGN_CENTER,
        0,
        0,
        &lv_font_montserrat_14
    );
    create_label(
        top,
        "WIFI 2.4 GHz",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_RIGHT_MID,
        -12,
        0,
        &lv_font_montserrat_14
    );

    lv_obj_t *status_panel = create_panel(s_root, 20, 245);
    create_label(
        status_panel,
        "KARTA SIECIOWA",
        SETTINGS_BLUE,
        LV_ALIGN_TOP_LEFT,
        0,
        0,
        &lv_font_montserrat_14
    );

    s_radio_value = create_value_row(status_panel, "Radio", "--", 40);
    s_mac_value = create_value_row(status_panel, "MAC", "--", 76);
    s_scan_value = create_value_row(status_panel, "Skan", "--", 112);
    s_count_value = create_value_row(status_panel, "Sieci znalezione", "0", 148);
    create_value_row(status_panel, "Tryb", "STACJA", 184);
    create_value_row(status_panel, "Pasmo", "2.4 GHz", 220);

    lv_obj_t *scan_button = lv_btn_create(status_panel);
    lv_obj_set_size(scan_button, 210, 38);
    lv_obj_align(scan_button, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(scan_button, lv_color_hex(0x12314A), LV_PART_MAIN);
    lv_obj_set_style_bg_color(
        scan_button,
        lv_color_hex(0x1A4566),
        LV_PART_MAIN | LV_STATE_PRESSED
    );
    lv_obj_set_style_radius(scan_button, 7, LV_PART_MAIN);
    lv_obj_set_style_border_width(scan_button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(scan_button, SETTINGS_BLUE, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(scan_button, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(scan_button, scan_button_event_cb, LV_EVENT_RELEASED, NULL);
    create_label(
        scan_button,
        "SKANUJ SIECI",
        ST_COLOR_TEXT,
        LV_ALIGN_CENTER,
        0,
        0,
        &lv_font_montserrat_14
    );

    lv_obj_t *networks_panel = create_panel(s_root, 278, 502);
    create_label(
        networks_panel,
        "DOSTEPNE SIECI",
        SETTINGS_GREEN,
        LV_ALIGN_TOP_LEFT,
        0,
        0,
        &lv_font_montserrat_14
    );

    s_info_label = create_label(
        networks_panel,
        "Oczekiwanie na skan...",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_TOP_RIGHT,
        0,
        2,
        &lv_font_montserrat_12
    );
    lv_obj_set_width(s_info_label, 310);
    lv_label_set_long_mode(s_info_label, LV_LABEL_LONG_DOT);

    for (uint8_t i = 0U; i < SETTINGS_VISIBLE_APS; i++) {
        s_network_labels[i] = create_label(
            networks_panel,
            "",
            ST_COLOR_TEXT,
            LV_ALIGN_TOP_LEFT,
            0,
            38 + ((int)i * 38),
            &lv_font_montserrat_12
        );
        lv_obj_set_width(s_network_labels[i], 468);
        lv_label_set_long_mode(s_network_labels[i], LV_LABEL_LONG_DOT);
    }

    s_refresh_timer = lv_timer_create(refresh_timer_cb, 500, NULL);
    refresh_settings();
}

void screen_settings_open(lv_obj_t *parent_screen)
{
    if (parent_screen == NULL) {
        return;
    }

    if (s_root == NULL) {
        build_screen(parent_screen);
    }

    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_root);
    refresh_settings();
}

void screen_settings_hide(void)
{
    if (s_root != NULL) {
        lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    }
}
