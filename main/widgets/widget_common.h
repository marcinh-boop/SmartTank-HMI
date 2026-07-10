#pragma once

#include "lvgl.h"

lv_obj_t *widget_label_create(
    lv_obj_t *parent,
    const char *text,
    lv_color_t color,
    lv_align_t align,
    int x,
    int y
);

lv_obj_t *widget_info_row_create(
    lv_obj_t *parent,
    const char *left_text,
    const char *right_text,
    lv_color_t border_color,
    int bottom_offset
);