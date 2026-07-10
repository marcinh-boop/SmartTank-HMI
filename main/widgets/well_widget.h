#pragma once

#include "lvgl.h"

typedef struct {
    lv_obj_t *root;
    lv_obj_t *arc;
    lv_obj_t *level_label;
    lv_obj_t *description_label;
    lv_obj_t *vertical_bar;
    lv_obj_t *status_label;
    lv_obj_t *reserve_value;
    lv_obj_t *consumption_value;
    lv_obj_t *regeneration_value;
} well_widget_t;

well_widget_t well_widget_create(lv_obj_t *parent);

void well_widget_set_data(
    well_widget_t *widget,
    float water_column_m,
    float well_depth_m
);