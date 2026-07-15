/*
 * Widżet tank_widget.h: wielokrotny komponent LVGL używany przez ekrany do spójnej prezentacji danych.
 * Ten nagłówek określa publiczne typy i funkcje dostępne dla innych części programu.
 * Oddzielenie odpowiedzialności ułatwia testowanie, diagnostykę i późniejszą rozbudowę urządzenia.
 */
#pragma once

#include "lvgl.h"
#include "app_model.h"

typedef struct {
    lv_obj_t *root;
    lv_obj_t *capacity_label;
    lv_obj_t *arc;
    lv_obj_t *percent_label;
    lv_obj_t *volume_label;
    lv_obj_t *vertical_bar;
    lv_obj_t *status_label;
    lv_obj_t *info_80_value;
    lv_obj_t *info_forecast_value;
    lv_obj_t *info_last_empty_value;
} tank_widget_t;

tank_widget_t tank_widget_create(lv_obj_t *parent);

void tank_widget_set_data(
    tank_widget_t *widget,
    int percent,
    float volume_m3,
    float capacity_m3,
    int warning_percent,
    int critical_percent,
    bool valid,
    sensor_health_t health,
    float distance_mm,
    float distance_empty_mm,
    float distance_full_mm
);
