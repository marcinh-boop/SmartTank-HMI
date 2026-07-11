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

static lv_obj_t *find_settings_top_bar(lv_obj_t *screen, lv_obj_t **settings_root)
{
    const uint32_t screen_children = lv_obj_get_child_cnt(screen);

    for (uint32_t root_index = 0U; root_index < screen_children; root_index++) {
        lv_obj_t *root = lv_obj_get_child(screen, root_index);
        if (root == NULL ||
            lv_obj_get_width(root) != 800 ||
            lv_obj_get_height(root) != 398) {
            continue;
        }

        const uint32_t root_children = lv_obj_get_child_cnt(root);
        for (uint32_t child_index = 0U; child_index < root_children; child_index++) {
            lv_obj_t *top = lv_obj_get_child(root, child_index);
            if (top == NULL ||
                lv_obj_get_width(top) != 760 ||
                lv_obj_get_height(top) != 50) {
                continue;
            }

            const uint32_t top_children = lv_obj_get_child_cnt(top);
            for (uint32_t label_index = 0U; label_index < top_children; label_index++) {
                lv_obj_t *label = lv_obj_get_child(top, label_index);
                if (label_has_text(label, "USTAWIENIA")) {
                    if (settings_root != NULL) {
                        *settings_root = root;
                    }
                    return top;
                }
            }
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

    lv_obj_t *settings_root = NULL;
    lv_obj_t *top = find_settings_top_bar(screen, &settings_root);
    if (top == NULL || settings_root == NULL) {
        return;
    }

    if (s_attached_root == settings_root && s_button != NULL) {
        return;
    }

    const uint32_t child_count = lv_obj_get_child_cnt(top);
    for (uint32_t index = 0U; index < child_count; index++) {
        lv_obj_t *child = lv_obj_get_child(top, index);
        if (label_has_text(child, "WIFI 2.4 GHz")) {
            lv_obj_add_flag(child, LV_OBJ_FLAG_HIDDEN);
        }
    }

    s_button = lv_btn_create(top);
    lv_obj_set_size(s_button, 146, 34);
    lv_obj_align(s_button, LV_ALIGN_RIGHT_MID, -8, 0);
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
    lv_obj_add_event_cb(
        s_button,
        open_location_event_cb,
        LV_EVENT_RELEASED,
        NULL
    );

    lv_obj_t *label = lv_label_create(s_button);
    lv_label_set_text(label, LV_SYMBOL_GPS " LOKALIZACJA");
    lv_obj_set_style_text_color(label, ST_COLOR_TEXT, LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_center(label);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);

    s_attached_root = settings_root;
}
