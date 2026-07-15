/*
 * Element ekranu screen_settings.c: tworzy widok LVGL, obsługuje zdarzenia użytkownika i odświeża prezentowane dane.
 * Implementacja ukrywa szczegóły działania; inne moduły powinny korzystać z odpowiadającego jej API.
 * Oddzielenie odpowiedzialności ułatwia testowanie, diagnostykę i późniejszą rozbudowę urządzenia.
 */
#include "screen_settings.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "settings_storage.h"
#include "theme.h"
#include "wifi_service.h"

#define SETTINGS_BLUE    lv_color_hex(0x2EA8FF)
#define SETTINGS_GREEN   lv_color_hex(0x39D12F)
#define SETTINGS_YELLOW  lv_color_hex(0xFFC247)
#define SETTINGS_RED     lv_color_hex(0xFF3333)
#define SETTINGS_PANEL   lv_color_hex(0x0B1825)
#define SETTINGS_BORDER  lv_color_hex(0x24384A)

#define SETTINGS_VISIBLE_APS 7U

typedef struct {
    uint8_t index;
} network_row_ctx_t;

static lv_obj_t *s_root;
static lv_timer_t *s_refresh_timer;
static lv_obj_t *s_radio_value;
static lv_obj_t *s_network_value;
static lv_obj_t *s_ip_value;
static lv_obj_t *s_signal_value;
static lv_obj_t *s_mac_value;
static lv_obj_t *s_scan_value;
static lv_obj_t *s_info_label;
static lv_obj_t *s_network_buttons[SETTINGS_VISIBLE_APS];
static lv_obj_t *s_network_labels[SETTINGS_VISIBLE_APS];
static network_row_ctx_t s_network_contexts[SETTINGS_VISIBLE_APS];
static uint32_t s_last_scan_revision;
static bool s_scan_requested_once;

static lv_obj_t *s_connect_overlay;
static lv_obj_t *s_ssid_textarea;
static lv_obj_t *s_password_textarea;
static lv_obj_t *s_keyboard;
static lv_obj_t *s_connect_status;
static bool s_selected_network_secured;

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

    lv_obj_t *value_label = create_label(
        parent,
        value,
        ST_COLOR_TEXT,
        LV_ALIGN_TOP_RIGHT,
        0,
        y,
        &lv_font_montserrat_12
    );
    lv_obj_set_width(value_label, 145);
    lv_obj_set_style_text_align(value_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_label_set_long_mode(value_label, LV_LABEL_LONG_DOT);
    return value_label;
}

static lv_obj_t *create_action_button(
    lv_obj_t *parent,
    const char *text,
    int x,
    lv_event_cb_t callback)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_size(button, 100, 38);
    lv_obj_align(button, LV_ALIGN_BOTTOM_LEFT, x, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x12314A), LV_PART_MAIN);
    lv_obj_set_style_bg_color(
        button,
        lv_color_hex(0x1A4566),
        LV_PART_MAIN | LV_STATE_PRESSED
    );
    lv_obj_set_style_radius(button, 7, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(button, SETTINGS_BLUE, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(button, callback, LV_EVENT_RELEASED, NULL);

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

static void close_connect_overlay(void)
{
    if (s_connect_overlay != NULL) {
        lv_obj_add_flag(s_connect_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

static void textarea_focus_event_cb(lv_event_t *event)
{
    lv_obj_t *textarea = lv_event_get_target(event);
    if (textarea != NULL && s_keyboard != NULL) {
        lv_keyboard_set_textarea(s_keyboard, textarea);
    }
}

static void open_connect_overlay(const char *ssid, bool secured)
{
    if (s_connect_overlay == NULL || ssid == NULL) {
        return;
    }

    s_selected_network_secured = secured;
    lv_textarea_set_text(s_ssid_textarea, ssid);
    lv_textarea_set_text(s_password_textarea, "");
    lv_keyboard_set_textarea(s_keyboard, s_password_textarea);
    lv_label_set_text(
        s_connect_status,
        secured ? "Wpisz haslo i nacisnij POLACZ." : "Siec otwarta - haslo moze pozostac puste."
    );
    lv_obj_set_style_text_color(s_connect_status, ST_COLOR_TEXT_DIM, LV_PART_MAIN);
    lv_obj_clear_flag(s_connect_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_connect_overlay);
}

static void connect_button_event_cb(lv_event_t *event)
{
    (void)event;

    const char *ssid = lv_textarea_get_text(s_ssid_textarea);
    const char *password = lv_textarea_get_text(s_password_textarea);

    if (ssid == NULL || ssid[0] == '\0') {
        lv_label_set_text(s_connect_status, "SSID nie moze byc puste.");
        lv_obj_set_style_text_color(s_connect_status, SETTINGS_RED, LV_PART_MAIN);
        return;
    }

    if (s_selected_network_secured &&
        (password == NULL || password[0] == '\0')) {
        lv_label_set_text(s_connect_status, "Wpisz haslo do zabezpieczonej sieci.");
        lv_obj_set_style_text_color(s_connect_status, SETTINGS_RED, LV_PART_MAIN);
        return;
    }

    const esp_err_t connect_result = wifi_service_connect(
        ssid,
        password != NULL ? password : ""
    );

    if (connect_result != ESP_OK) {
        char buffer[48];
        snprintf(
            buffer,
            sizeof(buffer),
            "Blad startu polaczenia: 0x%X",
            (unsigned int)connect_result
        );
        lv_label_set_text(s_connect_status, buffer);
        lv_obj_set_style_text_color(s_connect_status, SETTINGS_RED, LV_PART_MAIN);
        return;
    }

    wifi_credentials_t credentials = {0};
    strncpy(credentials.ssid, ssid, sizeof(credentials.ssid) - 1U);
    if (password != NULL) {
        strncpy(
            credentials.password,
            password,
            sizeof(credentials.password) - 1U
        );
    }

    const esp_err_t save_result =
        settings_storage_save_wifi_credentials(&credentials);

    if (save_result != ESP_OK) {
        lv_label_set_text(
            s_connect_status,
            "Laczenie trwa, ale zapis NVS nie powiodl sie."
        );
        lv_obj_set_style_text_color(s_connect_status, SETTINGS_YELLOW, LV_PART_MAIN);
        return;
    }

    lv_label_set_text(s_connect_status, "LACZENIE...");
    lv_obj_set_style_text_color(s_connect_status, SETTINGS_BLUE, LV_PART_MAIN);
}

static void cancel_button_event_cb(lv_event_t *event)
{
    (void)event;
    close_connect_overlay();
}

static void keyboard_event_cb(lv_event_t *event)
{
    const lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_READY) {
        connect_button_event_cb(event);
    } else if (code == LV_EVENT_CANCEL) {
        close_connect_overlay();
    }
}

static void network_row_event_cb(lv_event_t *event)
{
    const network_row_ctx_t *context = lv_event_get_user_data(event);
    if (context == NULL) {
        return;
    }

    wifi_service_snapshot_t wifi;
    wifi_service_get_snapshot(&wifi);

    if (context->index >= wifi.ap_count) {
        return;
    }

    const wifi_service_ap_t *ap = &wifi.aps[context->index];
    open_connect_overlay(ap->ssid, ap->secured);
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

static void forget_button_event_cb(lv_event_t *event)
{
    (void)event;

    const esp_err_t clear_result = settings_storage_clear_wifi_credentials();
    const esp_err_t disconnect_result = wifi_service_disconnect();

    if (clear_result == ESP_OK && disconnect_result == ESP_OK) {
        lv_label_set_text(s_info_label, "Siec zapomniana. Wybierz nowa siec z listy.");
        lv_obj_set_style_text_color(s_info_label, SETTINGS_YELLOW, LV_PART_MAIN);
    } else {
        lv_label_set_text(s_info_label, "Nie udalo sie usunac konfiguracji Wi-Fi.");
        lv_obj_set_style_text_color(s_info_label, SETTINGS_RED, LV_PART_MAIN);
    }
}

static void clear_network_rows(void)
{
    for (uint8_t i = 0U; i < SETTINGS_VISIBLE_APS; i++) {
        lv_label_set_text(s_network_labels[i], "");
        lv_obj_add_flag(s_network_buttons[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void refresh_connect_overlay(const wifi_service_snapshot_t *wifi)
{
    if (wifi == NULL ||
        s_connect_overlay == NULL ||
        lv_obj_has_flag(s_connect_overlay, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    const char *selected_ssid = lv_textarea_get_text(s_ssid_textarea);
    if (selected_ssid == NULL ||
        strncmp(selected_ssid, wifi->ssid, sizeof(wifi->ssid)) != 0) {
        return;
    }

    char buffer[96];

    if (wifi->connected) {
        snprintf(
            buffer,
            sizeof(buffer),
            "POLACZONO | IP %s | %d dBm",
            wifi->ip_address,
            (int)wifi->rssi
        );
        lv_label_set_text(s_connect_status, buffer);
        lv_obj_set_style_text_color(s_connect_status, SETTINGS_GREEN, LV_PART_MAIN);
    } else if (wifi->connecting) {
        snprintf(
            buffer,
            sizeof(buffer),
            "LACZENIE... proba %u/%u",
            (unsigned int)(wifi->retry_count + 1U),
            6U
        );
        lv_label_set_text(s_connect_status, buffer);
        lv_obj_set_style_text_color(s_connect_status, SETTINGS_BLUE, LV_PART_MAIN);
    } else if (wifi->configured && wifi->last_error != ESP_OK) {
        snprintf(
            buffer,
            sizeof(buffer),
            "BRAK POLACZENIA | powod %u",
            (unsigned int)wifi->last_disconnect_reason
        );
        lv_label_set_text(s_connect_status, buffer);
        lv_obj_set_style_text_color(s_connect_status, SETTINGS_RED, LV_PART_MAIN);
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
    } else if (wifi.connecting) {
        lv_label_set_text(s_radio_value, "LACZENIE");
        lv_obj_set_style_text_color(s_radio_value, SETTINGS_BLUE, LV_PART_MAIN);
    } else {
        lv_label_set_text(s_radio_value, "GOTOWE");
        lv_obj_set_style_text_color(s_radio_value, SETTINGS_GREEN, LV_PART_MAIN);
    }

    lv_label_set_text(
        s_network_value,
        wifi.ssid[0] != '\0' ? wifi.ssid : "NIE WYBRANO"
    );
    lv_label_set_text(s_ip_value, wifi.ip_address);

    char buffer[128];
    if (wifi.connected) {
        snprintf(buffer, sizeof(buffer), "%d dBm", (int)wifi.rssi);
    } else {
        snprintf(buffer, sizeof(buffer), "--");
    }
    lv_label_set_text(s_signal_value, buffer);

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

    if (wifi.scanning) {
        lv_label_set_text(s_info_label, "Trwa skanowanie sieci 2.4 GHz...");
        lv_obj_set_style_text_color(s_info_label, ST_COLOR_TEXT_DIM, LV_PART_MAIN);
    } else if (wifi.ap_count == 0U) {
        lv_label_set_text(s_info_label, "Brak wynikow. Nacisnij SKANUJ.");
        lv_obj_set_style_text_color(s_info_label, ST_COLOR_TEXT_DIM, LV_PART_MAIN);
    } else {
        snprintf(
            buffer,
            sizeof(buffer),
            "Znaleziono %u sieci. Dotknij wybranej pozycji.",
            (unsigned int)wifi.total_ap_count
        );
        lv_label_set_text(s_info_label, buffer);
        lv_obj_set_style_text_color(s_info_label, ST_COLOR_TEXT_DIM, LV_PART_MAIN);
    }

    refresh_connect_overlay(&wifi);

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
        lv_obj_clear_flag(s_network_buttons[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (s_root != NULL && !lv_obj_has_flag(s_root, LV_OBJ_FLAG_HIDDEN)) {
        refresh_settings();
    }
}

static void build_connect_overlay(void)
{
    s_connect_overlay = lv_obj_create(s_root);
    lv_obj_remove_style_all(s_connect_overlay);
    lv_obj_set_size(s_connect_overlay, 800, 398);
    lv_obj_align(s_connect_overlay, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(s_connect_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_connect_overlay, ST_COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_connect_overlay, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *top = lv_obj_create(s_connect_overlay);
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
        "POLACZENIE WIFI",
        ST_COLOR_ACCENT,
        LV_ALIGN_CENTER,
        0,
        0,
        &lv_font_montserrat_14
    );
    create_label(
        top,
        "HASLO ZAPISYWANE W NVS",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_RIGHT_MID,
        -12,
        0,
        &lv_font_montserrat_12
    );

    lv_obj_t *form_panel = create_panel(s_connect_overlay, 20, 300);
    create_label(
        form_panel,
        "DANE SIECI",
        SETTINGS_BLUE,
        LV_ALIGN_TOP_LEFT,
        0,
        0,
        &lv_font_montserrat_14
    );

    create_label(
        form_panel,
        "SSID",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_TOP_LEFT,
        0,
        34,
        &lv_font_montserrat_12
    );
    s_ssid_textarea = lv_textarea_create(form_panel);
    lv_obj_set_size(s_ssid_textarea, 270, 42);
    lv_obj_align(s_ssid_textarea, LV_ALIGN_TOP_LEFT, 0, 52);
    lv_textarea_set_one_line(s_ssid_textarea, true);
    lv_textarea_set_max_length(s_ssid_textarea, WIFI_SERVICE_SSID_MAX_LEN);
    lv_obj_add_event_cb(
        s_ssid_textarea,
        textarea_focus_event_cb,
        LV_EVENT_FOCUSED,
        NULL
    );

    create_label(
        form_panel,
        "HASLO",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_TOP_LEFT,
        0,
        104,
        &lv_font_montserrat_12
    );
    s_password_textarea = lv_textarea_create(form_panel);
    lv_obj_set_size(s_password_textarea, 270, 42);
    lv_obj_align(s_password_textarea, LV_ALIGN_TOP_LEFT, 0, 122);
    lv_textarea_set_one_line(s_password_textarea, true);
    lv_textarea_set_password_mode(s_password_textarea, true);
    lv_textarea_set_password_show_time(s_password_textarea, 1200);
    lv_textarea_set_max_length(
        s_password_textarea,
        WIFI_SERVICE_PASSWORD_MAX_LEN
    );
    lv_obj_add_event_cb(
        s_password_textarea,
        textarea_focus_event_cb,
        LV_EVENT_FOCUSED,
        NULL
    );

    s_connect_status = create_label(
        form_panel,
        "Wybierz siec.",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_TOP_LEFT,
        0,
        176,
        &lv_font_montserrat_12
    );
    lv_obj_set_width(s_connect_status, 270);
    lv_label_set_long_mode(s_connect_status, LV_LABEL_LONG_WRAP);

    lv_obj_t *connect_button = lv_btn_create(form_panel);
    lv_obj_set_size(connect_button, 130, 42);
    lv_obj_align(connect_button, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(connect_button, lv_color_hex(0x164B2A), LV_PART_MAIN);
    lv_obj_set_style_bg_color(
        connect_button,
        lv_color_hex(0x1D6638),
        LV_PART_MAIN | LV_STATE_PRESSED
    );
    lv_obj_set_style_border_width(connect_button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(connect_button, SETTINGS_GREEN, LV_PART_MAIN);
    lv_obj_set_style_radius(connect_button, 7, LV_PART_MAIN);
    lv_obj_add_event_cb(
        connect_button,
        connect_button_event_cb,
        LV_EVENT_RELEASED,
        NULL
    );
    create_label(
        connect_button,
        "POLACZ",
        ST_COLOR_TEXT,
        LV_ALIGN_CENTER,
        0,
        0,
        &lv_font_montserrat_14
    );

    lv_obj_t *cancel_button = lv_btn_create(form_panel);
    lv_obj_set_size(cancel_button, 130, 42);
    lv_obj_align(cancel_button, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(cancel_button, lv_color_hex(0x3A2020), LV_PART_MAIN);
    lv_obj_set_style_bg_color(
        cancel_button,
        lv_color_hex(0x563030),
        LV_PART_MAIN | LV_STATE_PRESSED
    );
    lv_obj_set_style_border_width(cancel_button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(cancel_button, SETTINGS_RED, LV_PART_MAIN);
    lv_obj_set_style_radius(cancel_button, 7, LV_PART_MAIN);
    lv_obj_add_event_cb(
        cancel_button,
        cancel_button_event_cb,
        LV_EVENT_RELEASED,
        NULL
    );
    create_label(
        cancel_button,
        "ANULUJ",
        ST_COLOR_TEXT,
        LV_ALIGN_CENTER,
        0,
        0,
        &lv_font_montserrat_14
    );

    lv_obj_t *keyboard_panel = create_panel(s_connect_overlay, 332, 448);
    create_label(
        keyboard_panel,
        "KLAWIATURA",
        SETTINGS_GREEN,
        LV_ALIGN_TOP_LEFT,
        0,
        0,
        &lv_font_montserrat_14
    );

    s_keyboard = lv_keyboard_create(keyboard_panel);
    lv_obj_set_size(s_keyboard, 422, 270);
    lv_obj_align(s_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_mode(s_keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_add_event_cb(s_keyboard, keyboard_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_add_flag(s_connect_overlay, LV_OBJ_FLAG_HIDDEN);
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

    s_radio_value = create_value_row(status_panel, "Radio", "--", 34);
    s_network_value = create_value_row(status_panel, "Siec", "--", 66);
    s_ip_value = create_value_row(status_panel, "Adres IP", "--", 98);
    s_signal_value = create_value_row(status_panel, "Sygnal", "--", 130);
    s_mac_value = create_value_row(status_panel, "MAC", "--", 162);
    s_scan_value = create_value_row(status_panel, "Skan", "--", 194);

    create_action_button(status_panel, "SKANUJ", 0, scan_button_event_cb);
    create_action_button(status_panel, "ZAPOMNIJ", 110, forget_button_event_cb);

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
    lv_obj_set_style_text_align(s_info_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_label_set_long_mode(s_info_label, LV_LABEL_LONG_DOT);

    for (uint8_t i = 0U; i < SETTINGS_VISIBLE_APS; i++) {
        s_network_contexts[i].index = i;

        s_network_buttons[i] = lv_btn_create(networks_panel);
        lv_obj_remove_style_all(s_network_buttons[i]);
        lv_obj_set_size(s_network_buttons[i], 468, 32);
        lv_obj_align(
            s_network_buttons[i],
            LV_ALIGN_TOP_LEFT,
            0,
            38 + ((int)i * 36)
        );
        lv_obj_set_style_bg_color(
            s_network_buttons[i],
            lv_color_hex(0x12314A),
            LV_PART_MAIN | LV_STATE_PRESSED
        );
        lv_obj_set_style_bg_opa(
            s_network_buttons[i],
            LV_OPA_TRANSP,
            LV_PART_MAIN | LV_STATE_DEFAULT
        );
        lv_obj_set_style_bg_opa(
            s_network_buttons[i],
            LV_OPA_COVER,
            LV_PART_MAIN | LV_STATE_PRESSED
        );
        lv_obj_set_style_radius(s_network_buttons[i], 6, LV_PART_MAIN);
        lv_obj_add_event_cb(
            s_network_buttons[i],
            network_row_event_cb,
            LV_EVENT_RELEASED,
            &s_network_contexts[i]
        );

        s_network_labels[i] = create_label(
            s_network_buttons[i],
            "",
            ST_COLOR_TEXT,
            LV_ALIGN_LEFT_MID,
            4,
            0,
            &lv_font_montserrat_12
        );
        lv_obj_set_width(s_network_labels[i], 458);
        lv_label_set_long_mode(s_network_labels[i], LV_LABEL_LONG_DOT);
        lv_obj_add_flag(s_network_buttons[i], LV_OBJ_FLAG_HIDDEN);
    }

    build_connect_overlay();

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

    wifi_service_snapshot_t wifi;
    wifi_service_get_snapshot(&wifi);
    if (!s_scan_requested_once && !wifi.scanning && wifi.ap_count == 0U) {
        if (wifi_service_request_scan() == ESP_OK) {
            s_scan_requested_once = true;
        }
    }

    refresh_settings();
}

void screen_settings_hide(void)
{
    close_connect_overlay();

    if (s_root != NULL) {
        lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    }
}
