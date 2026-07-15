/*
 * Element ekranu screen_alarms.h: tworzy widok LVGL, obsługuje zdarzenia użytkownika i odświeża prezentowane dane.
 * Ten nagłówek określa publiczne typy i funkcje dostępne dla innych części programu.
 * Oddzielenie odpowiedzialności ułatwia testowanie, diagnostykę i późniejszą rozbudowę urządzenia.
 */
#pragma once

#include "lvgl.h"

void screen_alarms_open(lv_obj_t *parent_screen);
void screen_alarms_hide(void);
