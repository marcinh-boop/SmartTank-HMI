#include "tank_widget.h"
#include "widget_common.h"
#include "theme.h"

#include <stdio.h>

#define TANK_GREEN       lv_color_hex(0x39D12F)
#define TANK_DARK_GREEN  lv_color_hex(0x073A12)
#define TANK_BORDER      lv_color_hex(0x1D5A27)
#define TANK_YELLOW      lv_color_hex(0xFFC247)
#define TANK_RED         lv_color_hex(0xFF3333)

tank_widget_t tank_widget_create(lv_obj_t *parent)
{
    tank_widget_t widget = {0};
    widget.root = parent;

    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    widget_label_create(
        parent,
        "SZAMBO",
        TANK_GREEN,
        LV_ALIGN_TOP_LEFT,
        0,
        0
    );

    widget.capacity_label = widget_label_create(
        parent,
        "Pojemnosc: 10.50 m3",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_TOP_LEFT,
        0,
        22
    );

    widget.arc = lv_arc_create(parent);
    lv_obj_set_size(widget.arc, 122, 122);
    lv_obj_align(widget.arc, LV_ALIGN_CENTER, -36, -27);

    lv_arc_set_range(widget.arc, 0, 100);
    lv_arc_set_value(widget.arc, 72);
    lv_arc_set_bg_angles(widget.arc, 135, 45);

    lv_obj_remove_style(widget.arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(widget.arc, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_style_arc_width(widget.arc, 9, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(widget.arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_color(widget.arc, TANK_DARK_GREEN, LV_PART_MAIN);

    lv_obj_set_style_arc_width(widget.arc, 9, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(widget.arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(widget.arc, TANK_GREEN, LV_PART_INDICATOR);

    widget.percent_label = widget_label_create(
        parent,
        "72%",
        ST_COLOR_TEXT,
        LV_ALIGN_CENTER,
        -36,
        -38
    );

    widget.volume_label = widget_label_create(
        parent,
        "7.56 m3",
        ST_COLOR_TEXT,
        LV_ALIGN_CENTER,
        -36,
        -4
    );

    widget.vertical_bar = lv_bar_create(parent);
    lv_obj_set_size(widget.vertical_bar, 18, 105);
    lv_obj_align(widget.vertical_bar, LV_ALIGN_RIGHT_MID, -8, -27);

    lv_bar_set_range(widget.vertical_bar, 0, 100);
    lv_bar_set_value(widget.vertical_bar, 72, LV_ANIM_OFF);

    lv_obj_set_style_bg_color(
        widget.vertical_bar,
        lv_color_hex(0x10251A),
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_color(
        widget.vertical_bar,
        TANK_GREEN,
        LV_PART_INDICATOR
    );

    lv_obj_set_style_radius(widget.vertical_bar, 5, LV_PART_MAIN);
    lv_obj_set_style_radius(widget.vertical_bar, 5, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(widget.vertical_bar, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(
        widget.vertical_bar,
        lv_color_hex(0x2C6E35),
        LV_PART_MAIN
    );

    widget_label_create(
        parent,
        "100%",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_RIGHT_MID,
        -32,
        -75
    );

    widget_label_create(
        parent,
        "80%",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_RIGHT_MID,
        -32,
        -40
    );

    widget_label_create(
        parent,
        "50%",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_RIGHT_MID,
        -32,
        0
    );

    widget_label_create(
        parent,
        "0%",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_RIGHT_MID,
        -32,
        50
    );

    widget.info_80_value = widget_info_row_create(
        parent,
        "Rezerwa",
        "1.05 m3",
        TANK_BORDER,
        -78
    );

    lv_obj_set_style_text_color(
        widget.info_80_value,
        TANK_YELLOW,
        LV_PART_MAIN
    );

    widget.info_forecast_value = widget_info_row_create(
        parent,
        "Napelnienie",
        "5 dni",
        TANK_BORDER,
        -50
    );

    widget.info_last_empty_value = widget_info_row_create(
        parent,
        "Oproznienie",
        "18 dni",
        TANK_BORDER,
        -22
    );

    widget.status_label = widget_label_create(
        parent,
        "Poziom OK",
        TANK_GREEN,
        LV_ALIGN_BOTTOM_MID,
        0,
        0
    );

    return widget;
}

void tank_widget_set_data(
    tank_widget_t *widget,
    int percent,
    float volume_m3,
    float capacity_m3,
    int warning_percent,
    int critical_percent)
{
    if (widget == NULL) {
        return;
    }

    if (percent < 0) {
        percent = 0;
    }

    if (percent > 100) {
        percent = 100;
    }

    lv_color_t status_color = TANK_GREEN;
    const char *status_text = "Poziom OK";

    if (percent >= critical_percent) {
        status_color = TANK_RED;
        status_text = "ALARM";
    } else if (percent >= warning_percent) {
        status_color = TANK_YELLOW;
        status_text = "Ostrzezenie";
    }

    char buffer[48];

    snprintf(buffer, sizeof(buffer), "Pojemnosc: %.2f m3", capacity_m3);
    lv_label_set_text(widget->capacity_label, buffer);

    snprintf(buffer, sizeof(buffer), "%d%%", percent);
    lv_label_set_text(widget->percent_label, buffer);

    snprintf(buffer, sizeof(buffer), "%.2f m3", volume_m3);
    lv_label_set_text(widget->volume_label, buffer);

    float reserve_m3 = capacity_m3 - volume_m3;
    if (reserve_m3 < 0.0f) {
        reserve_m3 = 0.0f;
    }
    snprintf(buffer, sizeof(buffer), "%.2f m3", reserve_m3);
    lv_label_set_text(widget->info_80_value, buffer);

    lv_arc_set_value(widget->arc, percent);
    lv_bar_set_value(widget->vertical_bar, percent, LV_ANIM_ON);

    lv_obj_set_style_arc_color(
        widget->arc,
        status_color,
        LV_PART_INDICATOR
    );

    lv_obj_set_style_bg_color(
        widget->vertical_bar,
        status_color,
        LV_PART_INDICATOR
    );

    lv_label_set_text(widget->status_label, status_text);

    lv_obj_set_style_text_color(
        widget->status_label,
        status_color,
        LV_PART_MAIN
    );
}
