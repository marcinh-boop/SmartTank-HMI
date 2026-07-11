#include "bottom_nav.h"
#include "screen_service.h"
#include "screen_settings.h"
#include "screen_settings_weather.h"
#include "screen_weather_location.h"
#include "theme.h"

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

static const char *NAV_ICONS[NAV_ITEM_COUNT] = {
    LV_SYMBOL_HOME,
    LV_SYMBOL_LIST,
    LV_SYMBOL_BELL,
    LV_SYMBOL_SETTINGS,
    LV_SYMBOL_EYE_OPEN,
    LV_SYMBOL_FILE
};

static void apply_button_style(bottom_nav_t *nav, bottom_nav_page_t page)
{
    const bool is_active = (page == nav->active_page);
    const lv_color_t bg_color = is_active ? NAV_ACTIVE_BG : NAV_BG;
    const lv_color_t text_color = is_active ? NAV_TEXT_ACTIVE : NAV_TEXT;
    const lv_color_t icon_color = is_active ? NAV_ACTIVE_BORDER : NAV_TEXT;

    lv_obj_t *button = nav->buttons[page];
    lv_obj_t *icon = nav->icons[page];
    lv_obj_t *label = nav->labels[page];

    lv_obj_set_style_bg_color(button, bg_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(button, bg_color, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(button, NAV_ACTIVE_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_opa(button, is_active ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);

    lv_obj_set_style_transform_width(button, 0, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_transform_height(button, 0, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_translate_x(button, 0, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_translate_y(button, 0, LV_PART_MAIN | LV_STATE_PRESSED);

    lv_obj_set_style_text_color(icon, icon_color, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, text_color, LV_PART_MAIN);
}

void bottom_nav_set_active(bottom_nav_t *nav, bottom_nav_page_t active_page)
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
    bottom_nav_button_ctx_t *ctx = lv_event_get_user_data(event);

    if (ctx == NULL || ctx->nav == NULL) {
        return;
    }

    bottom_nav_t *nav = ctx->nav;
    const bottom_nav_page_t page = ctx->page;

    if (page == nav->active_page) {
        return;
    }

    if (page == NAV_SETTINGS) {
        screen_service_hide();
        screen_weather_location_hide();
        screen_settings_open(lv_scr_act());
        screen_settings_weather_attach(lv_scr_act());
        bottom_nav_set_active(nav, page);
        return;
    }

    if (page == NAV_SERVICE) {
        screen_weather_location_hide();
        screen_settings_hide();
        screen_service_open(lv_scr_act());
        bottom_nav_set_active(nav, page);
        return;
    }

    if (nav->change_cb != NULL && !nav->change_cb(page)) {
        return;
    }

    screen_weather_location_hide();
    screen_service_hide();
    screen_settings_hide();
    bottom_nav_set_active(nav, page);
}

bottom_nav_t *bottom_nav_create(
    lv_obj_t *parent,
    bottom_nav_page_t active_page,
    bottom_nav_change_cb_t change_cb)
{
    bottom_nav_t *nav = lv_mem_alloc(sizeof(bottom_nav_t));
    if (nav == NULL) {
        return NULL;
    }

    memset(nav, 0, sizeof(*nav));

    nav->root = parent;
    nav->active_page = active_page;
    nav->change_cb = change_cb;

    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLL_MOMENTUM);

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
        nav->buttons[i] = lv_btn_create(parent);
        lv_obj_remove_style_all(nav->buttons[i]);

        lv_obj_set_size(nav->buttons[i], 118, 42);
        lv_obj_set_style_radius(nav->buttons[i], 7, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(nav->buttons[i], 0, LV_PART_MAIN);
        lv_obj_set_style_outline_width(nav->buttons[i], 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(nav->buttons[i], 0, LV_PART_MAIN);

        nav->icons[i] = lv_label_create(nav->buttons[i]);
        lv_label_set_text(nav->icons[i], NAV_ICONS[i]);
        lv_obj_set_style_text_font(nav->icons[i], &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_align(nav->icons[i], LV_ALIGN_TOP_MID, 0, -1);
        lv_obj_clear_flag(nav->icons[i], LV_OBJ_FLAG_CLICKABLE);

        nav->labels[i] = lv_label_create(nav->buttons[i]);
        lv_label_set_text(nav->labels[i], NAV_NAMES[i]);
        lv_obj_set_style_text_font(nav->labels[i], &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(nav->labels[i], LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_clear_flag(nav->labels[i], LV_OBJ_FLAG_CLICKABLE);

        nav->contexts[i].nav = nav;
        nav->contexts[i].page = (bottom_nav_page_t)i;

        lv_obj_add_event_cb(
            nav->buttons[i],
            nav_button_event_cb,
            LV_EVENT_RELEASED,
            &nav->contexts[i]
        );
    }

    bottom_nav_set_active(nav, active_page);
    return nav;
}
