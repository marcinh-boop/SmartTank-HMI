/*
 * Moduł screen_well_calibration.h należy do warstwy głównej programu SmartTank.
 * Udostępnia typy, stałe i funkcje używane przez pozostałe części aplikacji.
 * Oddzielenie tej odpowiedzialności ułatwia diagnostykę, testy i dalszą rozbudowę.
 */
#pragma once

#include "app_model.h"
#include "lvgl.h"
#include "well_settings.h"

typedef void (*screen_well_calibration_back_cb_t)(void);
typedef void (*screen_well_calibration_save_cb_t)(const well_settings_t *settings);

lv_obj_t *screen_well_calibration_create(
    lv_obj_t *parent,
    screen_well_calibration_back_cb_t back_cb,
    screen_well_calibration_save_cb_t save_cb
);

void screen_well_calibration_begin(const smarttank_state_t *state);
void screen_well_calibration_update_live(const smarttank_state_t *state);
