#include "tank_widget.h"
#include "theme.h"
#include <stdio.h>

#define TANK_GREEN  lv_color_hex(0x39D12F)
#define TANK_YELLOW lv_color_hex(0xFFC247)
#define TANK_RED    lv_color_hex(0xFF3333)

static lv_obj_t *txt(lv_obj_t *p, const char *s, lv_color_t c, lv_align_t a, int x, int y)
{
    lv_obj_t *l = lv_label_create(p);
    lv_label_set_text(l, s);
    lv_obj_set_style_text_color(l, c, LV_PART_MAIN);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(l, a, x, y);
    return l;
}

static lv_obj_t *row(lv_obj_t *p, const char *left, const char *right, int y)
{
    lv_obj_t *r = lv_obj_create(p);
    lv_obj_set_size(r, 205, 24);
    lv_obj_align(r, LV_ALIGN_BOTTOM_MID, 0, y);
    lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_bg_color(r, lv_color_hex(0x0B1A12), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(r, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_radius(r, 5, LV_PART_MAIN);
    lv_obj_set_style_border_width(r, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(r, lv_color_hex(0x1D4D25), LV_PART_MAIN);
    lv_obj_set_style_pad_all(r, 4, LV_PART_MAIN);

    txt(r, left, ST_COLOR_TEXT, LV_ALIGN_LEFT_MID, 0, 0);
    return txt(r, right, ST_COLOR_TEXT, LV_ALIGN_RIGHT_MID, 0, 0);
}

tank_widget_t tank_widget_create(lv_obj_t *parent)
{
    tank_widget_t w = {0};
    w.root = parent;

    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    txt(parent, "SZAMBO", TANK_GREEN, LV_ALIGN_TOP_LEFT, 0, 0);
    txt(parent, "Pojemnosc: 10.50 m3", ST_COLOR_TEXT_DIM, LV_ALIGN_TOP_LEFT, 0, 22);

    w.arc = lv_arc_create(parent);
    lv_obj_set_size(w.arc, 122, 122);
    lv_obj_align(w.arc, LV_ALIGN_CENTER, -36, -30);
    lv_arc_set_range(w.arc, 0, 100);
    lv_arc_set_value(w.arc, 72);
    lv_arc_set_bg_angles(w.arc, 135, 45);
    lv_obj_remove_style(w.arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(w.arc, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_style_arc_width(w.arc, 9, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(w.arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_color(w.arc, lv_color_hex(0x073A12), LV_PART_MAIN);

    lv_obj_set_style_arc_width(w.arc, 9, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(w.arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(w.arc, TANK_GREEN, LV_PART_INDICATOR);

    w.percent_label = txt(parent, "72%", ST_COLOR_TEXT, LV_ALIGN_CENTER, -36, -40);
    w.volume_label = txt(parent, "7.56 m3", ST_COLOR_TEXT, LV_ALIGN_CENTER, -36, -7);
    w.capacity_label = NULL;

    w.vertical_bar = lv_bar_create(parent);
    lv_obj_set_size(w.vertical_bar, 18, 105);
    lv_obj_align(w.vertical_bar, LV_ALIGN_RIGHT_MID, -8, -30);
    lv_bar_set_range(w.vertical_bar, 0, 100);
    lv_bar_set_value(w.vertical_bar, 72, LV_ANIM_OFF);

    lv_obj_set_style_bg_color(w.vertical_bar, lv_color_hex(0x10251A), LV_PART_MAIN);
    lv_obj_set_style_bg_color(w.vertical_bar, TANK_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_radius(w.vertical_bar, 5, LV_PART_MAIN);
    lv_obj_set_style_radius(w.vertical_bar, 5, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(w.vertical_bar, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(w.vertical_bar, lv_color_hex(0x2C6E35), LV_PART_MAIN);

    txt(parent, "100%", ST_COLOR_TEXT_DIM, LV_ALIGN_RIGHT_MID, -32, -78);
    txt(parent, "80%",  ST_COLOR_TEXT_DIM, LV_ALIGN_RIGHT_MID, -32, -43);
    txt(parent, "50%",  ST_COLOR_TEXT_DIM, LV_ALIGN_RIGHT_MID, -32, -3);
    txt(parent, "0%",   ST_COLOR_TEXT_DIM, LV_ALIGN_RIGHT_MID, -32, 47);

    w.info_80_value = row(parent, "Do poziomu 80%", "1.05 m3", -76);
    lv_obj_set_style_text_color(w.info_80_value, TANK_YELLOW, LV_PART_MAIN);

    w.info_forecast_value = row(parent, "Prognoza", "5 dni", -49);
    w.info_last_empty_value = row(parent, "Oproznienie", "18 dni", -22);

    w.status_label = txt(parent, "✓  Poziom OK", TANK_GREEN, LV_ALIGN_BOTTOM_MID, 0, -2);

    return w;
}

void tank_widget_set_data(tank_widget_t *widget, int percent, float volume_m3, float capacity_m3)
{
    if (!widget) return;

    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    lv_color_t color = TANK_GREEN;
    const char *status = "✓  Poziom OK";

    if (percent >= 90) {
        color = TANK_RED;
        status = "⚠  ALARM";
    } else if (percent >= 80) {
        color = TANK_YELLOW;
        status = "⚠  Ostrzezenie";
    }

    char b[64];

    snprintf(b, sizeof(b), "%d%%", percent);
    lv_label_set_text(widget->percent_label, b);

    snprintf(b, sizeof(b), "%.2f m3", volume_m3);
    lv_label_set_text(widget->volume_label, b);

    lv_arc_set_value(widget->arc, percent);
    lv_bar_set_value(widget->vertical_bar, percent, LV_ANIM_ON);

    lv_obj_set_style_arc_color(widget->arc, color, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(widget->vertical_bar, color, LV_PART_INDICATOR);

    lv_label_set_text(widget->status_label, status);
    lv_obj_set_style_text_color(widget->status_label, color, LV_PART_MAIN);
}