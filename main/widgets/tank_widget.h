#pragma once

#include "lvgl.h"

typedef struct {
    lv_obj_t *root;
    lv_obj_t *arc;
    lv_obj_t *percent_label;
    lv_obj_t *volume_label;
    lv_obj_t *capacity_label;
    lv_obj_t *vertical_bar;
    lv_obj_t *status_label;
    lv_obj_t *info_80_value;
    lv_obj_t *info_forecast_value;
    lv_obj_t *info_last_empty_value;
} tank_widget_t;

tank_widget_t tank_widget_create(lv_obj_t *parent);
void tank_widget_set_data(tank_widget_t *widget, int percent, float volume_m3, float capacity_m3);