/*
 * Element ekranu screen_settings_weather.h: tworzy widok LVGL, obsługuje zdarzenia użytkownika i odświeża prezentowane dane.
 * Ten nagłówek określa publiczne typy i funkcje dostępne dla innych części programu.
 * Oddzielenie odpowiedzialności ułatwia testowanie, diagnostykę i późniejszą rozbudowę urządzenia.
 */
#pragma once

#include "lvgl.h"

void screen_settings_weather_attach(lv_obj_t *screen);
