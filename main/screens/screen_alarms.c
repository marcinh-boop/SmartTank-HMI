#include "screen_alarms.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "alarm_service.h"
#include "theme.h"

#define ALARM_BG          lv_color_hex(0x06111B)
#define ALARM_PANEL       lv_color_hex(0x0B1825)
#define ALARM_ROW         lv_color_hex(0x091621)
#define ALARM_BORDER      lv_color_hex(0x24384A)
#define ALARM_BLUE        lv_color_hex(0x2EA8FF)
#define ALARM_GREEN       lv_color_hex(0x39D12F)
#define ALARM_YELLOW      lv_color_hex(0xFFC247)
#define ALARM_RED         lv_color_hex(0xFF4D4D)
#define ALARM_BUTTON_BG   lv_color_hex(0x12314A)
#define ALARM_EPOCH_MIN   1704067200LL

#define COL_TIME_WIDTH      108
#define COL_TITLE_WIDTH     190
#define COL_EVENT_WIDTH     112
#define COL_SEVERITY_WIDTH  102
#define COL_MESSAGE_WIDTH   236

static lv_obj_t *s_root;
static lv_obj_t *s_summary;
static lv_obj_t *s_ack_all_button;
static lv_obj_t *s_table;
static lv_obj_t *s_empty_label;
static lv_timer_t *s_refresh_timer;
static uint32_t s_last_service_revision;
static uint32_t s_last_event_revision;
static alarm_service_snapshot_t s_service_snapshot;
static alarm_event_log_snapshot_t s_event_snapshot;

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

static void acknowledge_all_cb(lv_event_t *event)
{
    (void)event;
    (void)alarm_service_acknowledge_all();
}

static void format_event_time(
    const alarm_event_t *event,
    char *buffer,
    size_t buffer_size)
{
    if (event == NULL || buffer == NULL || buffer_size == 0U) {
        return;
    }

    if (event->epoch >= ALARM_EPOCH_MIN) {
        const time_t event_time = (time_t)event->epoch;
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

    snprintf(buffer, buffer_size, "T+%lu s", (unsigned long)event->uptime);
}

static void copy_short_text(
    char *destination,
    size_t destination_size,
    const char *source,
    size_t maximum_characters)
{
    if (destination == NULL || destination_size == 0U) {
        return;
    }

    if (source == NULL) {
        destination[0] = '\0';
        return;
    }

    const size_t source_length = strnlen(source, maximum_characters + 1U);
    if (source_length <= maximum_characters) {
        snprintf(destination, destination_size, "%s", source);
        return;
    }

    const size_t copy_length = maximum_characters > 3U
        ? maximum_characters - 3U
        : 0U;
    snprintf(
        destination,
        destination_size,
        "%.*s...",
        (int)copy_length,
        source
    );
}

static void set_header_label(
    lv_obj_t *parent,
    const char *text,
    int x,
    int width)
{
    lv_obj_t *label = create_label(
        parent,
        text,
        ST_COLOR_TEXT_DIM,
        &lv_font_montserrat_12
    );
    lv_obj_set_pos(label, x + 6, 7);
    lv_obj_set_width(label, width - 12);
}

static void build_table_header(lv_obj_t *parent)
{
    lv_obj_t *header = lv_obj_create(parent);
    lv_obj_set_size(header, 760, 28);
    lv_obj_set_pos(header, 20, 122);
    style_panel(header, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x102334), LV_PART_MAIN);
    lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);

    int x = 0;
    set_header_label(header, "CZAS", x, COL_TIME_WIDTH);
    x += COL_TIME_WIDTH;
    set_header_label(header, "ALARM", x, COL_TITLE_WIDTH);
    x += COL_TITLE_WIDTH;
    set_header_label(header, "ZDARZENIE", x, COL_EVENT_WIDTH);
    x += COL_EVENT_WIDTH;
    set_header_label(header, "PRIORYTET", x, COL_SEVERITY_WIDTH);
    x += COL_SEVERITY_WIDTH;
    set_header_label(header, "OPIS", x, COL_MESSAGE_WIDTH);
}

static void build_event_table(lv_obj_t *parent)
{
    s_table = lv_table_create(parent);
    lv_obj_set_size(s_table, 760, 230);
    lv_obj_set_pos(s_table, 20, 150);
    lv_table_set_col_cnt(s_table, 5);
    lv_table_set_row_cnt(s_table, 1);
    lv_table_set_col_width(s_table, 0, COL_TIME_WIDTH);
    lv_table_set_col_width(s_table, 1, COL_TITLE_WIDTH);
    lv_table_set_col_width(s_table, 2, COL_EVENT_WIDTH);
    lv_table_set_col_width(s_table, 3, COL_SEVERITY_WIDTH);
    lv_table_set_col_width(s_table, 4, COL_MESSAGE_WIDTH);

    lv_obj_set_style_bg_color(s_table, ALARM_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_table, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_table, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_table, ALARM_BORDER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_table, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(s_table, 0, LV_PART_MAIN);

    lv_obj_set_style_bg_color(s_table, ALARM_ROW, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(s_table, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_border_width(s_table, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_color(s_table, ALARM_BORDER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(s_table, ST_COLOR_TEXT, LV_PART_ITEMS);
    lv_obj_set_style_text_font(s_table, &lv_font_montserrat_12, LV_PART_ITEMS);
    lv_obj_set_style_pad_left(s_table, 6, LV_PART_ITEMS);
    lv_obj_set_style_pad_right(s_table, 6, LV_PART_ITEMS);
    lv_obj_set_style_pad_top(s_table, 7, LV_PART_ITEMS);
    lv_obj_set_style_pad_bottom(s_table, 7, LV_PART_ITEMS);

    lv_obj_set_scroll_dir(s_table, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_table, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_clear_flag(s_table, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(s_table, LV_OBJ_FLAG_SCROLL_MOMENTUM);

    s_empty_label = create_label(
        parent,
        "BRAK ZAREJESTROWANYCH ZDARZEN",
        ALARM_GREEN,
        &lv_font_montserrat_16
    );
    lv_obj_align_to(s_empty_label, s_table, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(s_empty_label, LV_OBJ_FLAG_CLICKABLE);
}

static void update_table(void)
{
    const uint16_t row_count = s_event_snapshot.count > 0U
        ? s_event_snapshot.count
        : 1U;
    lv_table_set_row_cnt(s_table, row_count);

    if (s_event_snapshot.count == 0U) {
        for (uint8_t column = 0U; column < 5U; column++) {
            lv_table_set_cell_value(s_table, 0U, column, "");
        }
        lv_obj_clear_flag(s_empty_label, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_add_flag(s_empty_label, LV_OBJ_FLAG_HIDDEN);

    for (uint8_t index = 0U; index < s_event_snapshot.count; index++) {
        const alarm_event_t *event = &s_event_snapshot.events[index];
        char time_text[24];
        char title_text[32];
        char message_text[40];

        format_event_time(event, time_text, sizeof(time_text));
        copy_short_text(title_text, sizeof(title_text), event->title, 25U);
        copy_short_text(message_text, sizeof(message_text), event->message, 34U);

        lv_table_set_cell_value(s_table, index, 0U, time_text);
        lv_table_set_cell_value(s_table, index, 1U, title_text);
        lv_table_set_cell_value(
            s_table,
            index,
            2U,
            alarm_service_event_name(event->type)
        );
        lv_table_set_cell_value(
            s_table,
            index,
            3U,
            alarm_service_severity_name(event->severity)
        );
        lv_table_set_cell_value(s_table, index, 4U, message_text);
    }
}

static void refresh_screen(bool force)
{
    alarm_service_get_snapshot(&s_service_snapshot);
    alarm_service_get_event_log(&s_event_snapshot);

    if (!force &&
        s_service_snapshot.revision == s_last_service_revision &&
        s_event_snapshot.revision == s_last_event_revision) {
        return;
    }

    const bool events_changed = force ||
        s_event_snapshot.revision != s_last_event_revision;

    s_last_service_revision = s_service_snapshot.revision;
    s_last_event_revision = s_event_snapshot.revision;

    char summary[128];
    snprintf(
        summary,
        sizeof(summary),
        "AKTYWNE: %u   NOWE: %u   ZDARZENIA: %lu   RAM: %u/%u",
        (unsigned int)s_service_snapshot.active_count,
        (unsigned int)s_service_snapshot.unacknowledged_count,
        (unsigned long)s_event_snapshot.total_count,
        (unsigned int)s_event_snapshot.count,
        (unsigned int)ALARM_EVENT_LOG_CAPACITY
    );
    lv_label_set_text(s_summary, summary);
    lv_obj_set_style_text_color(
        s_summary,
        s_service_snapshot.unacknowledged_count > 0U
            ? ALARM_RED
            : (s_service_snapshot.active_count > 0U ? ALARM_YELLOW : ALARM_GREEN),
        LV_PART_MAIN
    );

    if (s_service_snapshot.unacknowledged_count == 0U) {
        lv_obj_add_state(s_ack_all_button, LV_STATE_DISABLED);
    } else {
        lv_obj_clear_state(s_ack_all_button, LV_STATE_DISABLED);
    }

    if (events_changed) {
        update_table();
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
        "DZIENNIK ALARMOW",
        ALARM_RED,
        &lv_font_montserrat_14
    );
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *status = create_label(
        top,
        "AKTYWACJA / POTWIERDZENIE / USTAPIENIE",
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
        "AKTYWNE: 0   NOWE: 0   ZDARZENIA: 0   RAM: 0/32",
        ALARM_GREEN,
        &lv_font_montserrat_14
    );
    lv_obj_align(s_summary, LV_ALIGN_LEFT_MID, 0, 0);

    s_ack_all_button = create_button(
        summary_panel,
        "POTWIERDZ WSZYSTKIE",
        160,
        32
    );
    lv_obj_align(s_ack_all_button, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(
        s_ack_all_button,
        acknowledge_all_cb,
        LV_EVENT_RELEASED,
        NULL
    );

    build_table_header(s_root);
    build_event_table(s_root);

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
