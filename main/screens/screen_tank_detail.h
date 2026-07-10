#pragma once

#include "app_model.h"
#include "lvgl.h"

typedef void (*screen_tank_detail_back_cb_t)(void);
typedef void (*screen_tank_detail_calibration_cb_t)(void);

lv_obj_t *screen_tank_detail_create(
    lv_obj_t *parent,
    screen_tank_detail_back_cb_t back_cb,
    screen_tank_detail_calibration_cb_t calibration_cb
);

void screen_tank_detail_update(const smarttank_state_t *state);
