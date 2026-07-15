/*
 * Element ekranu screen_boot.c: tworzy widok LVGL, obsługuje zdarzenia użytkownika i odświeża prezentowane dane.
 * Implementacja ukrywa szczegóły działania; inne moduły powinny korzystać z odpowiadającego jej API.
 * Oddzielenie odpowiedzialności ułatwia testowanie, diagnostykę i późniejszą rozbudowę urządzenia.
 */
#include "screen_boot.h"
#include "theme.h"
#include "lvgl.h"

void screen_boot_create(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_clean(screen);

    lv_obj_set_style_bg_color(screen, ST_COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *panel = lv_obj_create(screen);
    lv_obj_set_size(panel, 560, 240);
    lv_obj_center(panel);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_bg_color(panel, ST_COLOR_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 18, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, ST_COLOR_ACCENT, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 20, LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, "SmartTank HMI");
    lv_obj_set_style_text_color(title, ST_COLOR_TEXT, LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 35);

    lv_obj_t *status = lv_label_create(panel);
    lv_label_set_text(status, "Inicjalizacja systemu...");
    lv_obj_set_style_text_color(status, ST_COLOR_ACCENT, LV_PART_MAIN);
    lv_obj_set_style_text_font(status, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(status, LV_ALIGN_CENTER, 0, 10);

    lv_obj_t *version = lv_label_create(panel);
    lv_label_set_text(version, "Wersja 0.1");
    lv_obj_set_style_text_color(version, ST_COLOR_TEXT_DIM, LV_PART_MAIN);
    lv_obj_set_style_text_font(version, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(version, LV_ALIGN_BOTTOM_MID, 0, -25);
}
