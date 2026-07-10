#pragma once

#include "lvgl.h"

typedef struct {
    lv_obj_t *root;

    lv_obj_t *temperature_label;
    lv_obj_t *description_label;

    lv_obj_t *rain_value;
    lv_obj_t *wind_value;
    lv_obj_t *humidity_value;

    lv_obj_t *forecast_temp[4];
    lv_obj_t *forecast_icon[4];
} weather_widget_t;

weather_widget_t weather_widget_create(lv_obj_t *parent);

void weather_widget_set_current(
    weather_widget_t *widget,
    float temperature_c,
    int rain_percent,
    float wind_kmh,
    int humidity_percent,
    const char *description
);