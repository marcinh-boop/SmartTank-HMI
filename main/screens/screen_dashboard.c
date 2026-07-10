#include "screen_dashboard.h"
#include "theme.h"
#include "lvgl.h"

#include "tank_widget.h"
#include "well_widget.h"
#include "weather_widget.h"
#include "bottom_nav.h"

static lv_obj_t *create_card(
    lv_obj_t *parent,
    lv_color_t color)
{
    lv_obj_t *card = lv_obj_create(parent);

    lv_obj_set_size(card, 245, 330);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_bg_color(
        card,
        ST_COLOR_PANEL,
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        card,
        LV_OPA_COVER,
        LV_PART_MAIN
    );

    lv_obj_set_style_radius(
        card,
        14,
        LV_PART_MAIN
    );

    lv_obj_set_style_border_width(
        card,
        2,
        LV_PART_MAIN
    );

    lv_obj_set_style_border_color(
        card,
        color,
        LV_PART_MAIN
    );

    lv_obj_set_style_pad_all(
        card,
        14,
        LV_PART_MAIN
    );

    return card;
}

void screen_dashboard_create(void)
{
    lv_obj_t *screen = lv_scr_act();

    lv_obj_clean(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_bg_color(
        screen,
        ST_COLOR_BG,
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        screen,
        LV_OPA_COVER,
        LV_PART_MAIN
    );

    lv_obj_t *top = lv_obj_create(screen);

    lv_obj_set_size(top, 760, 50);
    lv_obj_align(top, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_clear_flag(top, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_bg_color(
        top,
        lv_color_hex(0x08131F),
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        top,
        LV_OPA_COVER,
        LV_PART_MAIN
    );

    lv_obj_set_style_radius(top, 10, LV_PART_MAIN);
    lv_obj_set_style_border_width(top, 0, LV_PART_MAIN);

    lv_obj_t *logo = lv_label_create(top);

    lv_label_set_text(logo, "SmartTank HMI");

    lv_obj_set_style_text_color(
        logo,
        ST_COLOR_TEXT,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_font(
        logo,
        &lv_font_montserrat_14,
        LV_PART_MAIN
    );

    lv_obj_align(
        logo,
        LV_ALIGN_LEFT_MID,
        12,
        0
    );

    lv_obj_t *time_label = lv_label_create(top);

    lv_label_set_text(
        time_label,
        "12:30   09.07.2026"
    );

    lv_obj_set_style_text_color(
        time_label,
        ST_COLOR_TEXT,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_font(
        time_label,
        &lv_font_montserrat_14,
        LV_PART_MAIN
    );

    lv_obj_align(
        time_label,
        LV_ALIGN_CENTER,
        0,
        0
    );

    lv_obj_t *status = lv_label_create(top);

    lv_label_set_text(
        status,
        "WiFi   MQTT   24.1 V"
    );

    lv_obj_set_style_text_color(
        status,
        ST_COLOR_ACCENT,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_font(
        status,
        &lv_font_montserrat_14,
        LV_PART_MAIN
    );

    lv_obj_align(
        status,
        LV_ALIGN_RIGHT_MID,
        -12,
        0
    );

    lv_obj_t *tank_card = create_card(
        screen,
        lv_color_hex(0x39D12F)
    );

    lv_obj_align(
        tank_card,
        LV_ALIGN_TOP_LEFT,
        20,
        68
    );

    tank_widget_t tank =
        tank_widget_create(tank_card);

    tank_widget_set_data(
        &tank,
        72,
        7.56f,
        10.50f
    );

    lv_obj_t *well_card = create_card(
        screen,
        lv_color_hex(0x2EA8FF)
    );

    lv_obj_align(
        well_card,
        LV_ALIGN_TOP_MID,
        0,
        68
    );

    well_widget_t well =
        well_widget_create(well_card);

    well_widget_set_data(
        &well,
        2.81f,
        4.00f
    );

    lv_obj_t *weather_card = create_card(
        screen,
        lv_color_hex(0xFFC247)
    );

    lv_obj_align(
        weather_card,
        LV_ALIGN_TOP_RIGHT,
        -20,
        68
    );

    weather_widget_t weather =
        weather_widget_create(weather_card);

    weather_widget_set_current(
        &weather,
        18.6f,
        10,
        12.0f,
        62,
        "Zachmurzenie"
    );

    lv_obj_t *bottom = lv_obj_create(screen);

    lv_obj_set_size(bottom, 760, 50);
    lv_obj_align(
        bottom,
        LV_ALIGN_BOTTOM_MID,
        0,
        -8
    );

    lv_obj_clear_flag(
        bottom,
        LV_OBJ_FLAG_SCROLLABLE
    );

    lv_obj_set_style_bg_color(
        bottom,
        lv_color_hex(0x08131F),
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        bottom,
        LV_OPA_COVER,
        LV_PART_MAIN
    );

    lv_obj_set_style_radius(
        bottom,
        10,
        LV_PART_MAIN
    );

    lv_obj_set_style_border_width(
        bottom,
        0,
        LV_PART_MAIN
    );

    bottom_nav_create(
        bottom,
        NAV_DASHBOARD
    );
}