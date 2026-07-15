/*
 * Widżet weather_widget.h: wielokrotny komponent LVGL używany przez ekrany do spójnej prezentacji danych.
 * Ten nagłówek określa publiczne typy i funkcje dostępne dla innych części programu.
 * Oddzielenie odpowiedzialności ułatwia testowanie, diagnostykę i późniejszą rozbudowę urządzenia.
 */
#pragma once

#include "app_model.h"
#include "lvgl.h"

typedef struct {
    lv_obj_t *root;

    lv_obj_t *temperature_label;
    lv_obj_t *description_label;
    lv_obj_t *update_status_label;

    lv_obj_t *rain_value;
    lv_obj_t *wind_value;
    lv_obj_t *humidity_value;

    lv_obj_t *icon_root;
    lv_obj_t *icon_clear;
    lv_obj_t *icon_partly_cloudy;
    lv_obj_t *icon_cloudy;
    lv_obj_t *icon_fog;
    lv_obj_t *icon_rain;
    lv_obj_t *icon_snow;
    lv_obj_t *icon_storm;

    lv_obj_t *forecast_day[WEATHER_FORECAST_DAYS];
    lv_obj_t *forecast_temp[WEATHER_FORECAST_DAYS];
    lv_obj_t *forecast_icon[WEATHER_FORECAST_DAYS];
} weather_widget_t;

weather_widget_t weather_widget_create(lv_obj_t *parent);
void weather_widget_set_data(
    weather_widget_t *widget,
    const weather_measurement_t *measurement
);

void weather_widget_set_current(
    weather_widget_t *widget,
    float temperature_c,
    int rain_percent,
    float wind_kmh,
    int humidity_percent,
    const char *description
);
