/*
 * Moduł theme.h należy do warstwy głównej programu SmartTank.
 * Udostępnia typy, stałe i funkcje używane przez pozostałe części aplikacji.
 * Oddzielenie tej odpowiedzialności ułatwia diagnostykę, testy i dalszą rozbudowę.
 */
#pragma once

#include "lvgl.h"

#define ST_COLOR_BG        lv_color_hex(0x071018)
#define ST_COLOR_PANEL     lv_color_hex(0x111D2A)
#define ST_COLOR_TEXT      lv_color_hex(0xFFFFFF)
#define ST_COLOR_TEXT_DIM  lv_color_hex(0x8FA3B8)
#define ST_COLOR_ACCENT    lv_color_hex(0x2EA8FF)

void theme_init(void);
