#include "screen_alarms.h"

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "alarm_service.h"
#include "theme.h"

#define ALARM_BG          lv_color_hex(0x06111B)
#define ALARM_PANEL       lv_color_hex(0x0B1825)
#define ALARM_BORDER      lv_color_hex(0x24384A)
#define ALARM_BLUE        lv_color_hex(0x2EA8FF)
#define ALARM_GREEN       lv_color_hex(0x39D12F)
#define ALARM_YELLOW      lv_color_hex(0xFFC247)
#define ALARM_RED         lv_color_hex(0xFF4D4D)
#define ALARM_BUTTON_BG   lv_color_hex(0x12314A)
#define ALARM_EPOCH_MIN   1704067200LL

#define ALARM_CARD_WIDTH  370
#define ALARM_CARD_HEIGHT 112

typedef struct {
    lv_obj_t *card;
    lv_obj_t *title;
    lv_obj_t *severity;
    lv_obj_t *message;
    lv_obj_t *state;
    lv_obj_t *time;
    lv_obj_t *ack_button;
} alarm_card_view_t;

static lv_obj_t *s_root;
static lv_obj_t *s_summary;
static lv_obj_t *s_empty_label;
static lv_timer_t *s_refresh_timer;
static uint32_t s_last_revision;
static alarm_card_view_t s_cards[ALARM_SERVICE_ITEM_COUNT];
static alarm_id_t s_card_ids[ALARM_SERVICE_ITEM_COUNT] = {
    ALARM_ID_TANK_WARNING,
    ALARM_ID_TANK_CRITICAL,
    ALARM_ID_TANK_SENSOR,
    ALARM_ID_MODBUS_COMMUNICATION,
};

static lv_obj_t *create_label(
    lv_obj_t *parent,
    const char *text,
    lv_color_t color,
    const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    return label;
}

static void style_panel(lv_obj_t *panel, int radius)
{
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(panel, ALARM_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, ALARM_BORDER, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, radius, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(panel, 0, LV_PART_MAIN);
}

static lv_obj_t *create_button(
    lv_obj_t *parent,
    const char *text,
    int width,
    int height)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_size(button, width, height);
    lv_obj_set_style_bg_color(button, ALARM_BUTTON_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(button, ALARM_BLUE, LV_PART_MAIN);
    lv_obj_set_style_radius(button, 7, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);

    lv_obj_t *label = create_label(
        button,
        text,
        ST_COLOR_TEXT,
        &lv_font_montserrat_12
    );
    lv_obj_center(label);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    return button;
}

static lv_color_t severity_color(alarm_severity_t severity)
{
    return severity == ALARM_SEVERITY_CRITICAL ? ALARM_RED : ALARM_YELLOW;
}

static void format_alarm_time(
    const alarm_item_t *item,
    char *buffer,
    size_t buffer_size)
{
    if (item == NULL || buffer == NULL || buffer_size == 0U) {
        return;
    }

    const int64_t epoch = item->active ? item->active_since_epoch : item->cleared_epoch;
    const uint32_t uptime = item->active
        ? item->active_since_uptime
        : item->cleared_uptime;

    if (epoch >= ALARM_EPOCH_MIN) {
        const time_t event_time = (time_t)epoch;
        struct tm local_time = {0};
        if (localtime_r(&event_time, &local_time) != NULL) {
            snprintf(
                buffer,
                buffer_size,
                "%02d.%02d %02d:%02d:%02d",
                local_time.tm_mday,
                local_time.tm_mon + 1,
                local_time.tm_hour,
                local_time.tm_min,
                local_time.tm_sec
            );
            return;
        }
    }

    snprintf(buffer, buffer_size, "T+%lu s", (unsigned long)uptime);
}

static void acknowledge_cb(lv_event_t *event)
{
    alarm_id_t *id = lv_event_get_user_data(event);
    if (id == NULL) {
        return;
    }

    (void)alarm_service_acknowledge(*id);
}

static void acknowledge_all_cb(lv_event_t *event)
{
    (void)event;
    (void)alarm_service_acknowledge_all();
}

static alarm_card_view_t create_alarm_card(
    lv_obj_t *parent,
    uint8_t index,
    int x,
    int y)
{
    alarm_card_view_t view = {0};

    view.card = lv_obj_create(parent);
    lv_obj_set_size(view.card, ALARM_CARD_WIDTH, ALARM_CARD_HEIGHT);
    lv_obj_set_pos(view.card, x, y);
    style_panel(view.card, 10);
    lv_obj_set_style_pad_all(view.card, 10, LV_PART_MAIN);

    view.title = create_label(
        view.card,
        "--",
        ST_COLOR_TEXT,
        &lv_font_montserrat_14
    );
    lv_obj_align(view.title, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_width(view.title, 220);
    lv_label_set_long_mode(view.title, LV_LABEL_LONG_DOT);

    view.severity = create_label(
        view.card,
        "--",
        ALARM_YELLOW,
        &lv_font_montserrat_12
    );
    lv_obj_align(view.severity, LV_ALIGN_TOP_RIGHT, 0, 1);

    view.message = create_label(
        view.card,
        "--",
        ST_COLOR_TEXT_DIM,
        &lv_font_montserrat_12
    );
    lv_obj_align(view.message, LV_ALIGN_TOP_LEFT, 0, 25);
    lv_obj_set_width(view.message, 340);
    lv_label_set_long_mode(view.message, LV_LABEL_LONG_DOT);

    view.state = create_label(
        view.card,
        "--",
        ALARM_YELLOW,
        &lv_font_montserrat_12
    );
    lv_obj_align(view.state, LV_ALIGN_TOP_LEFT, 0, 52);

    view.time = create_label(
        view.card,
        "--",
        ST_COLOR_TEXT_DIM,
        &lv_font_montserrat_12
    );
    lv_obj_align(view.time, LV_ALIGN_TOP_LEFT, 0, 76);

    view.ack_button = create_button(view.card, "POTWIERDZ", 100, 30);
    lv_obj_align(view.ack_button, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_add_event_cb(
        view.ack_button,
        acknowledge_cb,
        LV_EVENT_RELEASED,
        &s_card_ids[index]
    );

    lv_obj_add_flag(view.card, LV_OBJ_FLAG_HIDDEN);
    return view;
}

static void update_card(
    alarm_card_view_t *view,
    const alarm_item_t *item)
{
    if (view == NULL || item == NULL) {
        return;
    }

    if (item->occurrence_count == 0U && !item->active) {
        lv_obj_add_flag(view->card, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_clear_flag(view->card, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(view->title, item->title);
    lv_label_set_text(
        view->severity,
        alarm_service_severity_name(item->severity)
    );
    lv_obj_set_style_text_color(
        view->severity,
        severity_color(item->severity),
        LV_PART_MAIN
    );
    lv_label_set_text(view->message, item->message);

    if (item->active) {
        lv_label_set_text(
            view->state,
            item->acknowledged ? "AKTYWNY / POTWIERDZONY" : "AKTYWNY / NOWY"
        );
        lv_obj_set_style_text_color(
            view->state,
            item->acknowledged ? ALARM_BLUE : severity_color(item->severity),
            LV_PART_MAIN
        );

        if (item->acknowledged) {
            lv_obj_add_flag(view->ack_button, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(view->ack_button, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        lv_label_set_text(view->state, "USTAPIL");
        lv_obj_set_style_text_color(view->state, ALARM_GREEN, LV_PART_MAIN);
        lv_obj_add_flag(view->ack_button, LV_OBJ_FLAG_HIDDEN);
    }

    char time_buffer[32];
    format_alarm_time(item, time_buffer, sizeof(time_buffer));
    lv_label_set_text(view->time, time_buffer);
}

static void refresh_screen(bool force)
{
    alarm_service_snapshot_t snapshot;
    alarm_service_get_snapshot(&snapshot);

    if (!force && snapshot.revision == s_last_revision) {
        return;
    }
    s_last_revision = snapshot.revision;

    char summary[96];
    snprintf(
        summary,
        sizeof(summary),
        "AKTYWNE: %u    NOWE: %u    ZAPIS: RAM",
        (unsigned int)snapshot.active_count,
        (unsigned int)snapshot.unacknowledged_count
    );
    lv_label_set_text(s_summary, summary);
    lv_obj_set_style_text_color(
        s_summary,
        snapshot.unacknowledged_count > 0U
            ? ALARM_RED
            : (snapshot.active_count > 0U ? ALARM_YELLOW : ALARM_GREEN),
        LV_PART_MAIN
    );

    uint8_t visible_count = 0U;
    for (uint8_t index = 0U; index < ALARM_SERVICE_ITEM_COUNT; index++) {
        update_card(&s_cards[index], &snapshot.items[index]);
        if (snapshot.items[index].occurrence_count > 0U ||
            snapshot.items[index].active) {
            visible_count++;
        }
    }

    if (visible_count == 0U) {
        lv_obj_clear_flag(s_empty_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_empty_label, LV_OBJ_FLAG_HIDDEN);
    }
}

static void refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (s_root != NULL && !lv_obj_has_flag(s_root, LV_OBJ_FLAG_HIDDEN)) {
        refresh_screen(false);
    }
}

static void build_screen(lv_obj_t *parent_screen)
{
    s_root = lv_obj_create(parent_screen);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, 800, 398);
    lv_obj_align(s_root, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_root, ALARM_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *top = lv_obj_create(s_root);
    lv_obj_set_size(top, 760, 50);
    lv_obj_align(top, LV_ALIGN_TOP_MID, 0, 8);
    style_panel(top, 10);
    lv_obj_set_style_border_width(top, 0, LV_PART_MAIN);

    lv_obj_t *brand = create_label(
        top,
        "SmartTank HMI",
        ST_COLOR_TEXT,
        &lv_font_montserrat_14
    );
    lv_obj_align(brand, LV_ALIGN_LEFT_MID, 12, 0);

    lv_obj_t *title = create_label(
        top,
        "CENTRUM ALARMOW",
        ALARM_RED,
        &lv_font_montserrat_14
    );
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *status = create_label(
        top,
        "POZIOM / CZUJNIK / MODBUS",
        ST_COLOR_TEXT_DIM,
        &lv_font_montserrat_12
    );
    lv_obj_align(status, LV_ALIGN_RIGHT_MID, -12, 0);

    lv_obj_t *summary_panel = lv_obj_create(s_root);
    lv_obj_set_size(summary_panel, 760, 48);
    lv_obj_align(summary_panel, LV_ALIGN_TOP_MID, 0, 68);
    style_panel(summary_panel, 9);
    lv_obj_set_style_pad_left(summary_panel, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_right(summary_panel, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_top(summary_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(summary_panel, 0, LV_PART_MAIN);

    s_summary = create_label(
        summary_panel,
        "AKTYWNE: 0    NOWE: 0    ZAPIS: RAM",
        ALARM_GREEN,
        &lv_font_montserrat_14
    );
    lv_obj_align(s_summary, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *ack_all = create_button(summary_panel, "POTWIERDZ WSZYSTKIE", 160, 32);
    lv_obj_align(ack_all, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(
        ack_all,
        acknowledge_all_cb,
        LV_EVENT_RELEASED,
        NULL
    );

    s_cards[0] = create_alarm_card(s_root, 0U, 20, 124);
    s_cards[1] = create_alarm_card(s_root, 1U, 410, 124);
    s_cards[2] = create_alarm_card(s_root, 2U, 20, 242);
    s_cards[3] = create_alarm_card(s_root, 3U, 410, 242);

    s_empty_label = create_label(
        s_root,
        "BRAK ZAREJESTROWANYCH ALARMOW",
        ALARM_GREEN,
        &lv_font_montserrat_20
    );
    lv_obj_align(s_empty_label, LV_ALIGN_CENTER, 0, 72);

    s_refresh_timer = lv_timer_create(refresh_timer_cb, 500, NULL);
    refresh_screen(true);
}

void screen_alarms_open(lv_obj_t *parent_screen)
{
    if (parent_screen == NULL) {
        return;
    }

    if (s_root == NULL) {
        build_screen(parent_screen);
    }

    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_root);
    refresh_screen(true);
}

void screen_alarms_hide(void)
{
    if (s_root != NULL) {
        lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    }
}
