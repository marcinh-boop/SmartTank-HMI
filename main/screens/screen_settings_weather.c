#include "screen_settings_weather.h"

#include <string.h>

#include "screen_weather_location.h"
#include "theme.h"

#define SHORTCUT_BLUE  lv_color_hex(0x2EA8FF)
#define SHORTCUT_BG    lv_color_hex(0x12314A)
#define SHORTCUT_PRESS lv_color_hex(0x1A4566)

static lv_obj_t *s_attached_root;
static lv_obj_t *s_button;

static bool label_has_text(lv_obj_t *object, const char *text)
{
    return object != NULL &&
           lv_obj_check_type(object, &lv_label_class) &&
           strcmp(lv_label_get_text(object), text) == 0;
}

static lv_obj_t *find_settings_root(lv_obj_t *screen)
{
    const uint32_t screen_children = lv_obj_get_child_cnt(screen);

    for (uint32_t root_index = 0U; root_index < screen_children; root_index++) {
        lv_obj_t *root = lv_obj_get_child(screen, (int32_t)root_index);
        if (root == NULL ||
            lv_obj_get_width(root) != 800 ||
            lv_obj_get_height(root) != 398) {
            continue;
        }

        const uint32_t root_children = lv_obj_get_child_cnt(root);
        for (uint32_t child_index = 0U; child_index < root_children; child_index++) {
            lv_obj_t *container = lv_obj_get_child(root, (int32_t)child_index);
            if (container == NULL) {
                continue;
            }

            if (label_has_text(container, "USTAWIENIA")) {
                return root;
            }

            const uint32_t nested_count = lv_obj_get_child_cnt(container);
            for (uint32_t nested_index = 0U; nested_index < nested_count; nested_index++) {
                lv_obj_t *nested = lv_obj_get_child(container, (int32_t)nested_index);
                if (label_has_text(nested, "USTAWIENIA")) {
                    return root;
                }
            }
        }
    }

    return NULL;
}

static lv_obj_t *find_status_panel(lv_obj_t *settings_root)
{
    const uint32_t child_count = lv_obj_get_child_cnt(settings_root);

    for (uint32_t index = 0U; index < child_count; index++) {
        lv_obj_t *candidate = lv_obj_get_child(settings_root, (int32_t)index);
        if (candidate != NULL &&
            lv_obj_get_width(candidate) == 245 &&
            lv_obj_get_height(candidate) == 320 &&
            lv_obj_get_x(candidate) < 100) {
            return candidate;
        }
    }

    return NULL;
}

static void open_location_event_cb(lv_event_t *event)
{
    (void)event;
    screen_weather_location_open(lv_scr_act());
}

void screen_settings_weather_attach(lv_obj_t *screen)
{
    if (screen == NULL) {
        return;
    }

    lv_obj_update_layout(screen);

    lv_obj_t *settings_root = find_settings_root(screen);
    if (settings_root == NULL) {
        return;
    }

    if (s_attached_root == settings_root && s_button != NULL) {
        lv_obj_clear_flag(s_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_button);
        return;
    }

    lv_obj_t *status_panel = find_status_panel(settings_root);
    if (status_panel == NULL) {
        return;
    }

    s_button = lv_btn_create(status_panel);
    lv_obj_set_size(s_button, 88, 30);
    lv_obj_align(s_button, LV_ALIGN_TOP_RIGHT, 0, -4);
    lv_obj_set_style_bg_color(s_button, SHORTCUT_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_color(
        s_button,
        SHORTCUT_PRESS,
        LV_PART_MAIN | LV_STATE_PRESSED
    );
    lv_obj_set_style_radius(s_button, 7, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_button, SHORTCUT_BLUE, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(s_button, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_button, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(
        s_button,
        open_location_event_cb,
        LV_EVENT_RELEASED,
        NULL
    );

    lv_obj_t *label = lv_label_create(s_button);
    lv_label_set_text(label, LV_SYMBOL_GPS " MIASTO");
    lv_obj_set_style_text_color(label, ST_COLOR_TEXT, LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_center(label);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);

    s_attached_root = settings_root;
}