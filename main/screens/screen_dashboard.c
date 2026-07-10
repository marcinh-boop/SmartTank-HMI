#include "screen_dashboard.h"

#include <stdint.h>

#include "app_model.h"
#include "theme.h"
#include "lvgl.h"

#include "tank_widget.h"
#include "well_widget.h"
#include "weather_widget.h"
#include "bottom_nav.h"

static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_dashboard_content = NULL;
static lv_obj_t *s_history_content = NULL;
static lv_obj_t *s_page_label = NULL;
static lv_obj_t *s_source_label = NULL;
static bottom_nav_t *s_nav = NULL;
static lv_timer_t *s_refresh_timer = NULL;
static uint32_t s_last_revision = 0;

static tank_widget_t s_tank_widget;
static well_widget_t s_well_widget;
static weather_widget_t s_weather_widget;

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

static void create_history_chart(
    lv_obj_t *parent,
    const char *title_text,
    lv_color_t color,
    const lv_coord_t *values,
    uint32_t count)
{
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, title_text);
    lv_obj_set_style_text_color(title, color, LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *range = lv_label_create(parent);
    lv_label_set_text(range, "Ostatnie 24 h");
    lv_obj_set_style_text_color(range, ST_COLOR_TEXT_DIM, LV_PART_MAIN);
    lv_obj_set_style_text_font(range, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(range, LV_ALIGN_TOP_RIGHT, 0, 0);

    lv_obj_t *chart = lv_chart_create(parent);
    lv_obj_set_size(chart, 330, 235);
    lv_obj_align(chart, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_point_count(chart, count);
    lv_chart_set_div_line_count(chart, 5, 6);
    lv_obj_set_style_bg_color(chart, lv_color_hex(0x09131D), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(chart, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(chart, 0, LV_PART_MAIN);
    lv_obj_set_style_line_color(chart, lv_color_hex(0x23384A), LV_PART_MAIN);
    lv_obj_set_style_line_width(chart, 1, LV_PART_MAIN);

    lv_chart_series_t *series = lv_chart_add_series(chart, color, LV_CHART_AXIS_PRIMARY_Y);
    for (uint32_t i = 0; i < count; i++) {
        lv_chart_set_next_value(chart, series, values[i]);
    }
}

static void build_dashboard_content(void)
{
    s_dashboard_content = create_content_layer(s_screen);

    lv_obj_t *tank_card = create_card(s_dashboard_content, lv_color_hex(0x39D12F));
    lv_obj_align(tank_card, LV_ALIGN_TOP_LEFT, 20, 10);
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
    static const lv_coord_t tank_values[24] = {
        54, 55, 55, 56, 57, 58, 59, 60,
        61, 62, 63, 64, 65, 66, 67, 68,
        68, 69, 70, 70, 71, 71, 72, 72
    };

    static const lv_coord_t well_values[24] = {
        76, 75, 74, 74, 73, 72, 71, 70,
        69, 68, 69, 70, 71, 72, 73, 72,
        71, 70, 70, 69, 70, 70, 70, 70
    };

    s_history_content = create_content_layer(s_screen);

    lv_obj_t *tank_panel = create_history_panel(s_history_content, 20);
    create_history_chart(tank_panel, "SZAMBO  72%", lv_color_hex(0x39D12F), tank_values, 24);

    lv_obj_t *well_panel = create_history_panel(s_history_content, 410);
    create_history_chart(well_panel, "STUDNIA  2.81 m", lv_color_hex(0x2EA8FF), well_values, 24);

    lv_obj_add_flag(s_history_content, LV_OBJ_FLAG_HIDDEN);
}

static void refresh_dashboard_from_model(bool force)
{
    smarttank_state_t state;
    app_model_get_snapshot(&state);

    if (!force && state.revision == s_last_revision) {
        return;
    }

    s_last_revision = state.revision;

    tank_widget_set_data(
        &s_tank_widget,
        state.tank.level_percent,
        state.tank.volume_m3,
        state.tank.capacity_m3
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

    if (state.system.simulation_active) {
        lv_label_set_text(s_source_label, "SYMULACJA | RS485: OFF");
    } else if (state.system.modbus_connected) {
        lv_label_set_text(s_source_label, "MODBUS RTU | ONLINE");
    } else {
        lv_label_set_text(s_source_label, "MODBUS RTU | OFFLINE");
    }
}

static void dashboard_refresh_cb(lv_timer_t *timer)
{
    (void)timer;
    refresh_dashboard_from_model(false);
}

static void show_page(bottom_nav_page_t page)
{
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
    s_source_label = create_label(top, "SYMULACJA | RS485: OFF", ST_COLOR_TEXT_DIM, LV_ALIGN_RIGHT_MID, -12, 0);

    build_dashboard_content();
    build_history_content();

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
