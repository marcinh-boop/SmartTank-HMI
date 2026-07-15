/*
 * Element ekranu screen_settings.h: tworzy widok LVGL, obsługuje zdarzenia użytkownika i odświeża prezentowane dane.
 * Ten nagłówek określa publiczne typy i funkcje dostępne dla innych części programu.
 * Oddzielenie odpowiedzialności ułatwia testowanie, diagnostykę i późniejszą rozbudowę urządzenia.
 */
#pragma once

#include "lvgl.h"

void screen_settings_open(lv_obj_t *parent_screen);
void screen_settings_hide(void);
