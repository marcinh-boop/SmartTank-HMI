#include "bottom_nav.h"

#include "alarm_service.h"
#include "screen_alarms.h"
#include "screen_info.h"
#include "screen_service.h"
#include "screen_settings.h"
#include "screen_settings_weather.h"
#include "screen_weather_location.h"
#include "theme.h"

#include <stdio.h>
#include <string.h>

#define NAV_BG             lv_color_hex(0x08131F)
#define NAV_ACTIVE_BG      lv_color_hex(0x12314A)
#define NAV_ACTIVE_BORDER  lv_color_hex(0x2EA8FF)
#define NAV_TEXT           lv_color_hex(0x8FA3B8)
#define NAV_TEXT_ACTIVE    lv_color_hex(0xFFFFFF)
#define NAV_ALARM_RED      lv_color_hex(0xFF4D4D)
#define NAV_ALARM_YELLOW   lv_color_hex(0xFFC247)

static const char *NAV_NAMES[NAV_ITEM_COUNT] = {
    "Pulpit",
    "Historia",
    "Alarmy",
    "Ustawienia",
    "Serwis",
    "Informacje"
};

static const char *NAV_SYMBOLS[NAV_ITEM_COUNT] = {
    LV_SYMBOL_HOME,
    NULL,
    LV_SYMBOL_BELL,
    LV_SYMBOL_SETTINGS,
    NULL,
    NULL
};

static const lv_point_t SERVICE_SHAFT_POINTS[] = {
    {5, 17},
    {16, 6},
};

static const lv_point_t SERVICE_JAW_TOP_POINTS[] = {
    {15, 7},
    {21, 1},
};

static const lv_point_t SERVICE_JAW_BOTTOM_POINTS[] = {
    {15, 7},
    {22, 10},
};

static void set_icon_color_recursive(lv_obj_t *obj, lv_color_t color)
{
    if (obj == NULL) {
        return;
    }

    lv_obj_set_style_text_color(obj, color, LV_PART_MAIN);
    lv_obj_set_style_line_color(obj, color, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, color, LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, color, LV_PART_MAIN);

    const uint32_t child_count = lv_obj_get_child_cnt(obj);
    for (uint32_t index = 0U; index < child_count; index++) {
        set_icon_color_recursive(
            lv_obj_get_child(obj, (int32_t)index),
            color
        );
    }
}

static lv_obj_t *create_icon_root(lv_obj_t *parent)
{
    /*
     * Korzeń niestandardowej ikony jest pustą etykietą. Górny pasek
     * rozpoznaje przyciski nawigacji przez bezpieczne odczyty etykiet;
     * zwykły lv_obj powodował wywołanie lv_label_get_text na złej klasie.
     */
    lv_obj_t *root = lv_label_create(parent);
    lv_label_set_text(root, "");
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, 26, 22);
    lv_obj_align(root, LV_ALIGN_TOP_MID, 0, -1);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    return root;
}

static lv_obj_t *create_shape(
    lv_obj_t *parent,
    int x,
    int y,
    int width,
    int height)
{
    lv_obj_t *shape = lv_obj_create(parent);
    lv_obj_remove_style_all(shape);
    lv_obj_set_size(shape, width, height);
    lv_obj_align(shape, LV_ALIGN_TOP_LEFT, x, y);
    lv_obj_set_style_bg_color(shape, NAV_TEXT, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(shape, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(shape, 1, LV_PART_MAIN);
    lv_obj_clear_flag(shape, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(shape, LV_OBJ_FLAG_SCROLLABLE);
    return shape;
}

static lv_obj_t *create_history_icon(lv_obj_t *parent)
{
    lv_obj_t *root = create_icon_root(parent);

    create_shape(root, 2, 2, 2, 17);
    create_shape(root, 2, 17, 21, 2);
    create_shape(root, 7, 12, 3, 5);
    create_shape(root, 12, 8, 3, 9);
    create_shape(root, 17, 4, 3, 13);

    return root;
}

static lv_obj_t *create_service_line(
    lv_obj_t *parent,
    const lv_point_t *points,
    uint16_t point_count,
    int width)
{
    lv_obj_t *line = lv_line_create(parent);
    lv_line_set_points(line, points, point_count);
    lv_obj_set_style_line_color(line, NAV_TEXT, LV_PART_MAIN);
    lv_obj_set_style_line_width(line, width, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(line, true, LV_PART_MAIN);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
    return line;
}

static lv_obj_t *create_service_icon(lv_obj_t *parent)
{
    lv_obj_t *root = create_icon_root(parent);

    create_service_line(
        root,
        SERVICE_SHAFT_POINTS,
        sizeof(SERVICE_SHAFT_POINTS) / sizeof(SERVICE_SHAFT_POINTS[0]),
        4
    );
    create_service_line(
        root,
        SERVICE_JAW_TOP_POINTS,
        sizeof(SERVICE_JAW_TOP_POINTS) / sizeof(SERVICE_JAW_TOP_POINTS[0]),
        4
    );
    create_service_line(
        root,
        SERVICE_JAW_BOTTOM_POINTS,
        sizeof(SERVICE_JAW_BOTTOM_POINTS) / sizeof(SERVICE_JAW_BOTTOM_POINTS[0]),
        4
    );

    lv_obj_t *ring = lv_obj_create(root);
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, 8, 8);
    lv_obj_align(ring, LV_ALIGN_TOP_LEFT, 1, 13);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(ring, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(ring, NAV_TEXT, LV_PART_MAIN);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_CLICKABLE);

    return root;
}

static lv_obj_t *create_info_icon(lv_obj_t *parent)
{
    lv_obj_t *root = create_icon_root(parent);
    lv_obj_set_size(root, 22, 22);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(root, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(root, NAV_TEXT, LV_PART_MAIN);
    lv_obj_set_style_radius(root, LV_RADIUS_CIRCLE, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(root);
    lv_label_set_text(label, "i");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, NAV_TEXT, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 1);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);

    return root;
}

static lv_obj_t *create_nav_icon(
    lv_obj_t *parent,
    bottom_nav_page_t page)
{
    if (page == NAV_HISTORY) {
        return create_history_icon(parent);
    }

    if (page == NAV_SERVICE) {
        return create_service_icon(parent);
    }

    if (page == NAV_INFO) {
        return create_info_icon(parent);
    }

    lv_obj_t *icon = lv_label_create(parent);
    lv_label_set_text(icon, NAV_SYMBOLS[page]);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, -1);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);
    return icon;
}

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

    set_icon_color_recursive(icon, icon_color);
    lv_obj_set_style_text_color(label, text_color, LV_PART_MAIN);
}

static void refresh_alarm_badge(bottom_nav_t *nav)
{
    if (nav == NULL || nav->alarm_badge == NULL) {
        return;
    }

    alarm_service_snapshot_t snapshot;
    alarm_service_get_snapshot(&snapshot);

    if (!snapshot.started || snapshot.active_count == 0U) {
        lv_obj_add_flag(nav->alarm_badge, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    char text[4];
    if (snapshot.active_count > 9U) {
        snprintf(text, sizeof(text), "9+");
    } else {
        snprintf(text, sizeof(text), "%u", (unsigned int)snapshot.active_count);
    }

    lv_label_set_text(nav->alarm_badge, text);
    lv_obj_set_style_bg_color(
        nav->alarm_badge,
        snapshot.unacknowledged_count > 0U ? NAV_ALARM_RED : NAV_ALARM_YELLOW,
        LV_PART_MAIN
    );
    lv_obj_clear_flag(nav->alarm_badge, LV_OBJ_FLAG_HIDDEN);
}

static void alarm_badge_timer_cb(lv_timer_t *timer)
{
    bottom_nav_t *nav = timer != NULL ? timer->user_data : NULL;
    refresh_alarm_badge(nav);
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

static void hide_overlay_screens(void)
{
    screen_alarms_hide();
    screen_info_hide();
    screen_weather_location_hide();
    screen_service_hide();
    screen_settings_hide();
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

    if (page == NAV_ALARMS) {
        screen_info_hide();
        screen_weather_location_hide();
        screen_service_hide();
        screen_settings_hide();
        screen_alarms_open(lv_scr_act());
        bottom_nav_set_active(nav, page);
        return;
    }

    if (page == NAV_SETTINGS) {
        screen_alarms_hide();
        screen_info_hide();
        screen_service_hide();
        screen_weather_location_hide();
        screen_settings_open(lv_scr_act());
        screen_settings_weather_attach(lv_scr_act());
        bottom_nav_set_active(nav, page);
        return;
    }

    if (page == NAV_SERVICE) {
        screen_alarms_hide();
        screen_info_hide();
        screen_weather_location_hide();
        screen_settings_hide();
        screen_service_open(lv_scr_act());
        bottom_nav_set_active(nav, page);
        return;
    }

    if (page == NAV_INFO) {
        screen_alarms_hide();
        screen_weather_location_hide();
        screen_service_hide();
        screen_settings_hide();
        screen_info_open(lv_scr_act());
        bottom_nav_set_active(nav, page);
        return;
    }

    if (nav->change_cb != NULL && !nav->change_cb(page)) {
        return;
    }

    hide_overlay_screens();
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

        nav->icons[i] = create_nav_icon(
            nav->buttons[i],
            (bottom_nav_page_t)i
        );

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

    nav->alarm_badge = lv_label_create(nav->buttons[NAV_ALARMS]);
    lv_label_set_text(nav->alarm_badge, "0");
    lv_obj_set_size(nav->alarm_badge, 20, 18);
    lv_obj_align(nav->alarm_badge, LV_ALIGN_TOP_RIGHT, -18, -2);
    lv_obj_set_style_text_font(nav->alarm_badge, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_set_style_text_color(nav->alarm_badge, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_align(nav->alarm_badge, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(nav->alarm_badge, NAV_ALARM_RED, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(nav->alarm_badge, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(nav->alarm_badge, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_pad_top(nav->alarm_badge, 1, LV_PART_MAIN);
    lv_obj_set_style_border_width(nav->alarm_badge, 0, LV_PART_MAIN);
    lv_obj_clear_flag(nav->alarm_badge, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(nav->alarm_badge, LV_OBJ_FLAG_HIDDEN);

    nav->alarm_badge_timer = lv_timer_create(alarm_badge_timer_cb, 500, nav);

    bottom_nav_set_active(nav, active_page);
    refresh_alarm_badge(nav);
    return nav;
}
