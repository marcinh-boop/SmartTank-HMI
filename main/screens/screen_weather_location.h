/*
 * Moduł screen_weather_location.h należy do warstwy głównej programu SmartTank.
 * Udostępnia typy, stałe i funkcje używane przez pozostałe części aplikacji.
 * Oddzielenie tej odpowiedzialności ułatwia diagnostykę, testy i dalszą rozbudowę.
 */
#pragma once

#include "lvgl.h"

void screen_weather_location_open(lv_obj_t *parent_screen);
void screen_weather_location_hide(void);
