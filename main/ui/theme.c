/*
 * Moduł theme.c należy do warstwy głównej programu SmartTank.
 * Realizuje logikę modułu i ukrywa jej szczegóły za publicznym interfejsem.
 * Oddzielenie tej odpowiedzialności ułatwia diagnostykę, testy i dalszą rozbudowę.
 */
#include "theme.h"

void theme_init(void)
{
    lv_obj_t *screen = lv_scr_act();

    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, ST_COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
}
