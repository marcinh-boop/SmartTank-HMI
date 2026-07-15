/*
 * Moduł well_detail_integration.h należy do warstwy głównej programu SmartTank.
 * Udostępnia typy, stałe i funkcje używane przez pozostałe części aplikacji.
 * Oddzielenie tej odpowiedzialności ułatwia diagnostykę, testy i dalszą rozbudowę.
 */
#pragma once

#include <stdbool.h>

#include "lvgl.h"

bool well_detail_integration_attach(lv_obj_t *screen);
