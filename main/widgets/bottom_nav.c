#include "bottom_nav.h"
#include "theme.h"

#include <stdint.h>

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

static bottom_nav_t *s_nav = NULL;

static void apply_button_style(
    bottom_nav_t *nav,
    bottom_nav_page_t page)
{
    const bool is_active = (page == nav->active_page);

    lv_obj_set_style_bg_color(
        nav->buttons[page],
        is_active ? NAV_ACTIVE_BG : NAV_BG,
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        nav->buttons[page],
        LV_OPA_COVER,
        LV_PART_MAIN
    );

    lv_obj_set_style_border_width(
        nav->buttons[page],
        is_active ? 1 : 0,
        LV_PART_MAIN
    );

    lv_obj_set_style_border_color(
        nav->buttons[page],
        NAV_ACTIVE_BORDER,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        nav->labels[page],
        is_active ? NAV_TEXT_ACTIVE : NAV_TEXT,
        LV_PART_MAIN
    );
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
    if (s_nav == NULL) {
        return;
    }

    bottom_nav_page_t page = (bottom_nav_page_t)(intptr_t)
        lv_event_get_user_data(event);

    bottom_nav_set_active(s_nav, page);
}

bottom_nav_t bottom_nav_create(
    lv_obj_t *parent,
    bottom_nav_page_t active_page)
{
    bottom_nav_t nav = {0};

    nav.root = parent;
    nav.active_page = active_page;

    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

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
        nav.buttons[i] = lv_btn_create(parent);

        lv_obj_set_size(nav.buttons[i], 118, 38);
        lv_obj_set_style_radius(nav.buttons[i], 7, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(nav.buttons[i], 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(nav.buttons[i], 0, LV_PART_MAIN);

        nav.labels[i] = lv_label_create(nav.buttons[i]);
        lv_label_set_text(nav.labels[i], NAV_NAMES[i]);

        lv_obj_set_style_text_font(
            nav.labels[i],
            &lv_font_montserrat_14,
            LV_PART_MAIN
        );

        lv_obj_center(nav.labels[i]);

        lv_obj_add_event_cb(
            nav.buttons[i],
            nav_button_event_cb,
            LV_EVENT_CLICKED,
            (void *)(intptr_t)i
        );
    }

    s_nav = &nav;

    /*
     * s_nav musi wskazywać strukturę istniejącą po wyjściu z funkcji.
     * Dlatego poniżej tworzona jest trwała kopia statyczna.
     */
    static bottom_nav_t persistent_nav;
    persistent_nav = nav;
    s_nav = &persistent_nav;

    bottom_nav_set_active(s_nav, active_page);

    return persistent_nav;
}