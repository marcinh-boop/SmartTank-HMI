#pragma once

#include "app_model.h"
#include "lvgl.h"

typedef void (*screen_tank_calibration_back_cb_t)(void);
typedef void (*screen_tank_calibration_save_cb_t)(
    const tank_channel_config_t *config
);

lv_obj_t *screen_tank_calibration_create(
    lv_obj_t *parent,
    screen_tank_calibration_back_cb_t back_cb,
    screen_tank_calibration_save_cb_t save_cb
);

void screen_tank_calibration_begin(const smarttank_state_t *state);
void screen_tank_calibration_update_live(const smarttank_state_t *state);
