#include "panel_icons.h"

#include <string.h>

#include "theme.h"

#define ICON_TANK_COLOR     lv_color_hex(0x39D12F)
#define ICON_WELL_COLOR     lv_color_hex(0x2EA8FF)
#define ICON_WEATHER_COLOR  lv_color_hex(0xFFC247)
#define ICON_BADGE_BG       lv_color_hex(0x0A1A27)
#define ICON_BADGE_BORDER   lv_color_hex(0x294256)
#define ICON_CLOUD_COLOR    lv_color_hex(0xDCEAF4)

static bool s_applied;

static void make_noninteractive(lv_obj_t *obj)
{
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t *create_badge(lv_obj_t *parent, lv_color_t color)
{
    lv_obj_t *badge = lv_obj_create(parent);
    lv_obj_set_size(badge, 36, 36);
    lv_obj_align(badge, LV_ALIGN_TOP_RIGHT, -1, -1);
    make_noninteractive(badge);

    lv_obj_set_style_bg_color(badge, ICON_BADGE_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(badge, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(badge, color, LV_PART_MAIN);
    lv_obj_set_style_radius(badge, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(badge, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(badge, 0, LV_PART_MAIN);

    return badge;
}

static void create_tank_badge(lv_obj_t *card)
{
    lv_obj_t *badge = create_badge(card, ICON_TANK_COLOR);

    lv_obj_t *neck = lv_obj_create(badge);
    lv_obj_set_size(neck, 8, 4);
    lv_obj_align(neck, LV_ALIGN_TOP_MID, 0, 5);
    make_noninteractive(neck);
    lv_obj_set_style_bg_color(neck, ICON_TANK_COLOR, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(neck, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(neck, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(neck, 1, LV_PART_MAIN);

    lv_obj_t *body = lv_obj_create(badge);
    lv_obj_set_size(body, 19, 22);
    lv_obj_align(body, LV_ALIGN_CENTER, 0, 3);
    make_noninteractive(body);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(body, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(body, ICON_TANK_COLOR, LV_PART_MAIN);
    lv_obj_set_style_radius(body, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_all(body, 0, LV_PART_MAIN);

    lv_obj_t *level = lv_obj_create(body);
    lv_obj_set_size(level, 11, 7);
    lv_obj_align(level, LV_ALIGN_BOTTOM_MID, 0, -3);
    make_noninteractive(level);
    lv_obj_set_style_bg_color(level, ICON_TANK_COLOR, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(level, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(level, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(level, 2, LV_PART_MAIN);
}

static void create_well_badge(lv_obj_t *card)
{
    lv_obj_t *badge = create_badge(card, ICON_WELL_COLOR);

    lv_obj_t *symbol = lv_label_create(badge);
    lv_label_set_text(symbol, LV_SYMBOL_DOWNLOAD);
    lv_obj_set_style_text_font(symbol, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(symbol, ICON_WELL_COLOR, LV_PART_MAIN);
    lv_obj_align(symbol, LV_ALIGN_CENTER, 0, -2);
    make_noninteractive(symbol);

    lv_obj_t *water_line = lv_obj_create(badge);
    lv_obj_set_size(water_line, 18, 2);
    lv_obj_align(water_line, LV_ALIGN_BOTTOM_MID, 0, -6);
    make_noninteractive(water_line);
    lv_obj_set_style_bg_color(water_line, ICON_WELL_COLOR, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(water_line, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(water_line, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(water_line, 1, LV_PART_MAIN);
}

static lv_obj_t *create_solid_shape(
    lv_obj_t *parent,
    int width,
    int height,
    int x,
    int y,
    lv_color_t color,
    int radius)
{
    lv_obj_t *shape = lv_obj_create(parent);
    lv_obj_set_size(shape, width, height);
    lv_obj_set_pos(shape, x, y);
    make_noninteractive(shape);
    lv_obj_set_style_bg_color(shape, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(shape, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(shape, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(shape, radius, LV_PART_MAIN);
    lv_obj_set_style_pad_all(shape, 0, LV_PART_MAIN);
    return shape;
}

static void create_sun_badge(lv_obj_t *card)
{
    lv_obj_t *badge = create_badge(card, ICON_WEATHER_COLOR);

    create_solid_shape(badge, 13, 13, 11, 11, ICON_WEATHER_COLOR, LV_RADIUS_CIRCLE);
    create_solid_shape(badge, 3, 6, 16, 3, ICON_WEATHER_COLOR, 1);
    create_solid_shape(badge, 3, 6, 16, 27, ICON_WEATHER_COLOR, 1);
    create_solid_shape(badge, 6, 3, 3, 16, ICON_WEATHER_COLOR, 1);
    create_solid_shape(badge, 6, 3, 27, 16, ICON_WEATHER_COLOR, 1);
}

static void hide_weather_placeholders(lv_obj_t *card)
{
    const uint32_t child_count = lv_obj_get_child_cnt(card);

    for (uint32_t i = 0U; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(card, i);
        if (child == NULL || !lv_obj_check_type(child, &lv_label_class)) {
            continue;
        }

        const char *text = lv_label_get_text(child);
        if (text != NULL &&
            (strcmp(text, "SUN") == 0 || strcmp(text, "CLOUD") == 0)) {
            lv_obj_add_flag(child, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void create_large_weather_icon(lv_obj_t *card)
{
    hide_weather_placeholders(card);

    lv_obj_t *root = lv_obj_create(card);
    lv_obj_set_size(root, 82, 82);
    lv_obj_align(root, LV_ALIGN_TOP_LEFT, -2, 58);
    make_noninteractive(root);
    lv_obj_remove_style_all(root);

    create_solid_shape(root, 25, 25, 43, 3, ICON_WEATHER_COLOR, LV_RADIUS_CIRCLE);
    create_solid_shape(root, 3, 8, 54, 0, ICON_WEATHER_COLOR, 1);
    create_solid_shape(root, 3, 8, 54, 25, ICON_WEATHER_COLOR, 1);
    create_solid_shape(root, 8, 3, 36, 14, ICON_WEATHER_COLOR, 1);
    create_solid_shape(root, 8, 3, 67, 14, ICON_WEATHER_COLOR, 1);

    create_solid_shape(root, 58, 18, 5, 50, ICON_CLOUD_COLOR, 9);
    create_solid_shape(root, 28, 28, 11, 37, ICON_CLOUD_COLOR, LV_RADIUS_CIRCLE);
    create_solid_shape(root, 37, 37, 28, 27, ICON_CLOUD_COLOR, LV_RADIUS_CIRCLE);
    create_solid_shape(root, 24, 24, 51, 42, ICON_CLOUD_COLOR, LV_RADIUS_CIRCLE);
}

static lv_obj_t *find_dashboard_layer(lv_obj_t *screen)
{
    const uint32_t screen_child_count = lv_obj_get_child_cnt(screen);

    for (uint32_t i = 0U; i < screen_child_count; i++) {
        lv_obj_t *candidate = lv_obj_get_child(screen, i);
        if (candidate == NULL ||
            lv_obj_get_width(candidate) != 800 ||
            lv_obj_get_height(candidate) != 340) {
            continue;
        }

        uint32_t card_count = 0U;
        const uint32_t candidate_child_count = lv_obj_get_child_cnt(candidate);
        for (uint32_t j = 0U; j < candidate_child_count; j++) {
            lv_obj_t *child = lv_obj_get_child(candidate, j);
            if (child != NULL &&
                lv_obj_get_width(child) == 245 &&
                lv_obj_get_height(child) == 330) {
                card_count++;
            }
        }

        if (card_count == 3U) {
            return candidate;
        }
    }

    return NULL;
}

static void decorate_dashboard_cards(lv_obj_t *screen)
{
    lv_obj_t *dashboard = find_dashboard_layer(screen);
    if (dashboard == NULL) {
        return;
    }

    lv_obj_t *cards[3] = {0};
    uint32_t card_count = 0U;
    const uint32_t child_count = lv_obj_get_child_cnt(dashboard);

    for (uint32_t i = 0U; i < child_count && card_count < 3U; i++) {
        lv_obj_t *child = lv_obj_get_child(dashboard, i);
        if (child != NULL &&
            lv_obj_get_width(child) == 245 &&
            lv_obj_get_height(child) == 330) {
            cards[card_count++] = child;
        }
    }

    for (uint32_t i = 0U; i < card_count; i++) {
        for (uint32_t j = i + 1U; j < card_count; j++) {
            if (lv_obj_get_x(cards[j]) < lv_obj_get_x(cards[i])) {
                lv_obj_t *temporary = cards[i];
                cards[i] = cards[j];
                cards[j] = temporary;
            }
        }
    }

    if (card_count == 3U) {
        create_tank_badge(cards[0]);
        create_well_badge(cards[1]);
        create_sun_badge(cards[2]);
        create_large_weather_icon(cards[2]);
    }
}

void panel_icons_apply(lv_obj_t *screen)
{
    if (screen == NULL || s_applied) {
        return;
    }

    lv_obj_update_layout(screen);
    decorate_dashboard_cards(screen);
    s_applied = true;
}
