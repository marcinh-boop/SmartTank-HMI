#include "panel_icons.h"

#include "theme.h"

#define ICON_TANK_COLOR     lv_color_hex(0x39D12F)
#define ICON_WELL_COLOR     lv_color_hex(0x2EA8FF)
#define ICON_WEATHER_COLOR  lv_color_hex(0xFFC247)
#define ICON_WEATHER_BLUE   lv_color_hex(0x2EA8FF)
#define ICON_WEATHER_CLOUD  lv_color_hex(0xDCEAF4)
#define ICON_WEATHER_RED    lv_color_hex(0xFF5A5A)
#define ICON_BADGE_BG       lv_color_hex(0x0A1A27)

#define FORECAST_ICON_COUNT       4U
#define FORECAST_REFRESH_MS       500U
#define FORECAST_SOURCE_Y_MIN     240
#define FORECAST_SOURCE_Y_MAX     280

typedef enum {
    FORECAST_VARIANT_NONE = 0,
    FORECAST_VARIANT_CLEAR,
    FORECAST_VARIANT_CLOUDY,
    FORECAST_VARIANT_FOG,
    FORECAST_VARIANT_RAIN,
    FORECAST_VARIANT_SNOW,
    FORECAST_VARIANT_STORM,
    FORECAST_VARIANT_COUNT,
} forecast_variant_t;

typedef struct {
    lv_obj_t *source_label;
    lv_obj_t *root;
    lv_obj_t *layers[FORECAST_VARIANT_COUNT];
    forecast_variant_t last_variant;
} forecast_icon_overlay_t;

static bool s_applied;
static forecast_icon_overlay_t s_forecast_icons[FORECAST_ICON_COUNT];
static lv_timer_t *s_forecast_timer;

static void make_noninteractive(lv_obj_t *obj)
{
    if (obj == NULL) {
        return;
    }

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
    lv_obj_set_style_shadow_width(shape, 0, LV_PART_MAIN);
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

static lv_obj_t *create_forecast_layer(lv_obj_t *root)
{
    lv_obj_t *layer = lv_obj_create(root);
    lv_obj_remove_style_all(layer);
    lv_obj_set_size(layer, 34, 32);
    lv_obj_set_pos(layer, 0, 0);
    make_noninteractive(layer);
    lv_obj_add_flag(layer, LV_OBJ_FLAG_HIDDEN);
    return layer;
}

static void create_mini_cloud(lv_obj_t *parent, int y)
{
    create_solid_shape(parent, 25, 8, 4, y + 8, ICON_WEATHER_CLOUD, 4);
    create_solid_shape(parent, 11, 11, 7, y + 3, ICON_WEATHER_CLOUD, LV_RADIUS_CIRCLE);
    create_solid_shape(parent, 15, 15, 14, y, ICON_WEATHER_CLOUD, LV_RADIUS_CIRCLE);
}

static lv_obj_t *create_forecast_clear(lv_obj_t *root)
{
    lv_obj_t *layer = create_forecast_layer(root);
    create_solid_shape(layer, 13, 13, 10, 9, ICON_WEATHER_COLOR, LV_RADIUS_CIRCLE);
    create_solid_shape(layer, 2, 5, 15, 2, ICON_WEATHER_COLOR, 1);
    create_solid_shape(layer, 2, 5, 15, 24, ICON_WEATHER_COLOR, 1);
    create_solid_shape(layer, 5, 2, 3, 14, ICON_WEATHER_COLOR, 1);
    create_solid_shape(layer, 5, 2, 25, 14, ICON_WEATHER_COLOR, 1);
    return layer;
}

static lv_obj_t *create_forecast_cloudy(lv_obj_t *root)
{
    lv_obj_t *layer = create_forecast_layer(root);
    create_solid_shape(layer, 9, 9, 21, 3, ICON_WEATHER_COLOR, LV_RADIUS_CIRCLE);
    create_mini_cloud(layer, 9);
    return layer;
}

static lv_obj_t *create_forecast_fog(lv_obj_t *root)
{
    lv_obj_t *layer = create_forecast_layer(root);
    create_solid_shape(layer, 23, 3, 5, 7, ICON_WEATHER_CLOUD, 1);
    create_solid_shape(layer, 29, 3, 2, 14, ICON_WEATHER_CLOUD, 1);
    create_solid_shape(layer, 21, 3, 7, 21, ICON_WEATHER_CLOUD, 1);
    return layer;
}

static lv_obj_t *create_forecast_rain(lv_obj_t *root)
{
    lv_obj_t *layer = create_forecast_layer(root);
    create_mini_cloud(layer, 3);
    create_solid_shape(layer, 3, 7, 8, 22, ICON_WEATHER_BLUE, 1);
    create_solid_shape(layer, 3, 7, 16, 24, ICON_WEATHER_BLUE, 1);
    create_solid_shape(layer, 3, 7, 24, 22, ICON_WEATHER_BLUE, 1);
    return layer;
}

static lv_obj_t *create_forecast_snow(lv_obj_t *root)
{
    lv_obj_t *layer = create_forecast_layer(root);
    create_mini_cloud(layer, 3);
    create_solid_shape(layer, 4, 4, 7, 23, ICON_WEATHER_BLUE, LV_RADIUS_CIRCLE);
    create_solid_shape(layer, 4, 4, 15, 26, ICON_WEATHER_BLUE, LV_RADIUS_CIRCLE);
    create_solid_shape(layer, 4, 4, 23, 23, ICON_WEATHER_BLUE, LV_RADIUS_CIRCLE);
    return layer;
}

static lv_obj_t *create_forecast_storm(lv_obj_t *root)
{
    lv_obj_t *layer = create_forecast_layer(root);
    create_mini_cloud(layer, 3);
    create_solid_shape(layer, 6, 10, 15, 20, ICON_WEATHER_COLOR, 1);
    create_solid_shape(layer, 10, 4, 12, 24, ICON_WEATHER_COLOR, 1);
    create_solid_shape(layer, 4, 7, 12, 26, ICON_WEATHER_COLOR, 1);
    return layer;
}

static forecast_variant_t forecast_variant_from_label(const char *text)
{
    if (text == NULL || text[0] == '\0') {
        return FORECAST_VARIANT_NONE;
    }

    switch (text[0]) {
        case 'S':
            return FORECAST_VARIANT_CLEAR;
        case 'C':
            return FORECAST_VARIANT_CLOUDY;
        case 'M':
            return FORECAST_VARIANT_FOG;
        case 'R':
            return FORECAST_VARIANT_RAIN;
        case 'N':
            return FORECAST_VARIANT_SNOW;
        case 'B':
            return FORECAST_VARIANT_STORM;
        default:
            return FORECAST_VARIANT_NONE;
    }
}

static void set_forecast_variant(
    forecast_icon_overlay_t *overlay,
    forecast_variant_t variant)
{
    if (overlay == NULL || overlay->root == NULL || overlay->last_variant == variant) {
        return;
    }

    for (uint8_t index = 1U; index < FORECAST_VARIANT_COUNT; index++) {
        if (overlay->layers[index] != NULL) {
            lv_obj_add_flag(overlay->layers[index], LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (variant > FORECAST_VARIANT_NONE && variant < FORECAST_VARIANT_COUNT &&
        overlay->layers[variant] != NULL) {
        lv_obj_clear_flag(overlay->layers[variant], LV_OBJ_FLAG_HIDDEN);
    }

    overlay->last_variant = variant;
}

static void refresh_forecast_icons(void)
{
    for (uint8_t index = 0U; index < FORECAST_ICON_COUNT; index++) {
        forecast_icon_overlay_t *overlay = &s_forecast_icons[index];
        if (overlay->source_label == NULL) {
            continue;
        }

        lv_obj_set_style_text_opa(
            overlay->source_label,
            LV_OPA_TRANSP,
            LV_PART_MAIN
        );

        set_forecast_variant(
            overlay,
            forecast_variant_from_label(lv_label_get_text(overlay->source_label))
        );
    }
}

static void forecast_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    refresh_forecast_icons();
}

static void attach_forecast_icons(lv_obj_t *weather_card)
{
    lv_obj_t *labels[FORECAST_ICON_COUNT] = {0};
    uint8_t found = 0U;
    const uint32_t child_count = lv_obj_get_child_cnt(weather_card);

    for (uint32_t index = 0U; index < child_count && found < FORECAST_ICON_COUNT; index++) {
        lv_obj_t *child = lv_obj_get_child(weather_card, (int32_t)index);
        if (child == NULL || !lv_obj_check_type(child, &lv_label_class)) {
            continue;
        }

        const int y = lv_obj_get_y(child);
        const char *text = lv_label_get_text(child);
        if (y < FORECAST_SOURCE_Y_MIN || y > FORECAST_SOURCE_Y_MAX ||
            text == NULL || text[0] == '\0' || text[1] != '\0') {
            continue;
        }

        labels[found++] = child;
    }

    if (found != FORECAST_ICON_COUNT) {
        return;
    }

    for (uint8_t left = 0U; left < found; left++) {
        for (uint8_t right = left + 1U; right < found; right++) {
            if (lv_obj_get_x(labels[right]) < lv_obj_get_x(labels[left])) {
                lv_obj_t *temporary = labels[left];
                labels[left] = labels[right];
                labels[right] = temporary;
            }
        }
    }

    for (uint8_t index = 0U; index < FORECAST_ICON_COUNT; index++) {
        forecast_icon_overlay_t *overlay = &s_forecast_icons[index];
        overlay->source_label = labels[index];
        overlay->last_variant = FORECAST_VARIANT_COUNT;

        overlay->root = lv_obj_create(weather_card);
        lv_obj_remove_style_all(overlay->root);
        lv_obj_set_size(overlay->root, 34, 32);
        lv_obj_set_pos(
            overlay->root,
            lv_obj_get_x(labels[index]) - 10,
            lv_obj_get_y(labels[index]) - 6
        );
        make_noninteractive(overlay->root);

        overlay->layers[FORECAST_VARIANT_CLEAR] = create_forecast_clear(overlay->root);
        overlay->layers[FORECAST_VARIANT_CLOUDY] = create_forecast_cloudy(overlay->root);
        overlay->layers[FORECAST_VARIANT_FOG] = create_forecast_fog(overlay->root);
        overlay->layers[FORECAST_VARIANT_RAIN] = create_forecast_rain(overlay->root);
        overlay->layers[FORECAST_VARIANT_SNOW] = create_forecast_snow(overlay->root);
        overlay->layers[FORECAST_VARIANT_STORM] = create_forecast_storm(overlay->root);
    }

    refresh_forecast_icons();
    s_forecast_timer = lv_timer_create(
        forecast_timer_cb,
        FORECAST_REFRESH_MS,
        NULL
    );
}

static lv_obj_t *find_dashboard_layer(lv_obj_t *screen)
{
    const uint32_t screen_child_count = lv_obj_get_child_cnt(screen);

    for (uint32_t i = 0U; i < screen_child_count; i++) {
        lv_obj_t *candidate = lv_obj_get_child(screen, (int32_t)i);
        if (candidate == NULL ||
            lv_obj_get_width(candidate) != 800 ||
            lv_obj_get_height(candidate) != 340) {
            continue;
        }

        uint32_t card_count = 0U;
        const uint32_t candidate_child_count = lv_obj_get_child_cnt(candidate);
        for (uint32_t j = 0U; j < candidate_child_count; j++) {
            lv_obj_t *child = lv_obj_get_child(candidate, (int32_t)j);
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
        lv_obj_t *child = lv_obj_get_child(dashboard, (int32_t)i);
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
        attach_forecast_icons(cards[2]);
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
