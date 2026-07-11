#pragma once

#include "app_model.h"
#include "lvgl.h"

typedef struct {
    lv_obj_t *root;

    lv_obj_t *temperature_label;
    lv_obj_t *description_label;

    lv_obj_t *rain_value;
    lv_obj_t *wind_value;
    lv_obj_t *humidity_value;

    lv_obj_t *forecast_day[WEATHER_FORECAST_DAYS];
    lv_obj_t *forecast_temp[WEATHER_FORECAST_DAYS];
    lv_obj_t *forecast_icon[WEATHER_FORECAST_DAYS];
} weather_widget_t;

weather_widget_t weather_widget_create(lv_obj_t *parent);
void weather_widget_set_data(
    weather_widget_t *widget,
    const weather_measurement_t *measurement
);
