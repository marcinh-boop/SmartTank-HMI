/*
 * Moduł screen_history.c należy do warstwy głównej programu SmartTank.
 * Realizuje logikę modułu i ukrywa jej szczegóły za publicznym interfejsem.
 * Oddzielenie tej odpowiedzialności ułatwia diagnostykę, testy i dalszą rozbudowę.
 */
#include "screen_history.h"
#include "screen_dashboard.h"
#include "bottom_nav.h"
#include "theme.h"
#include "lvgl.h"

static lv_obj_t *s_history_screen = NULL;

static void load_dashboard_async(void *user_data)
{
    (void)user_data;
    screen_dashboard_create();
}

static bool history_nav_change(bottom_nav_page_t page)
{
    if (page == NAV_DASHBOARD) {
        /* Przelaczenie nastapi po zakonczeniu obslugi dotyku. */
        lv_async_call(load_dashboard_async, NULL);
    }

    /* Aktywna zakladka jest ustawiona w pasku docelowego ekranu. */
    return false;
}

static lv_obj_t *create_panel(lv_obj_t *parent, int x, int width)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, width, 330);
    lv_obj_align(panel, LV_ALIGN_TOP_LEFT, x, 68);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(panel, ST_COLOR_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 14, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x24384A), LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 14, LV_PART_MAIN);
    return panel;
}

static void create_top_bar(lv_obj_t *screen)
{
    lv_obj_t *top = lv_obj_create(screen);
    lv_obj_set_size(top, 760, 50);
    lv_obj_align(top, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_clear_flag(top, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(top, lv_color_hex(0x08131F), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(top, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(top, 10, LV_PART_MAIN);
    lv_obj_set_style_border_width(top, 0, LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(top);
    lv_label_set_text(title, "SmartTank HMI");
    lv_obj_set_style_text_color(title, ST_COLOR_TEXT, LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 12, 0);

    lv_obj_t *page = lv_label_create(top);
    lv_label_set_text(page, "HISTORIA");
    lv_obj_set_style_text_color(page, ST_COLOR_ACCENT, LV_PART_MAIN);
    lv_obj_set_style_text_font(page, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_center(page);

    lv_obj_t *time = lv_label_create(top);
    lv_label_set_text(time, "12:30");
    lv_obj_set_style_text_color(time, ST_COLOR_TEXT_DIM, LV_PART_MAIN);
    lv_obj_set_style_text_font(time, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(time, LV_ALIGN_RIGHT_MID, -12, 0);
}

static void create_bottom_bar(lv_obj_t *screen)
{
    lv_obj_t *bottom = lv_obj_create(screen);
    lv_obj_set_size(bottom, 760, 50);
    lv_obj_align(bottom, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_clear_flag(bottom, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(bottom, lv_color_hex(0x08131F), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bottom, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bottom, 10, LV_PART_MAIN);
    lv_obj_set_style_border_width(bottom, 0, LV_PART_MAIN);
    bottom_nav_create(bottom, NAV_HISTORY, history_nav_change);
}

static lv_obj_t *create_chart(
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

    lv_obj_t *range = lv_label_create(parent);
    lv_label_set_text(range, "Ostatnie 24 h");
    lv_obj_set_style_text_color(range, ST_COLOR_TEXT_DIM, LV_PART_MAIN);
    lv_obj_set_style_text_font(range, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(range, LV_ALIGN_TOP_RIGHT, 0, 0);

    return chart;
}

static void build_history_screen(void)
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

    s_history_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(s_history_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_history_screen, ST_COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_history_screen, LV_OPA_COVER, LV_PART_MAIN);

    create_top_bar(s_history_screen);

    lv_obj_t *tank_panel = create_panel(s_history_screen, 20, 370);
    create_chart(tank_panel, "SZAMBO  72%", lv_color_hex(0x39D12F), tank_values, 24);

    lv_obj_t *well_panel = create_panel(s_history_screen, 410, 370);
    create_chart(well_panel, "STUDNIA  2.81 m", lv_color_hex(0x2EA8FF), well_values, 24);

    create_bottom_bar(s_history_screen);
}

void screen_history_create(void)
{
    if (s_history_screen == NULL) {
        build_history_screen();
    }

    lv_scr_load(s_history_screen);
}
