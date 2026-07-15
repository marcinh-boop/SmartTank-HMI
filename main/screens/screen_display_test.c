/*
 * Element ekranu screen_display_test.c: tworzy widok LVGL, obsługuje zdarzenia użytkownika i odświeża prezentowane dane.
 * Implementacja ukrywa szczegóły działania; inne moduły powinny korzystać z odpowiadającego jej API.
 * Oddzielenie odpowiedzialności ułatwia testowanie, diagnostykę i późniejszą rozbudowę urządzenia.
 */
#include "screen_display_test.h"
#include "lvgl.h"

static lv_obj_t *s_test_box = NULL;
static lv_obj_t *s_counter_label = NULL;
static uint32_t s_touch_count = 0;
static bool s_toggle = false;

static void test_touch_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_RELEASED) {
        return;
    }

    s_touch_count++;
    s_toggle = !s_toggle;

    lv_obj_set_style_bg_color(
        s_test_box,
        s_toggle ? lv_color_hex(0x39D12F) : lv_color_hex(0x2EA8FF),
        LV_PART_MAIN
    );

    lv_label_set_text_fmt(s_counter_label, "Dotkniecia: %lu", (unsigned long)s_touch_count);
}

void screen_display_test_create(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_clean(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x08131F), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "TEST WYSWIETLACZA RGB");
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 34);

    lv_obj_t *description = lv_label_create(screen);
    lv_label_set_text(
        description,
        "Dotykaj dowolnego miejsca. Zmieniac ma sie tylko prostokat i licznik."
    );
    lv_obj_set_style_text_color(description, lv_color_hex(0x8FA3B8), LV_PART_MAIN);
    lv_obj_set_style_text_font(description, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(description, LV_ALIGN_TOP_MID, 0, 78);

    s_test_box = lv_obj_create(screen);
    lv_obj_set_size(s_test_box, 160, 110);
    lv_obj_center(s_test_box);
    lv_obj_clear_flag(s_test_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_test_box, lv_color_hex(0x2EA8FF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_test_box, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_test_box, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_test_box, 12, LV_PART_MAIN);

    s_counter_label = lv_label_create(screen);
    lv_label_set_text(s_counter_label, "Dotkniecia: 0");
    lv_obj_set_style_text_color(s_counter_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_counter_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(s_counter_label, LV_ALIGN_BOTTOM_MID, 0, -54);

    lv_obj_add_event_cb(screen, test_touch_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(s_test_box, test_touch_cb, LV_EVENT_RELEASED, NULL);
}
