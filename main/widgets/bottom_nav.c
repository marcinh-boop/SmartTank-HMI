#include "bottom_nav.h"
#include "theme.h"

#include <stdint.h>
#include <string.h>

#define NAV_BG             lv_color_hex(0x08131F)
#define NAV_ACTIVE_BG      lv_color_hex(0x12314A)
#define NAV_ACTIVE_BORDER  lv_color_hex(0x2EA8FF)
#define NAV_TEXT           lv_color_hex(0x8FA3B8)
#define NAV_TEXT_ACTIVE    lv_color_hex(0xFFFFFF)

static const char *NAV_NAMES[NAV_ITEM_COUNT] = {
    "Pulpit",
    "Historia",
    "Alarmy",
    "Ustawienia",
    "Serwis",
    "Informacje"
};

static bottom_nav_t s_nav;

static void apply_button_style(
    bottom_nav_t *nav,
    bottom_nav_page_t page)
{
    const bool is_active = (page == nav->active_page);
    const lv_color_t bg_color = is_active ? NAV_ACTIVE_BG : NAV_BG;
    const lv_color_t text_color = is_active ? NAV_TEXT_ACTIVE : NAV_TEXT;
    const lv_opa_t border_opa = is_active ? LV_OPA_COVER : LV_OPA_TRANSP;

    lv_obj_t *button = nav->buttons[page];
    lv_obj_t *label = nav->labels[page];

    lv_obj_set_style_bg_color(button, bg_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(button, bg_color, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);

    /* Szerokosc obramowania jest zawsze taka sama, zmienia sie tylko widocznosc. */
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_color(button, NAV_ACTIVE_BORDER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(button, NAV_ACTIVE_BORDER, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_opa(button, border_opa, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(button, border_opa, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_set_style_transform_width(button, 0, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_transform_height(button, 0, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_translate_x(button, 0, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_translate_y(button, 0, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_set_style_text_color(label, text_color, LV_PART_MAIN);
}

void bottom_nav_set_active(
    bottom_nav_t *nav,
    bottom_nav_page_t active_page)
{
    if (nav == NULL || active_page >= NAV_ITEM_COUNT) {
        return;
    }

    nav->active_page = active_page;

    for (int i = 0; i < NAV_ITEM_COUNT; i++) {
        apply_button_style(nav, (bottom_nav_page_t)i);
    }
}

static void nav_button_event_cb(lv_event_t *event)
{
    bottom_nav_page_t page = (bottom_nav_page_t)(intptr_t)
        lv_event_get_user_data(event);

    bottom_nav_set_active(&s_nav, page);
}

bottom_nav_t bottom_nav_create(
    lv_obj_t *parent,
    bottom_nav_page_t active_page)
{
    memset(&s_nav, 0, sizeof(s_nav));

    s_nav.root = parent;
    s_nav.active_page = active_page;

    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLL_CHAIN_VER);

    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        parent,
        LV_FLEX_ALIGN_SPACE_EVENLY,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    lv_obj_set_style_pad_left(parent, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_right(parent, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_top(parent, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(parent, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_column(parent, 3, LV_PART_MAIN);

    for (int i = 0; i < NAV_ITEM_COUNT; i++) {
        s_nav.buttons[i] = lv_btn_create(parent);

        lv_obj_remove_style_all(s_nav.buttons[i]);
        lv_obj_clear_flag(s_nav.buttons[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(s_nav.buttons[i], LV_OBJ_FLAG_SCROLL_ELASTIC);
        lv_obj_clear_flag(s_nav.buttons[i], LV_OBJ_FLAG_SCROLL_MOMENTUM);

        lv_obj_set_size(s_nav.buttons[i], 118, 38);
        lv_obj_set_style_radius(s_nav.buttons[i], 7, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(s_nav.buttons[i], 0, LV_PART_MAIN);
        lv_obj_set_style_outline_width(s_nav.buttons[i], 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(s_nav.buttons[i], 0, LV_PART_MAIN);

        s_nav.labels[i] = lv_label_create(s_nav.buttons[i]);
        lv_label_set_text(s_nav.labels[i], NAV_NAMES[i]);

        lv_obj_set_style_text_font(
            s_nav.labels[i],
            &lv_font_montserrat_14,
            LV_PART_MAIN
        );

        lv_obj_center(s_nav.labels[i]);

        lv_obj_add_event_cb(
            s_nav.buttons[i],
            nav_button_event_cb,
            LV_EVENT_RELEASED,
            (void *)(intptr_t)i
        );
    }

    bottom_nav_set_active(&s_nav, active_page);

    return s_nav;
}
