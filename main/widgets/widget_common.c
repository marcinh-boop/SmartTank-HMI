#include "widget_common.h"
#include "theme.h"

lv_obj_t *widget_label_create(
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

lv_obj_t *widget_info_row_create(
    lv_obj_t *parent,
    const char *left_text,
    const char *right_text,
    lv_color_t border_color,
    int bottom_offset)
{
    lv_obj_t *row = lv_obj_create(parent);

    lv_obj_set_size(row, 205, 25);
    lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, bottom_offset);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_bg_color(row, lv_color_hex(0x09141D), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(row, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_radius(row, 5, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(row, border_color, LV_PART_MAIN);
    lv_obj_set_style_pad_left(row, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_right(row, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_top(row, 3, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(row, 3, LV_PART_MAIN);

    widget_label_create(
        row,
        left_text,
        ST_COLOR_TEXT,
        LV_ALIGN_LEFT_MID,
        0,
        0
    );

    return widget_label_create(
        row,
        right_text,
        ST_COLOR_TEXT,
        LV_ALIGN_RIGHT_MID,
        0,
        0
    );
}