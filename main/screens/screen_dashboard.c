#include "screen_dashboard.h"
#include "theme.h"
#include "lvgl.h"

static lv_obj_t *create_card(lv_obj_t *parent, const char *title, lv_color_t color)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 245, 285);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_bg_color(card, ST_COLOR_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 14, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, color, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 14, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(card);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);

    return card;
}

void screen_dashboard_create(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_clean(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_bg_color(screen, ST_COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *top = lv_obj_create(screen);
    lv_obj_set_size(top, 760, 55);
    lv_obj_align(top, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_clear_flag(top, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(top, lv_color_hex(0x08131F), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(top, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(top, 10, LV_PART_MAIN);
    lv_obj_set_style_border_width(top, 0, LV_PART_MAIN);

    lv_obj_t *logo = lv_label_create(top);
    lv_label_set_text(logo, "SmartTank HMI");
    lv_obj_set_style_text_color(logo, ST_COLOR_TEXT, LV_PART_MAIN);
    lv_obj_set_style_text_font(logo, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(logo, LV_ALIGN_LEFT_MID, 12, 0);

    lv_obj_t *time = lv_label_create(top);
    lv_label_set_text(time, "12:30");
    lv_obj_set_style_text_color(time, ST_COLOR_TEXT, LV_PART_MAIN);
    lv_obj_set_style_text_font(time, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(time, LV_ALIGN_CENTER, -20, 0);

    lv_obj_t *status = lv_label_create(top);
    lv_label_set_text(status, "WiFi   MQTT   24.1V");
    lv_obj_set_style_text_color(status, ST_COLOR_ACCENT, LV_PART_MAIN);
    lv_obj_set_style_text_font(status, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(status, LV_ALIGN_RIGHT_MID, -12, 0);

    lv_obj_t *szambo = create_card(screen, "SZAMBO", lv_color_hex(0x39D12F));
    lv_obj_align(szambo, LV_ALIGN_TOP_LEFT, 20, 85);

    lv_obj_t *studnia = create_card(screen, "STUDNIA", lv_color_hex(0x2EA8FF));
    lv_obj_align(studnia, LV_ALIGN_TOP_MID, 0, 85);

    lv_obj_t *pogoda = create_card(screen, "POGODA", lv_color_hex(0xFFC247));
    lv_obj_align(pogoda, LV_ALIGN_TOP_RIGHT, -20, 85);

    lv_obj_t *bottom = lv_obj_create(screen);
    lv_obj_set_size(bottom, 760, 55);
    lv_obj_align(bottom, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_clear_flag(bottom, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(bottom, lv_color_hex(0x08131F), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bottom, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bottom, 10, LV_PART_MAIN);
    lv_obj_set_style_border_width(bottom, 0, LV_PART_MAIN);

    lv_obj_t *menu = lv_label_create(bottom);
    lv_label_set_text(menu, "Pulpit      Historia      Alarmy      Ustawienia      Serwis      Informacje");
    lv_obj_set_style_text_color(menu, ST_COLOR_TEXT_DIM, LV_PART_MAIN);
    lv_obj_set_style_text_font(menu, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_center(menu);
}