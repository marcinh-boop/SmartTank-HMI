#include "screen_info.h"

#include <stdio.h>

#include "alarm_service.h"
#include "analog_module_service.h"
#include "app_model.h"
#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "theme.h"
#include "wifi_service.h"

#define INFO_BLUE    lv_color_hex(0x2EA8FF)
#define INFO_GREEN   lv_color_hex(0x39D12F)
#define INFO_YELLOW  lv_color_hex(0xFFC247)
#define INFO_RED     lv_color_hex(0xFF4D4D)
#define INFO_PANEL   lv_color_hex(0x0B1825)
#define INFO_BORDER  lv_color_hex(0x24384A)

#define INFO_AUTHOR_NAME  "Marcin Hoinca"
#define INFO_AUTHOR_EMAIL "marcin.hoinca@gmail.com"

static lv_obj_t *s_root;
static lv_timer_t *s_refresh_timer;

static lv_obj_t *s_heap_internal_value;
static lv_obj_t *s_heap_internal_min_value;
static lv_obj_t *s_heap_psram_value;
static lv_obj_t *s_uptime_value;
static lv_obj_t *s_wifi_value;
static lv_obj_t *s_ssid_value;
static lv_obj_t *s_ip_value;
static lv_obj_t *s_rssi_value;
static lv_obj_t *s_alarm_value;
static lv_obj_t *s_module_value;

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

static lv_obj_t *create_panel(
    lv_obj_t *parent,
    int x,
    int width,
    const char *title,
    lv_color_t title_color)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, width, 320);
    lv_obj_align(panel, LV_ALIGN_TOP_LEFT, x, 68);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(panel, INFO_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, INFO_BORDER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 12, LV_PART_MAIN);

    create_label(
        panel,
        title,
        title_color,
        LV_ALIGN_TOP_LEFT,
        0,
        0,
        &lv_font_montserrat_14
    );

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
    lv_obj_set_width(value_label, 132);
    lv_obj_set_style_text_align(value_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_label_set_long_mode(value_label, LV_LABEL_LONG_DOT);
    return value_label;
}

static const char *reset_reason_name(esp_reset_reason_t reason)
{
    switch (reason) {
        case ESP_RST_POWERON:
            return "ZASILANIE";
        case ESP_RST_EXT:
            return "ZEWNETRZNY";
        case ESP_RST_SW:
            return "PROGRAMOWY";
        case ESP_RST_PANIC:
            return "PANIC";
        case ESP_RST_INT_WDT:
            return "WDT CPU";
        case ESP_RST_TASK_WDT:
            return "WDT TASK";
        case ESP_RST_WDT:
            return "WDT";
        case ESP_RST_DEEPSLEEP:
            return "DEEP SLEEP";
        case ESP_RST_BROWNOUT:
            return "BROWNOUT";
        default:
            return "INNY";
    }
}

static void format_bytes(char *buffer, size_t buffer_size, size_t bytes)
{
    if (bytes >= (1024U * 1024U)) {
        snprintf(
            buffer,
            buffer_size,
            "%.1f MB",
            (double)bytes / (1024.0 * 1024.0)
        );
    } else {
        snprintf(
            buffer,
            buffer_size,
            "%u KB",
            (unsigned int)(bytes / 1024U)
        );
    }
}

static void format_uptime(char *buffer, size_t buffer_size, uint32_t seconds)
{
    const uint32_t days = seconds / 86400U;
    seconds %= 86400U;
    const uint32_t hours = seconds / 3600U;
    seconds %= 3600U;
    const uint32_t minutes = seconds / 60U;

    if (days > 0U) {
        snprintf(
            buffer,
            buffer_size,
            "%lud %02lu:%02lu",
            (unsigned long)days,
            (unsigned long)hours,
            (unsigned long)minutes
        );
    } else {
        snprintf(
            buffer,
            buffer_size,
            "%02lu:%02lu:%02lu",
            (unsigned long)hours,
            (unsigned long)minutes,
            (unsigned long)(seconds % 60U)
        );
    }
}

static void refresh_dynamic_values(void)
{
    char buffer[64];

    format_bytes(
        buffer,
        sizeof(buffer),
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
    );
    lv_label_set_text(s_heap_internal_value, buffer);

    format_bytes(
        buffer,
        sizeof(buffer),
        heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
    );
    lv_label_set_text(s_heap_internal_min_value, buffer);

    format_bytes(
        buffer,
        sizeof(buffer),
        heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
    );
    lv_label_set_text(s_heap_psram_value, buffer);

    smarttank_state_t state;
    app_model_get_snapshot(&state);
    format_uptime(buffer, sizeof(buffer), state.system.uptime_seconds);
    lv_label_set_text(s_uptime_value, buffer);

    wifi_service_snapshot_t wifi;
    wifi_service_get_snapshot(&wifi);
    lv_label_set_text(s_wifi_value, wifi.connected ? "ONLINE" : "OFFLINE");
    lv_obj_set_style_text_color(
        s_wifi_value,
        wifi.connected ? INFO_GREEN : INFO_YELLOW,
        LV_PART_MAIN
    );
    lv_label_set_text(s_ssid_value, wifi.ssid[0] != '\0' ? wifi.ssid : "--");
    lv_label_set_text(
        s_ip_value,
        wifi.connected && wifi.ip_address[0] != '\0' ? wifi.ip_address : "--"
    );

    if (wifi.connected) {
        snprintf(buffer, sizeof(buffer), "%d dBm", (int)wifi.rssi);
        lv_label_set_text(s_rssi_value, buffer);
    } else {
        lv_label_set_text(s_rssi_value, "--");
    }

    alarm_service_snapshot_t alarms;
    alarm_service_get_snapshot(&alarms);
    snprintf(
        buffer,
        sizeof(buffer),
        "%u aktywne",
        (unsigned int)alarms.active_count
    );
    lv_label_set_text(s_alarm_value, buffer);
    lv_obj_set_style_text_color(
        s_alarm_value,
        alarms.active_count > 0U ? INFO_RED : INFO_GREEN,
        LV_PART_MAIN
    );

    analog_module_service_snapshot_t module;
    analog_module_service_get_snapshot(&module);
    lv_label_set_text(
        s_module_value,
        analog_module_service_state_name(module.state)
    );
    lv_obj_set_style_text_color(
        s_module_value,
        module.online ? INFO_GREEN : INFO_YELLOW,
        LV_PART_MAIN
    );
}

static void refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (s_root != NULL && !lv_obj_has_flag(s_root, LV_OBJ_FLAG_HIDDEN)) {
        refresh_dynamic_values();
    }
}

static void build_firmware_panel(lv_obj_t *parent)
{
    lv_obj_t *panel = create_panel(parent, 20, 245, "OPROGRAMOWANIE", INFO_BLUE);
    const esp_app_desc_t *description = esp_app_get_description();
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);

    char sha[17];
    for (size_t index = 0U; index < 8U; index++) {
        snprintf(
            &sha[index * 2U],
            sizeof(sha) - (index * 2U),
            "%02x",
            description->app_elf_sha256[index]
        );
    }
    sha[16] = '\0';

    create_value_row(panel, "Projekt", description->project_name, 34);
    create_value_row(panel, "Wersja", description->version, 64);
    create_value_row(panel, "Data", description->date, 94);
    create_value_row(panel, "ESP-IDF", description->idf_ver, 124);
    create_value_row(panel, "SHA", sha, 154);
    create_value_row(panel, "Partycja", running != NULL ? running->label : "--", 184);

    lv_obj_t *ota_value = create_value_row(
        panel,
        "OTA A/B",
        next != NULL ? "GOTOWE" : "BRAK SLOTU",
        214
    );
    lv_obj_set_style_text_color(
        ota_value,
        next != NULL ? INFO_GREEN : INFO_RED,
        LV_PART_MAIN
    );

    create_value_row(panel, "Autor", INFO_AUTHOR_NAME, 244);

    lv_obj_t *email = create_label(
        panel,
        INFO_AUTHOR_EMAIL,
        INFO_BLUE,
        LV_ALIGN_TOP_MID,
        0,
        274,
        &lv_font_montserrat_12
    );
    lv_obj_set_width(email, 220);
    lv_obj_set_style_text_align(email, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
}

static void build_hardware_panel(lv_obj_t *parent)
{
    lv_obj_t *panel = create_panel(parent, 278, 245, "SPRZET", INFO_GREEN);

    esp_chip_info_t chip;
    esp_chip_info(&chip);

    char cores[16];
    char revision[16];
    char flash[24];
    uint32_t flash_size = 0U;

    snprintf(cores, sizeof(cores), "%u", (unsigned int)chip.cores);
    snprintf(revision, sizeof(revision), "%u", (unsigned int)chip.revision);

    if (esp_flash_get_size(NULL, &flash_size) == ESP_OK) {
        format_bytes(flash, sizeof(flash), flash_size);
    } else {
        snprintf(flash, sizeof(flash), "--");
    }

    create_value_row(panel, "Plyta", "Waveshare 5\"", 34);
    create_value_row(panel, "SoC", "ESP32-S3", 64);
    create_value_row(panel, "Rdzenie", cores, 94);
    create_value_row(panel, "Rewizja", revision, 124);
    create_value_row(panel, "Flash", flash, 154);
    s_heap_psram_value = create_value_row(panel, "PSRAM wolne", "--", 184);
    s_heap_internal_value = create_value_row(panel, "RAM wolne", "--", 214);
    s_heap_internal_min_value = create_value_row(panel, "RAM minimum", "--", 244);
    create_value_row(panel, "Reset", reset_reason_name(esp_reset_reason()), 274);
}

static void build_system_panel(lv_obj_t *parent)
{
    lv_obj_t *panel = create_panel(parent, 536, 244, "STATUS SYSTEMU", INFO_YELLOW);
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);

    s_uptime_value = create_value_row(panel, "Uptime", "--", 34);
    s_wifi_value = create_value_row(panel, "Wi-Fi", "--", 64);
    s_ssid_value = create_value_row(panel, "SSID", "--", 94);
    s_ip_value = create_value_row(panel, "IP", "--", 124);
    s_rssi_value = create_value_row(panel, "Sygnal", "--", 154);
    s_alarm_value = create_value_row(panel, "Alarmy", "--", 184);
    s_module_value = create_value_row(panel, "Modul 8CH", "--", 214);
    create_value_row(panel, "Aktywny slot", running != NULL ? running->label : "--", 244);
    create_value_row(panel, "Nastepny slot", next != NULL ? next->label : "--", 274);
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
        "INFORMACJE",
        ST_COLOR_ACCENT,
        LV_ALIGN_CENTER,
        0,
        0,
        &lv_font_montserrat_14
    );
    create_label(
        top,
        "SYSTEM / OTA",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_RIGHT_MID,
        -12,
        0,
        &lv_font_montserrat_14
    );

    build_firmware_panel(s_root);
    build_hardware_panel(s_root);
    build_system_panel(s_root);

    s_refresh_timer = lv_timer_create(refresh_timer_cb, 1000, NULL);
    refresh_dynamic_values();
}

void screen_info_open(lv_obj_t *parent_screen)
{
    if (parent_screen == NULL) {
        return;
    }

    if (s_root == NULL) {
        build_screen(parent_screen);
    }

    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_root);
    refresh_dynamic_values();
}

void screen_info_hide(void)
{
    if (s_root != NULL) {
        lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    }
}
