#pragma once

#include "app_model.h"
#include "esp_err.h"

esp_err_t settings_storage_init(void);
esp_err_t settings_storage_load_tank_config(tank_channel_config_t *config);
esp_err_t settings_storage_save_tank_config(const tank_channel_config_t *config);
