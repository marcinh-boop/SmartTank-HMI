#include "well_widget.h"
#include "widget_common.h"
#include "theme.h"

#include <stdio.h>

#define WELL_BLUE        lv_color_hex(0x2EA8FF)
#define WELL_DARK_BLUE   lv_color_hex(0x0D2744)
#define WELL_BORDER      lv_color_hex(0x154E78)
#define WELL_GREEN       lv_color_hex(0x39D12F)
#define WELL_RED         lv_color_hex(0xFF3333)
#define WELL_YELLOW      lv_color_hex(0xFFC247)

well_widget_t well_widget_create(lv_obj_t *parent)
{
    well_widget_t widget = {0};
    widget.root = parent;

    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    widget_label_create(
        parent,
        "STUDNIA",
        WELL_BLUE,
        LV_ALIGN_TOP_LEFT,
        0,
        0
    );

    widget.depth_label = widget_label_create(
        parent,
        "Glebokosc: 4.00 m",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_TOP_LEFT,
        0,
        22
    );

    widget.arc = lv_arc_create(parent);
    lv_obj_set_size(widget.arc, 122, 122);
    lv_obj_align(widget.arc, LV_ALIGN_CENTER, -36, -27);

    lv_arc_set_range(widget.arc, 0, 100);
    lv_arc_set_value(widget.arc, 70);
    lv_arc_set_bg_angles(widget.arc, 135, 45);

    lv_obj_remove_style(widget.arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(widget.arc, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_style_arc_width(widget.arc, 9, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(widget.arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_color(widget.arc, WELL_DARK_BLUE, LV_PART_MAIN);

    lv_obj_set_style_arc_width(widget.arc, 9, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(widget.arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(widget.arc, WELL_BLUE, LV_PART_INDICATOR);

    widget.level_label = widget_label_create(
        parent,
        "2.81 m",
        ST_COLOR_TEXT,
        LV_ALIGN_CENTER,
        -36,
        -38
    );

    widget.description_label = widget_label_create(
        parent,
        "slup wody",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_CENTER,
        -36,
        -4
    );

    widget.vertical_bar = lv_bar_create(parent);
    lv_obj_set_size(widget.vertical_bar, 18, 105);
    lv_obj_align(widget.vertical_bar, LV_ALIGN_RIGHT_MID, -8, -27);

    lv_bar_set_range(widget.vertical_bar, 0, 100);
    lv_bar_set_value(widget.vertical_bar, 70, LV_ANIM_OFF);

    lv_obj_set_style_bg_color(
        widget.vertical_bar,
        WELL_DARK_BLUE,
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_color(
        widget.vertical_bar,
        WELL_BLUE,
        LV_PART_INDICATOR
    );

    lv_obj_set_style_radius(widget.vertical_bar, 5, LV_PART_MAIN);
    lv_obj_set_style_radius(widget.vertical_bar, 5, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(widget.vertical_bar, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(
        widget.vertical_bar,
        lv_color_hex(0x1A5C8F),
        LV_PART_MAIN
    );

    widget.scale_top_label = widget_label_create(
        parent,
        "4.00 m",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_RIGHT_MID,
        -30,
        -75
    );

    widget.scale_mid_label = widget_label_create(
        parent,
        "2.00 m",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_RIGHT_MID,
        -30,
        -2
    );

    widget.scale_bottom_label = widget_label_create(
        parent,
        "0.00 m",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_RIGHT_MID,
        -30,
        50
    );

    widget.reserve_value = widget_info_row_create(
        parent,
        "Zapas",
        "2.00 m3",
        WELL_BORDER,
        -78
    );

    lv_obj_set_style_text_color(
        widget.reserve_value,
        WELL_BLUE,
        LV_PART_MAIN
    );

    widget.consumption_value = widget_info_row_create(
        parent,
        "Pobor",
        "-2.00 m3/h",
        WELL_BORDER,
        -50
    );

    lv_obj_set_style_text_color(
        widget.consumption_value,
        WELL_BLUE,
        LV_PART_MAIN
    );

    widget.regeneration_value = widget_info_row_create(
        parent,
        "Przyrost",
        "+1.80 m3/h",
        WELL_BORDER,
        -22
    );

    lv_obj_set_style_text_color(
        widget.regeneration_value,
        WELL_GREEN,
        LV_PART_MAIN
    );

    widget.status_label = widget_label_create(
        parent,
        "Poziom OK",
        WELL_BLUE,
        LV_ALIGN_BOTTOM_MID,
        0,
        0
    );

    return widget;
}

void well_widget_set_data(
    well_widget_t *widget,
    float water_column_m,
    float well_depth_m)
{
    if (widget == NULL || well_depth_m <= 0.0f) {
        return;
    }

    int percent = (int)((water_column_m / well_depth_m) * 100.0f);

    if (percent < 0) {
        percent = 0;
    }

    if (percent > 100) {
        percent = 100;
    }

    lv_color_t status_color = WELL_BLUE;
    const char *status_text = "Poziom OK";

    if (percent <= 15) {
        status_color = WELL_RED;
        status_text = "ALARM - niski poziom";
    } else if (percent <= 30) {
        status_color = WELL_YELLOW;
        status_text = "Niski poziom";
    }

    char buffer[48];

    snprintf(buffer, sizeof(buffer), "Glebokosc: %.2f m", well_depth_m);
    lv_label_set_text(widget->depth_label, buffer);

    snprintf(buffer, sizeof(buffer), "%.2f m", water_column_m);
    lv_label_set_text(widget->level_label, buffer);

    snprintf(buffer, sizeof(buffer), "%.2f m", well_depth_m);
    lv_label_set_text(widget->scale_top_label, buffer);

    snprintf(buffer, sizeof(buffer), "%.2f m", well_depth_m / 2.0f);
    lv_label_set_text(widget->scale_mid_label, buffer);

    lv_label_set_text(widget->scale_bottom_label, "0.00 m");

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
