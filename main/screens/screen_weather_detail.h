/*
 * Moduł screen_weather_detail.h należy do warstwy głównej programu SmartTank.
 * Udostępnia typy, stałe i funkcje używane przez pozostałe części aplikacji.
 * Oddzielenie tej odpowiedzialności ułatwia diagnostykę, testy i dalszą rozbudowę.
 */
#pragma once

#include "app_model.h"
#include "lvgl.h"

typedef void (*screen_weather_detail_back_cb_t)(void);

lv_obj_t *screen_weather_detail_create(
    lv_obj_t *parent,
    screen_weather_detail_back_cb_t back_cb
);

void screen_weather_detail_begin(const smarttank_state_t *state);
void screen_weather_detail_update(const smarttank_state_t *state);
