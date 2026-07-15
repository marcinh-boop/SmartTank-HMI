/*
 * Moduł screen_well_detail.h należy do warstwy głównej programu SmartTank.
 * Udostępnia typy, stałe i funkcje używane przez pozostałe części aplikacji.
 * Oddzielenie tej odpowiedzialności ułatwia diagnostykę, testy i dalszą rozbudowę.
 */
#pragma once

#include "app_model.h"
#include "lvgl.h"

typedef void (*screen_well_detail_back_cb_t)(void);
typedef void (*screen_well_detail_calibration_cb_t)(void);

lv_obj_t *screen_well_detail_create(
    lv_obj_t *parent,
    screen_well_detail_back_cb_t back_cb,
    screen_well_detail_calibration_cb_t calibration_cb
);

void screen_well_detail_update(const smarttank_state_t *state);
