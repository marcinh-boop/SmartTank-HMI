#include "top_status_bar.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analog_module_service.h"
#include "clock_service.h"
#include "wifi_service.h"

#define TOP_BAR_WIDTH          760
#define TOP_BAR_HEIGHT         50
#define TOP_BAR_Y              8
#define TOP_BAR_REFRESH_MS     500U

#define TOP_COLOR_BG           lv_color_hex(0x08131F)
#define TOP_COLOR_PANEL        lv_color_hex(0x0B1825)
#define TOP_COLOR_BORDER       lv_color_hex(0x24384A)
#define TOP_COLOR_TEXT         lv_color_hex(0xFFFFFF)
#define TOP_COLOR_DIM          lv_color_hex(0x8FA3B8)
#define TOP_COLOR_BLUE         lv_color_hex(0x2EA8FF)
#define TOP_COLOR_GREEN        lv_color_hex(0x39D12F)
#define TOP_COLOR_YELLOW       lv_color_hex(0xFFC247)
#define TOP_COLOR_RED          lv_color_hex(0xFF4D4D)

typedef struct {
    lv_obj_t *root;
    lv_obj_t *label;
    int last_state;
} top_status_chip_t;

typedef struct {
    const char *nav_label;
    const char *page_title;
    bool overlay_page;
} top_status_nav_link_t;

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *root;
    lv_obj_t *legacy_header;
    lv_obj_t *legacy_page_label;
    lv_obj_t *page_label;
    lv_obj_t *time_label;
    lv_obj_t *date_label;
    lv_obj_t *day_source_label;
    top_status_chip_t wifi_chip;
    top_status_chip_t mqtt_chip;
    top_status_chip_t rs485_chip;
    lv_timer_t *timer;
    bool page_override;
    char page_override_text[24];
} top_status_bar_state_t;

static top_status_bar_state_t s_bar;
static volatile top_status_mqtt_state_t s_mqtt_state = TOP_STATUS_MQTT_DISABLED;

static top_status_nav_link_t s_nav_links[] = {
    {"Pulpit", "PULPIT", false},
    {"Historia", "HISTORIA", false},
    {"Alarmy", "ALARMY", true},
    {"Ustawienia", "USTAWIENIA", true},
    {"Serwis", "SERWIS", true},
    {"Informacje", "INFORMACJE", true},
};

static const char *WEEKDAY_NAMES[] = {
    "Niedziela",
    "Poniedzialek",
    "Wtorek",
    "Sroda",
    "Czwartek",
    "Piatek",
    "Sobota",
};

static void set_label_text_if_changed(lv_obj_t *label, const char *text)
{
    if (label == NULL || text == NULL) {
        return;
    }

    const char *current = lv_label_get_text(label);
    if (current == NULL || strcmp(current, text) != 0) {
        lv_label_set_text(label, text);
    }
}

static lv_obj_t *create_text_label(
    lv_obj_t *parent,
    const char *text,
    const lv_font_t *font,
    lv_color_t color,
    int x,
    int y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, x, y);
    return label;
}

static lv_obj_t *create_separator(lv_obj_t *parent, int x)
{
    lv_obj_t *separator = lv_obj_create(parent);
    lv_obj_remove_style_all(separator);
    lv_obj_set_size(separator, 1, 32);
    lv_obj_align(separator, LV_ALIGN_TOP_LEFT, x, 9);
    lv_obj_set_style_bg_color(separator, TOP_COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(separator, LV_OPA_COVER, LV_PART_MAIN);
    return separator;
}

static top_status_chip_t create_status_chip(
    lv_obj_t *parent,
    int x,
    int width,
    const char *initial_text)
{
    top_status_chip_t chip = {0};

    chip.root = lv_obj_create(parent);
    lv_obj_set_size(chip.root, width, 26);
    lv_obj_align(chip.root, LV_ALIGN_TOP_LEFT, x, 12);
    lv_obj_clear_flag(chip.root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(chip.root, TOP_COLOR_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(chip.root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(chip.root, 8, LV_PART_MAIN);
    lv_obj_set_style_border_width(chip.root, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(chip.root, TOP_COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(chip.root, 0, LV_PART_MAIN);

    chip.label = lv_label_create(chip.root);
    lv_label_set_text(chip.label, initial_text);
    lv_obj_set_style_text_font(chip.label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(chip.label, TOP_COLOR_DIM, LV_PART_MAIN);
    lv_obj_center(chip.label);

    chip.last_state = -1;
    return chip;
}

static void update_status_chip(
    top_status_chip_t *chip,
    int state,
    const char *text,
    lv_color_t color)
{
    if (chip == NULL || chip->root == NULL || chip->label == NULL) {
        return;
    }

    if (chip->last_state == state) {
        return;
    }

    chip->last_state = state;
    set_label_text_if_changed(chip->label, text);
    lv_obj_set_style_text_color(chip->label, color, LV_PART_MAIN);
    lv_obj_set_style_border_color(chip->root, color, LV_PART_MAIN);
}

static lv_obj_t *find_legacy_bar(lv_obj_t *screen, bool top_bar)
{
    if (screen == NULL) {
        return NULL;
    }

    lv_obj_update_layout(screen);

    const uint32_t count = lv_obj_get_child_cnt(screen);
    for (uint32_t i = 0U; i < count; i++) {
        lv_obj_t *child = lv_obj_get_child(screen, (int32_t)i);
        if (child == NULL) {
            continue;
        }

        const int width = lv_obj_get_width(child);
        const int height = lv_obj_get_height(child);
        const int y = lv_obj_get_y(child);

        if (abs(width - TOP_BAR_WIDTH) > 4 || abs(height - TOP_BAR_HEIGHT) > 4) {
            continue;
        }

        if ((top_bar && y < 100) || (!top_bar && y > 300)) {
            return child;
        }
    }

    return NULL;
}

static lv_obj_t *find_legacy_page_label(lv_obj_t *legacy_header)
{
    if (legacy_header == NULL) {
        return NULL;
    }

    const uint32_t count = lv_obj_get_child_cnt(legacy_header);
    for (uint32_t i = 0U; i < count; i++) {
        lv_obj_t *child = lv_obj_get_child(legacy_header, (int32_t)i);
        if (child == NULL) {
            continue;
        }

        const char *text = lv_label_get_text(child);
        if (text != NULL && strcmp(text, "PULPIT") == 0) {
            return child;
        }
    }

    return NULL;
}

static const char *legacy_page_title(void)
{
    if (s_bar.legacy_page_label == NULL) {
        return "PULPIT";
    }

    const char *text = lv_label_get_text(s_bar.legacy_page_label);
    return text != NULL && text[0] != '\0' ? text : "PULPIT";
}

static void refresh_page_title(void)
{
    const char *title = s_bar.page_override
        ? s_bar.page_override_text
        : legacy_page_title();

    set_label_text_if_changed(s_bar.page_label, title);
}

static void bring_to_foreground(void)
{
    if (s_bar.screen == NULL || s_bar.root == NULL) {
        return;
    }

    const uint32_t count = lv_obj_get_child_cnt(s_bar.screen);
    if (count == 0U) {
        return;
    }

    lv_obj_t *last_child = lv_obj_get_child(
        s_bar.screen,
        (int32_t)(count - 1U)
    );

    if (last_child != s_bar.root) {
        lv_obj_move_foreground(s_bar.root);
    }
}

static void refresh_clock(void)
{
    clock_service_snapshot_t clock;
    clock_service_get_snapshot(&clock);

    char time_text[8];
    char date_text[16];
    char detail_text[32];
    lv_color_t detail_color = TOP_COLOR_YELLOW;

    if (!clock.system_time_valid) {
        snprintf(time_text, sizeof(time_text), "--:--");
        snprintf(date_text, sizeof(date_text), "--.--.----");
        snprintf(detail_text, sizeof(detail_text), "Czas niedostepny");
    } else {
        snprintf(
            time_text,
            sizeof(time_text),
            "%02u:%02u",
            (unsigned int)clock.local_datetime.hour,
            (unsigned int)clock.local_datetime.minute
        );
        snprintf(
            date_text,
            sizeof(date_text),
            "%02u.%02u.%04u",
            (unsigned int)clock.local_datetime.day,
            (unsigned int)clock.local_datetime.month,
            (unsigned int)clock.local_datetime.year
        );

        const char *source = "CZAS";
        switch (clock.source) {
            case CLOCK_TIME_SOURCE_NTP:
                source = "NTP";
                detail_color = TOP_COLOR_GREEN;
                break;
            case CLOCK_TIME_SOURCE_RTC:
                source = "RTC";
                detail_color = TOP_COLOR_BLUE;
                break;
            case CLOCK_TIME_SOURCE_MANUAL:
                source = "MAN";
                detail_color = TOP_COLOR_YELLOW;
                break;
            case CLOCK_TIME_SOURCE_NONE:
            default:
                source = "CZAS";
                detail_color = TOP_COLOR_YELLOW;
                break;
        }

        const uint8_t weekday = clock.local_datetime.weekday;
        const char *weekday_name = weekday < 7U
            ? WEEKDAY_NAMES[weekday]
            : "--";

        snprintf(
            detail_text,
            sizeof(detail_text),
            "%s  |  %s",
            weekday_name,
            source
        );
    }

    set_label_text_if_changed(s_bar.time_label, time_text);
    set_label_text_if_changed(s_bar.date_label, date_text);
    set_label_text_if_changed(s_bar.day_source_label, detail_text);
    lv_obj_set_style_text_color(
        s_bar.day_source_label,
        detail_color,
        LV_PART_MAIN
    );
}

static void refresh_wifi(void)
{
    wifi_service_snapshot_t wifi;
    wifi_service_get_snapshot(&wifi);

    if (wifi.connected) {
        update_status_chip(
            &s_bar.wifi_chip,
            2,
            LV_SYMBOL_WIFI " Wi-Fi",
            TOP_COLOR_GREEN
        );
    } else if (wifi.connecting || wifi.scanning) {
        update_status_chip(
            &s_bar.wifi_chip,
            1,
            LV_SYMBOL_WIFI " LACZY",
            TOP_COLOR_BLUE
        );
    } else if (wifi.started && (!wifi.radio_ready || wifi.last_error != ESP_OK)) {
        update_status_chip(
            &s_bar.wifi_chip,
            3,
            LV_SYMBOL_WIFI " BLAD",
            TOP_COLOR_RED
        );
    } else {
        update_status_chip(
            &s_bar.wifi_chip,
            0,
            LV_SYMBOL_WIFI " OFF",
            TOP_COLOR_DIM
        );
    }
}

static void refresh_mqtt(void)
{
    switch (s_mqtt_state) {
        case TOP_STATUS_MQTT_CONNECTING:
            update_status_chip(
                &s_bar.mqtt_chip,
                TOP_STATUS_MQTT_CONNECTING,
                "MQTT LACZY",
                TOP_COLOR_BLUE
            );
            break;
        case TOP_STATUS_MQTT_ONLINE:
            update_status_chip(
                &s_bar.mqtt_chip,
                TOP_STATUS_MQTT_ONLINE,
                "MQTT",
                TOP_COLOR_GREEN
            );
            break;
        case TOP_STATUS_MQTT_ERROR:
            update_status_chip(
                &s_bar.mqtt_chip,
                TOP_STATUS_MQTT_ERROR,
                "MQTT BLAD",
                TOP_COLOR_RED
            );
            break;
        case TOP_STATUS_MQTT_DISABLED:
        default:
            update_status_chip(
                &s_bar.mqtt_chip,
                TOP_STATUS_MQTT_DISABLED,
                "MQTT OFF",
                TOP_COLOR_DIM
            );
            break;
    }
}

static void refresh_rs485(void)
{
    analog_module_service_snapshot_t module;
    analog_module_service_get_snapshot(&module);

    if (!module.started || !module.hardware_enabled) {
        update_status_chip(
            &s_bar.rs485_chip,
            0,
            "RS485 OFF",
            TOP_COLOR_DIM
        );
    } else if (module.online) {
        update_status_chip(
            &s_bar.rs485_chip,
            2,
            "RS485",
            TOP_COLOR_GREEN
        );
    } else if (module.state == ANALOG_MODULE_STATE_STARTING ||
               module.state == ANALOG_MODULE_STATE_READY) {
        update_status_chip(
            &s_bar.rs485_chip,
            1,
            "RS485 START",
            TOP_COLOR_BLUE
        );
    } else {
        update_status_chip(
            &s_bar.rs485_chip,
            3,
            "RS485 BLAD",
            TOP_COLOR_RED
        );
    }
}

static void refresh_bar(void)
{
    if (s_bar.root == NULL) {
        return;
    }

    bring_to_foreground();
    refresh_page_title();
    refresh_clock();
    refresh_wifi();
    refresh_mqtt();
    refresh_rs485();
}

static void refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    refresh_bar();
}

static void nav_button_event_cb(lv_event_t *event)
{
    top_status_nav_link_t *link = lv_event_get_user_data(event);
    if (link == NULL) {
        return;
    }

    if (link->overlay_page) {
        top_status_bar_set_page_title(link->page_title);
    } else {
        top_status_bar_clear_page_override();
    }

    bring_to_foreground();
    refresh_page_title();
}

static bool attach_navigation_callbacks(lv_obj_t *bottom_bar)
{
    if (bottom_bar == NULL) {
        return false;
    }

    bool attached_any = false;
    const uint32_t button_count = lv_obj_get_child_cnt(bottom_bar);

    for (uint32_t i = 0U; i < button_count; i++) {
        lv_obj_t *button = lv_obj_get_child(bottom_bar, (int32_t)i);
        if (button == NULL) {
            continue;
        }

        const uint32_t child_count = lv_obj_get_child_cnt(button);
        for (uint32_t j = 0U; j < child_count; j++) {
            lv_obj_t *child = lv_obj_get_child(button, (int32_t)j);
            if (child == NULL) {
                continue;
            }

            const char *text = lv_label_get_text(child);
            if (text == NULL) {
                continue;
            }

            for (uint32_t link_index = 0U;
                 link_index < sizeof(s_nav_links) / sizeof(s_nav_links[0]);
                 link_index++) {
                if (strcmp(text, s_nav_links[link_index].nav_label) != 0) {
                    continue;
                }

                lv_obj_add_event_cb(
                    button,
                    nav_button_event_cb,
                    LV_EVENT_RELEASED,
                    &s_nav_links[link_index]
                );
                attached_any = true;
                break;
            }
        }
    }

    return attached_any;
}

static void build_bar(lv_obj_t *screen)
{
    s_bar.root = lv_obj_create(screen);
    lv_obj_set_size(s_bar.root, TOP_BAR_WIDTH, TOP_BAR_HEIGHT);
    lv_obj_align(s_bar.root, LV_ALIGN_TOP_MID, 0, TOP_BAR_Y);
    lv_obj_clear_flag(s_bar.root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_bar.root, TOP_COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_bar.root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_bar.root, 10, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_bar.root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_bar.root, 0, LV_PART_MAIN);

    lv_obj_t *drop = create_text_label(
        s_bar.root,
        LV_SYMBOL_TINT,
        &lv_font_montserrat_20,
        TOP_COLOR_BLUE,
        13,
        14
    );
    lv_obj_clear_flag(drop, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *brand = create_text_label(
        s_bar.root,
        "#FFFFFF SmartTank# #2EA8FF HMI#",
        &lv_font_montserrat_14,
        TOP_COLOR_TEXT,
        40,
        6
    );
    lv_label_set_recolor(brand, true);

    s_bar.page_label = create_text_label(
        s_bar.root,
        "PULPIT",
        &lv_font_montserrat_12,
        TOP_COLOR_BLUE,
        40,
        27
    );

    create_separator(s_bar.root, 205);

    s_bar.time_label = create_text_label(
        s_bar.root,
        "--:--",
        &lv_font_montserrat_20,
        TOP_COLOR_TEXT,
        220,
        5
    );
    lv_obj_set_width(s_bar.time_label, 72);
    lv_obj_set_style_text_align(
        s_bar.time_label,
        LV_TEXT_ALIGN_CENTER,
        LV_PART_MAIN
    );

    create_separator(s_bar.root, 298);

    s_bar.date_label = create_text_label(
        s_bar.root,
        "--.--.----",
        &lv_font_montserrat_12,
        TOP_COLOR_TEXT,
        311,
        6
    );

    s_bar.day_source_label = create_text_label(
        s_bar.root,
        "Czas niedostepny",
        &lv_font_montserrat_12,
        TOP_COLOR_YELLOW,
        311,
        26
    );
    lv_obj_set_width(s_bar.day_source_label, 142);

    s_bar.wifi_chip = create_status_chip(
        s_bar.root,
        460,
        88,
        LV_SYMBOL_WIFI " OFF"
    );
    s_bar.mqtt_chip = create_status_chip(
        s_bar.root,
        554,
        88,
        "MQTT OFF"
    );
    s_bar.rs485_chip = create_status_chip(
        s_bar.root,
        648,
        100,
        "RS485 OFF"
    );
}

bool top_status_bar_attach(lv_obj_t *screen)
{
    if (screen == NULL) {
        return false;
    }

    if (s_bar.root != NULL) {
        return s_bar.screen == screen;
    }

    memset(&s_bar, 0, sizeof(s_bar));
    s_bar.screen = screen;
    s_bar.legacy_header = find_legacy_bar(screen, true);
    lv_obj_t *bottom_bar = find_legacy_bar(screen, false);

    if (s_bar.legacy_header == NULL || bottom_bar == NULL) {
        memset(&s_bar, 0, sizeof(s_bar));
        return false;
    }

    s_bar.legacy_page_label = find_legacy_page_label(s_bar.legacy_header);
    if (s_bar.legacy_page_label == NULL) {
        memset(&s_bar, 0, sizeof(s_bar));
        return false;
    }

    build_bar(screen);
    lv_obj_add_flag(s_bar.legacy_header, LV_OBJ_FLAG_HIDDEN);

    if (!attach_navigation_callbacks(bottom_bar)) {
        lv_obj_del(s_bar.root);
        lv_obj_clear_flag(s_bar.legacy_header, LV_OBJ_FLAG_HIDDEN);
        memset(&s_bar, 0, sizeof(s_bar));
        return false;
    }

    s_bar.timer = lv_timer_create(
        refresh_timer_cb,
        TOP_BAR_REFRESH_MS,
        NULL
    );

    refresh_bar();
    return true;
}

void top_status_bar_set_mqtt_state(top_status_mqtt_state_t state)
{
    if (state < TOP_STATUS_MQTT_DISABLED || state > TOP_STATUS_MQTT_ERROR) {
        state = TOP_STATUS_MQTT_ERROR;
    }

    s_mqtt_state = state;
}

void top_status_bar_set_page_title(const char *title)
{
    if (title == NULL || title[0] == '\0') {
        return;
    }

    snprintf(
        s_bar.page_override_text,
        sizeof(s_bar.page_override_text),
        "%s",
        title
    );
    s_bar.page_override = true;
}

void top_status_bar_clear_page_override(void)
{
    s_bar.page_override = false;
    s_bar.page_override_text[0] = '\0';
}
