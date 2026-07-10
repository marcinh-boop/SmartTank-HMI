#include "screen_dashboard.h"

#include <stdint.h>
#include <stdio.h>

#include "app_model.h"
#include "clock_service.h"
#include "measurement_history.h"
#include "screen_tank_calibration.h"
#include "screen_tank_detail.h"
#include "theme.h"
#include "lvgl.h"

#include "tank_widget.h"
#include "well_widget.h"
#include "weather_widget.h"
#include "bottom_nav.h"

typedef struct {
    lv_obj_t *title_label;
    lv_obj_t *range_label;
    lv_obj_t *stats_label;
    lv_obj_t *chart;
    lv_chart_series_t *series;
} history_chart_view_t;

static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_dashboard_content = NULL;
static lv_obj_t *s_history_content = NULL;
static lv_obj_t *s_tank_detail_content = NULL;
static lv_obj_t *s_tank_calibration_content = NULL;
static lv_obj_t *s_page_label = NULL;
static lv_obj_t *s_source_label = NULL;
static bottom_nav_t *s_nav = NULL;
static lv_timer_t *s_refresh_timer = NULL;
static uint32_t s_last_revision = 0;
static uint32_t s_last_history_revision = 0;

static tank_widget_t s_tank_widget;
static well_widget_t s_well_widget;
static weather_widget_t s_weather_widget;
static history_chart_view_t s_tank_history;
static history_chart_view_t s_well_history;

static void refresh_dashboard_from_model(bool force);
static void show_page(bottom_nav_page_t page);
static void show_tank_detail(void);
static void show_tank_calibration(void);

static lv_obj_t *create_bar(lv_obj_t *screen, lv_align_t align, int y)
{
    lv_obj_t *bar = lv_obj_create(screen);
    lv_obj_set_size(bar, 760, 50);
    lv_obj_align(bar, align, 0, y);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x08131F), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 10, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
    return bar;
}

static lv_obj_t *create_label(
    lv_obj_t *parent,
    const char *text,
    lv_color_t color,
    lv_align_t align,
    int x,
    int y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(label, align, x, y);
    return label;
}

static lv_obj_t *create_content_layer(lv_obj_t *screen)
{
    lv_obj_t *layer = lv_obj_create(screen);
    lv_obj_remove_style_all(layer);
    lv_obj_set_size(layer, 800, 340);
    lv_obj_align(layer, LV_ALIGN_TOP_MID, 0, 58);
    lv_obj_clear_flag(layer, LV_OBJ_FLAG_SCROLLABLE);
    return layer;
}

static lv_obj_t *create_card(lv_obj_t *parent, lv_color_t color)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 245, 330);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(card, ST_COLOR_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 14, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, color, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 14, LV_PART_MAIN);
    return card;
}

static lv_obj_t *create_history_panel(lv_obj_t *parent, int x)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, 370, 330);
    lv_obj_align(panel, LV_ALIGN_TOP_LEFT, x, 10);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(panel, ST_COLOR_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 14, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x24384A), LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 14, LV_PART_MAIN);
    return panel;
}

static history_chart_view_t create_history_chart(
    lv_obj_t *parent,
    const char *title_text,
    lv_color_t color)
{
    history_chart_view_t view = {0};

    view.title_label = create_label(
        parent,
        title_text,
        color,
        LV_ALIGN_TOP_LEFT,
        0,
        0
    );

    view.range_label = create_label(
        parent,
        "24 probki | 1 s",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_TOP_RIGHT,
        0,
        0
    );

    view.stats_label = create_label(
        parent,
        "MIN --   AVG --   MAX --",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_TOP_LEFT,
        0,
        23
    );
    lv_obj_set_style_text_font(view.stats_label, &lv_font_montserrat_12, LV_PART_MAIN);

    view.chart = lv_chart_create(parent);
    lv_obj_set_size(view.chart, 330, 215);
    lv_obj_align(view.chart, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_chart_set_type(view.chart, LV_CHART_TYPE_LINE);
    lv_chart_set_range(view.chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_point_count(view.chart, MEASUREMENT_HISTORY_CAPACITY);
    lv_chart_set_div_line_count(view.chart, 5, 6);
    lv_obj_set_style_bg_color(view.chart, lv_color_hex(0x09131D), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(view.chart, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(view.chart, 0, LV_PART_MAIN);
    lv_obj_set_style_line_color(view.chart, lv_color_hex(0x23384A), LV_PART_MAIN);
    lv_obj_set_style_line_width(view.chart, 1, LV_PART_MAIN);

    view.series = lv_chart_add_series(view.chart, color, LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_value(view.chart, view.series, LV_CHART_POINT_NONE);

    return view;
}

static void tank_card_event_cb(lv_event_t *event)
{
    (void)event;
    show_tank_detail();
}

static void tank_detail_back_cb(void)
{
    show_page(NAV_DASHBOARD);
    bottom_nav_set_active(s_nav, NAV_DASHBOARD);
}

static void tank_detail_calibration_cb(void)
{
    show_tank_calibration();
}

static void tank_calibration_back_cb(void)
{
    show_tank_detail();
}

static void tank_calibration_save_cb(const tank_channel_config_t *config)
{
    app_model_update_tank_config(config);
    refresh_dashboard_from_model(true);
    show_tank_detail();
}

static void build_dashboard_content(void)
{
    s_dashboard_content = create_content_layer(s_screen);

    lv_obj_t *tank_card = create_card(s_dashboard_content, lv_color_hex(0x39D12F));
    lv_obj_align(tank_card, LV_ALIGN_TOP_LEFT, 20, 10);
    lv_obj_add_flag(tank_card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(
        tank_card,
        lv_color_hex(0x10251A),
        LV_PART_MAIN | LV_STATE_PRESSED
    );
    lv_obj_add_event_cb(tank_card, tank_card_event_cb, LV_EVENT_RELEASED, NULL);
    s_tank_widget = tank_widget_create(tank_card);

    lv_obj_t *well_card = create_card(s_dashboard_content, lv_color_hex(0x2EA8FF));
    lv_obj_align(well_card, LV_ALIGN_TOP_MID, 0, 10);
    s_well_widget = well_widget_create(well_card);

    lv_obj_t *weather_card = create_card(s_dashboard_content, lv_color_hex(0xFFC247));
    lv_obj_align(weather_card, LV_ALIGN_TOP_RIGHT, -20, 10);
    s_weather_widget = weather_widget_create(weather_card);
}

static void build_history_content(void)
{
    s_history_content = create_content_layer(s_screen);

    lv_obj_t *tank_panel = create_history_panel(s_history_content, 20);
    s_tank_history = create_history_chart(
        tank_panel,
        "SZAMBO  --%",
        lv_color_hex(0x39D12F)
    );

    lv_obj_t *well_panel = create_history_panel(s_history_content, 410);
    s_well_history = create_history_chart(
        well_panel,
        "STUDNIA  --%",
        lv_color_hex(0x2EA8FF)
    );

    lv_obj_add_flag(s_history_content, LV_OBJ_FLAG_HIDDEN);
}

static void update_history_chart(
    history_chart_view_t *view,
    const measurement_history_snapshot_t *history,
    bool tank_chart)
{
    for (uint32_t i = 0; i < MEASUREMENT_HISTORY_CAPACITY; i++) {
        lv_chart_set_value_by_id(
            view->chart,
            view->series,
            (uint16_t)i,
            LV_CHART_POINT_NONE
        );
    }

    if (history->count == 0U) {
        lv_label_set_text(view->stats_label, "Brak probek");
        lv_chart_refresh(view->chart);
        return;
    }

    int minimum = 101;
    int maximum = -1;
    int sum = 0;
    const uint32_t offset = MEASUREMENT_HISTORY_CAPACITY - history->count;

    for (uint32_t i = 0; i < history->count; i++) {
        const int value = tank_chart
            ? history->samples[i].tank_percent
            : history->samples[i].well_percent;

        lv_chart_set_value_by_id(
            view->chart,
            view->series,
            (uint16_t)(offset + i),
            value
        );

        if (value < minimum) {
            minimum = value;
        }
        if (value > maximum) {
            maximum = value;
        }
        sum += value;
    }

    const int latest = tank_chart
        ? history->samples[history->count - 1U].tank_percent
        : history->samples[history->count - 1U].well_percent;
    const float average = (float)sum / (float)history->count;

    char buffer[64];
    snprintf(
        buffer,
        sizeof(buffer),
        tank_chart ? "SZAMBO  %d%%" : "STUDNIA  %d%%",
        latest
    );
    lv_label_set_text(view->title_label, buffer);

    snprintf(
        buffer,
        sizeof(buffer),
        "MIN %d%%   AVG %.0f%%   MAX %d%%",
        minimum,
        average,
        maximum
    );
    lv_label_set_text(view->stats_label, buffer);
    lv_chart_refresh(view->chart);
}

static void refresh_history_from_model(bool force, const smarttank_state_t *state)
{
    measurement_history_snapshot_t history;
    measurement_history_get_snapshot(&history);

    if (!force && history.revision == s_last_history_revision) {
        return;
    }

    s_last_history_revision = history.revision;

    const char *range_text = state->system.simulation_active
        ? "24 probki | 1 s"
        : "Ostatnie 24 h";
    lv_label_set_text(s_tank_history.range_label, range_text);
    lv_label_set_text(s_well_history.range_label, range_text);

    update_history_chart(&s_tank_history, &history, true);
    update_history_chart(&s_well_history, &history, false);
}

static void refresh_header_status(const smarttank_state_t *state)
{
    const char *source_text;
    if (state->system.simulation_active) {
        source_text = "SYMULACJA | RS485 OFF";
    } else if (state->system.modbus_connected) {
        source_text = "MODBUS RTU | ONLINE";
    } else {
        source_text = "MODBUS RTU | OFFLINE";
    }

    clock_service_snapshot_t clock;
    clock_service_get_snapshot(&clock);

    char buffer[96];
    if (clock.system_time_valid) {
        snprintf(
            buffer,
            sizeof(buffer),
            "%02u.%02u.%04u %02u:%02u\n%s",
            (unsigned int)clock.local_datetime.day,
            (unsigned int)clock.local_datetime.month,
            (unsigned int)clock.local_datetime.year,
            (unsigned int)clock.local_datetime.hour,
            (unsigned int)clock.local_datetime.minute,
            source_text
        );
    } else {
        snprintf(
            buffer,
            sizeof(buffer),
            "--.--.---- --:--\n%s",
            source_text
        );
    }

    lv_label_set_text(s_source_label, buffer);
}

static void refresh_dashboard_from_model(bool force)
{
    smarttank_state_t state;
    app_model_get_snapshot(&state);

    if (force || state.revision != s_last_revision) {
        s_last_revision = state.revision;

        tank_widget_set_data(
            &s_tank_widget,
            state.tank.level_percent,
            state.tank.volume_m3,
            state.tank.capacity_m3,
            state.tank_config.warning_percent,
            state.tank_config.critical_percent
        );

        well_widget_set_data(
            &s_well_widget,
            state.well.water_column_m,
            state.well.well_depth_m
        );

        weather_widget_set_current(
            &s_weather_widget,
            state.weather.temperature_c,
            state.weather.rain_percent,
            state.weather.wind_kmh,
            state.weather.humidity_percent,
            state.weather.description
        );

        screen_tank_detail_update(&state);
        screen_tank_calibration_update_live(&state);
    }

    refresh_header_status(&state);
    refresh_history_from_model(force, &state);
}

static void dashboard_refresh_cb(lv_timer_t *timer)
{
    (void)timer;
    refresh_dashboard_from_model(false);
}

static void show_tank_detail(void)
{
    lv_obj_add_flag(s_dashboard_content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_history_content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_tank_calibration_content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_tank_detail_content, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_page_label, "SZAMBO");
    bottom_nav_set_active(s_nav, NAV_DASHBOARD);
}

static void show_tank_calibration(void)
{
    smarttank_state_t state;
    app_model_get_snapshot(&state);
    screen_tank_calibration_begin(&state);

    lv_obj_add_flag(s_dashboard_content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_history_content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_tank_detail_content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_tank_calibration_content, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_page_label, "KALIBRACJA");
    bottom_nav_set_active(s_nav, NAV_DASHBOARD);
}

static void show_page(bottom_nav_page_t page)
{
    lv_obj_add_flag(s_tank_detail_content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_tank_calibration_content, LV_OBJ_FLAG_HIDDEN);

    if (page == NAV_HISTORY) {
        lv_obj_add_flag(s_dashboard_content, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_history_content, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_page_label, "HISTORIA");
    } else {
        lv_obj_add_flag(s_history_content, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_dashboard_content, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_page_label, "PULPIT");
    }
}

static bool nav_change(bottom_nav_page_t page)
{
    if (page != NAV_DASHBOARD && page != NAV_HISTORY) {
        return false;
    }

    show_page(page);
    return true;
}

static void build_screen(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_screen, ST_COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *top = create_bar(s_screen, LV_ALIGN_TOP_MID, 8);
    create_label(top, "SmartTank HMI", ST_COLOR_TEXT, LV_ALIGN_LEFT_MID, 12, 0);
    s_page_label = create_label(top, "PULPIT", ST_COLOR_ACCENT, LV_ALIGN_CENTER, 0, 0);
    s_source_label = create_label(
        top,
        "--.--.---- --:--\nSYMULACJA | RS485 OFF",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_RIGHT_MID,
        -12,
        0
    );
    lv_obj_set_width(s_source_label, 230);
    lv_obj_set_style_text_font(s_source_label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_align(s_source_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

    build_dashboard_content();
    build_history_content();
    s_tank_detail_content = screen_tank_detail_create(
        s_screen,
        tank_detail_back_cb,
        tank_detail_calibration_cb
    );
    s_tank_calibration_content = screen_tank_calibration_create(
        s_screen,
        tank_calibration_back_cb,
        tank_calibration_save_cb
    );

    lv_obj_t *bottom = create_bar(s_screen, LV_ALIGN_BOTTOM_MID, -8);
    s_nav = bottom_nav_create(bottom, NAV_DASHBOARD, nav_change);

    s_refresh_timer = lv_timer_create(dashboard_refresh_cb, 500, NULL);
}

void screen_dashboard_create(void)
{
    if (s_screen == NULL) {
        build_screen();
    }

    refresh_dashboard_from_model(true);
    show_page(NAV_DASHBOARD);
    bottom_nav_set_active(s_nav, NAV_DASHBOARD);
    lv_scr_load(s_screen);
}
