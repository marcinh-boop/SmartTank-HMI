/*
 * Widżet weather_widget.c: wielokrotny komponent LVGL używany przez ekrany do spójnej prezentacji danych.
 * Implementacja ukrywa szczegóły działania; inne moduły powinny korzystać z odpowiadającego jej API.
 * Oddzielenie odpowiedzialności ułatwia testowanie, diagnostykę i późniejszą rozbudowę urządzenia.
 */
#include "weather_widget.h"
#include "widget_common.h"
#include "theme.h"

#include <stdio.h>
#include <time.h>

#define WEATHER_YELLOW  lv_color_hex(0xFFC247)
#define WEATHER_BLUE    lv_color_hex(0x2EA8FF)
#define WEATHER_BORDER  lv_color_hex(0x665015)
#define WEATHER_RED     lv_color_hex(0xFF5A5A)
#define WEATHER_CLOUD   lv_color_hex(0xDCEAF4)
#define WEATHER_DIM     lv_color_hex(0x708394)

#define WEATHER_STALE_AFTER_SECONDS 1800LL

typedef enum {
    WEATHER_ICON_CLEAR = 0,
    WEATHER_ICON_PARTLY_CLOUDY,
    WEATHER_ICON_CLOUDY,
    WEATHER_ICON_FOG,
    WEATHER_ICON_RAIN,
    WEATHER_ICON_SNOW,
    WEATHER_ICON_STORM,
} weather_icon_variant_t;

static void make_noninteractive(lv_obj_t *object)
{
    if (object == NULL) {
        return;
    }

    lv_obj_clear_flag(object, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
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

static lv_obj_t *create_icon_layer(lv_obj_t *parent)
{
    lv_obj_t *layer = lv_obj_create(parent);
    lv_obj_remove_style_all(layer);
    lv_obj_set_size(layer, 82, 82);
    lv_obj_set_pos(layer, 0, 0);
    make_noninteractive(layer);
    lv_obj_add_flag(layer, LV_OBJ_FLAG_HIDDEN);
    return layer;
}

static void create_sun_art(
    lv_obj_t *parent,
    int center_x,
    int center_y,
    int diameter)
{
    const int radius = diameter / 2;
    const int ray_offset = radius + 5;

    create_solid_shape(
        parent,
        diameter,
        diameter,
        center_x - radius,
        center_y - radius,
        WEATHER_YELLOW,
        LV_RADIUS_CIRCLE
    );

    create_solid_shape(parent, 3, 8, center_x - 1, center_y - ray_offset - 7, WEATHER_YELLOW, 1);
    create_solid_shape(parent, 3, 8, center_x - 1, center_y + ray_offset, WEATHER_YELLOW, 1);
    create_solid_shape(parent, 8, 3, center_x - ray_offset - 7, center_y - 1, WEATHER_YELLOW, 1);
    create_solid_shape(parent, 8, 3, center_x + ray_offset, center_y - 1, WEATHER_YELLOW, 1);
}

static void create_cloud_art(lv_obj_t *parent, int y)
{
    create_solid_shape(parent, 58, 18, 5, y + 23, WEATHER_CLOUD, 9);
    create_solid_shape(parent, 28, 28, 11, y + 10, WEATHER_CLOUD, LV_RADIUS_CIRCLE);
    create_solid_shape(parent, 37, 37, 28, y, WEATHER_CLOUD, LV_RADIUS_CIRCLE);
    create_solid_shape(parent, 24, 24, 51, y + 15, WEATHER_CLOUD, LV_RADIUS_CIRCLE);
}

static lv_obj_t *create_clear_icon(lv_obj_t *parent)
{
    lv_obj_t *layer = create_icon_layer(parent);
    create_sun_art(layer, 38, 38, 30);
    return layer;
}

static lv_obj_t *create_partly_cloudy_icon(lv_obj_t *parent)
{
    lv_obj_t *layer = create_icon_layer(parent);
    create_sun_art(layer, 57, 18, 22);
    create_cloud_art(layer, 27);
    return layer;
}

static lv_obj_t *create_cloudy_icon(lv_obj_t *parent)
{
    lv_obj_t *layer = create_icon_layer(parent);
    create_cloud_art(layer, 20);
    return layer;
}

static lv_obj_t *create_fog_icon(lv_obj_t *parent)
{
    lv_obj_t *layer = create_icon_layer(parent);
    create_solid_shape(layer, 58, 4, 8, 22, WEATHER_CLOUD, 2);
    create_solid_shape(layer, 68, 4, 3, 36, WEATHER_CLOUD, 2);
    create_solid_shape(layer, 54, 4, 12, 50, WEATHER_CLOUD, 2);
    create_solid_shape(layer, 63, 4, 6, 64, WEATHER_CLOUD, 2);
    return layer;
}

static lv_obj_t *create_rain_icon(lv_obj_t *parent)
{
    lv_obj_t *layer = create_icon_layer(parent);
    create_cloud_art(layer, 8);
    create_solid_shape(layer, 4, 14, 19, 57, WEATHER_BLUE, 2);
    create_solid_shape(layer, 4, 14, 37, 61, WEATHER_BLUE, 2);
    create_solid_shape(layer, 4, 14, 55, 57, WEATHER_BLUE, 2);
    return layer;
}

static lv_obj_t *create_snow_icon(lv_obj_t *parent)
{
    lv_obj_t *layer = create_icon_layer(parent);
    create_cloud_art(layer, 8);
    create_solid_shape(layer, 6, 6, 18, 59, WEATHER_BLUE, LV_RADIUS_CIRCLE);
    create_solid_shape(layer, 6, 6, 36, 65, WEATHER_BLUE, LV_RADIUS_CIRCLE);
    create_solid_shape(layer, 6, 6, 54, 59, WEATHER_BLUE, LV_RADIUS_CIRCLE);
    create_solid_shape(layer, 4, 4, 27, 72, WEATHER_BLUE, LV_RADIUS_CIRCLE);
    create_solid_shape(layer, 4, 4, 48, 72, WEATHER_BLUE, LV_RADIUS_CIRCLE);
    return layer;
}

static lv_obj_t *create_storm_icon(lv_obj_t *parent)
{
    lv_obj_t *layer = create_icon_layer(parent);
    create_cloud_art(layer, 8);
    create_solid_shape(layer, 9, 18, 36, 55, WEATHER_YELLOW, 2);
    create_solid_shape(layer, 15, 7, 31, 64, WEATHER_YELLOW, 2);
    create_solid_shape(layer, 7, 12, 31, 68, WEATHER_YELLOW, 2);
    return layer;
}

static void create_weather_icon(weather_widget_t *widget, lv_obj_t *parent)
{
    widget->icon_root = lv_obj_create(parent);
    lv_obj_remove_style_all(widget->icon_root);
    lv_obj_set_size(widget->icon_root, 82, 82);
    lv_obj_align(widget->icon_root, LV_ALIGN_TOP_LEFT, -2, 32);
    make_noninteractive(widget->icon_root);

    widget->icon_clear = create_clear_icon(widget->icon_root);
    widget->icon_partly_cloudy = create_partly_cloudy_icon(widget->icon_root);
    widget->icon_cloudy = create_cloudy_icon(widget->icon_root);
    widget->icon_fog = create_fog_icon(widget->icon_root);
    widget->icon_rain = create_rain_icon(widget->icon_root);
    widget->icon_snow = create_snow_icon(widget->icon_root);
    widget->icon_storm = create_storm_icon(widget->icon_root);
}

static lv_obj_t *icon_for_code(weather_widget_t *widget, int code)
{
    if (code == 0) {
        return widget->icon_clear;
    }
    if (code == 1 || code == 2) {
        return widget->icon_partly_cloudy;
    }
    if (code == 3) {
        return widget->icon_cloudy;
    }
    if (code == 45 || code == 48) {
        return widget->icon_fog;
    }
    if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) {
        return widget->icon_rain;
    }
    if ((code >= 71 && code <= 77) || (code >= 85 && code <= 86)) {
        return widget->icon_snow;
    }
    if (code >= 95 && code <= 99) {
        return widget->icon_storm;
    }
    return widget->icon_cloudy;
}

static void set_weather_icon(
    weather_widget_t *widget,
    bool visible,
    int weather_code)
{
    lv_obj_t *layers[] = {
        widget->icon_clear,
        widget->icon_partly_cloudy,
        widget->icon_cloudy,
        widget->icon_fog,
        widget->icon_rain,
        widget->icon_snow,
        widget->icon_storm,
    };

    for (size_t index = 0U; index < sizeof(layers) / sizeof(layers[0]); index++) {
        if (layers[index] != NULL) {
            lv_obj_add_flag(layers[index], LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (!visible) {
        return;
    }

    lv_obj_t *selected = icon_for_code(widget, weather_code);
    if (selected != NULL) {
        lv_obj_clear_flag(selected, LV_OBJ_FLAG_HIDDEN);
    }
}

static void create_separator(
    lv_obj_t *parent,
    int width,
    int y)
{
    lv_obj_t *line = lv_obj_create(parent);

    lv_obj_set_size(line, width, 1);
    lv_obj_align(line, LV_ALIGN_TOP_MID, 0, y);
    make_noninteractive(line);

    lv_obj_set_style_bg_color(
        line,
        WEATHER_BORDER,
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        line,
        LV_OPA_COVER,
        LV_PART_MAIN
    );

    lv_obj_set_style_border_width(
        line,
        0,
        LV_PART_MAIN
    );

    lv_obj_set_style_radius(
        line,
        0,
        LV_PART_MAIN
    );
}

static lv_obj_t *create_metric(
    lv_obj_t *parent,
    const char *symbol,
    const char *value,
    const char *name,
    int x)
{
    widget_label_create(
        parent,
        symbol,
        WEATHER_BLUE,
        LV_ALIGN_TOP_LEFT,
        x,
        164
    );

    lv_obj_t *value_label = widget_label_create(
        parent,
        value,
        ST_COLOR_TEXT,
        LV_ALIGN_TOP_LEFT,
        x + 20,
        162
    );

    lv_obj_t *name_label = widget_label_create(
        parent,
        name,
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_TOP_LEFT,
        x + 20,
        181
    );
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_12, LV_PART_MAIN);

    return value_label;
}

static void create_forecast_column(
    weather_widget_t *widget,
    lv_obj_t *parent,
    int index,
    const char *day,
    const char *icon,
    const char *temperature,
    int x)
{
    widget->forecast_day[index] = widget_label_create(
        parent,
        day,
        ST_COLOR_TEXT,
        LV_ALIGN_TOP_LEFT,
        x,
        220
    );

    widget->forecast_icon[index] = widget_label_create(
        parent,
        icon,
        WEATHER_YELLOW,
        LV_ALIGN_TOP_LEFT,
        x - 1,
        247
    );

    widget->forecast_temp[index] = widget_label_create(
        parent,
        temperature,
        ST_COLOR_TEXT,
        LV_ALIGN_TOP_LEFT,
        x - 5,
        283
    );
}

static const char *forecast_icon_for_code(int code)
{
    if (code == 0) {
        return "S";
    }
    if (code >= 1 && code <= 3) {
        return "C";
    }
    if (code == 45 || code == 48) {
        return "M";
    }
    if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) {
        return "R";
    }
    if ((code >= 71 && code <= 77) || (code >= 85 && code <= 86)) {
        return "N";
    }
    if (code >= 95 && code <= 99) {
        return "B";
    }
    return "?";
}

static lv_color_t forecast_color_for_code(int code)
{
    if (code == 0) {
        return WEATHER_YELLOW;
    }
    if (code >= 95 && code <= 99) {
        return WEATHER_RED;
    }
    if ((code >= 51 && code <= 86) || code == 45 || code == 48) {
        return WEATHER_BLUE;
    }
    return WEATHER_CLOUD;
}

static bool measurement_is_stale(const weather_measurement_t *measurement)
{
    if (measurement == NULL || !measurement->valid) {
        return false;
    }

    if (measurement->stale) {
        return true;
    }

    const time_t now = time(NULL);
    if (measurement->updated_epoch <= 0 || now <= 0) {
        return false;
    }

    const int64_t age_seconds = (int64_t)now - measurement->updated_epoch;
    return age_seconds > WEATHER_STALE_AFTER_SECONDS;
}

static void update_time_status(
    weather_widget_t *widget,
    const weather_measurement_t *measurement,
    bool stale)
{
    if (widget == NULL || widget->update_status_label == NULL) {
        return;
    }

    if (measurement == NULL || !measurement->valid || measurement->updated_epoch <= 0) {
        lv_label_set_text(widget->update_status_label, "OCZEKIWANIE");
        lv_obj_set_style_text_color(
            widget->update_status_label,
            ST_COLOR_TEXT_DIM,
            LV_PART_MAIN
        );
        return;
    }

    const time_t epoch = (time_t)measurement->updated_epoch;
    struct tm local_time = {0};
    if (localtime_r(&epoch, &local_time) == NULL) {
        lv_label_set_text(
            widget->update_status_label,
            stale ? "NIEAKTUALNE" : "AKTUALNE"
        );
    } else {
        char buffer[32];
        snprintf(
            buffer,
            sizeof(buffer),
            stale ? "NIEAKT. %02d:%02d" : "AKT. %02d:%02d",
            local_time.tm_hour,
            local_time.tm_min
        );
        lv_label_set_text(widget->update_status_label, buffer);
    }

    lv_obj_set_style_text_color(
        widget->update_status_label,
        stale ? WEATHER_YELLOW : WEATHER_DIM,
        LV_PART_MAIN
    );
}

weather_widget_t weather_widget_create(lv_obj_t *parent)
{
    weather_widget_t widget = {0};
    widget.root = parent;

    lv_obj_clear_flag(
        parent,
        LV_OBJ_FLAG_SCROLLABLE
    );

    widget_label_create(
        parent,
        "POGODA",
        WEATHER_YELLOW,
        LV_ALIGN_TOP_LEFT,
        0,
        0
    );

    create_weather_icon(&widget, parent);

    widget.temperature_label = widget_label_create(
        parent,
        "--.- C",
        ST_COLOR_TEXT,
        LV_ALIGN_TOP_RIGHT,
        0,
        67
    );

    widget.description_label = widget_label_create(
        parent,
        "Brak danych",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_TOP_RIGHT,
        0,
        101
    );
    lv_obj_set_width(widget.description_label, 120);
    lv_obj_set_style_text_align(
        widget.description_label,
        LV_TEXT_ALIGN_RIGHT,
        LV_PART_MAIN
    );
    lv_label_set_long_mode(widget.description_label, LV_LABEL_LONG_DOT);

    widget.update_status_label = widget_label_create(
        parent,
        "OCZEKIWANIE",
        WEATHER_DIM,
        LV_ALIGN_TOP_RIGHT,
        0,
        124
    );
    lv_obj_set_width(widget.update_status_label, 130);
    lv_obj_set_style_text_font(
        widget.update_status_label,
        &lv_font_montserrat_12,
        LV_PART_MAIN
    );
    lv_obj_set_style_text_align(
        widget.update_status_label,
        LV_TEXT_ALIGN_RIGHT,
        LV_PART_MAIN
    );

    create_separator(
        parent,
        205,
        143
    );

    widget.rain_value = create_metric(
        parent,
        "%",
        "--",
        "Szansa",
        0
    );

    widget.wind_value = create_metric(
        parent,
        ">",
        "--",
        "Wiatr",
        67
    );

    widget.humidity_value = create_metric(
        parent,
        "H",
        "--",
        "Wilg.",
        145
    );

    create_separator(
        parent,
        205,
        207
    );

    create_forecast_column(
        &widget,
        parent,
        0,
        "---",
        "-",
        "--/--",
        0
    );

    create_forecast_column(
        &widget,
        parent,
        1,
        "---",
        "-",
        "--/--",
        54
    );

    create_forecast_column(
        &widget,
        parent,
        2,
        "---",
        "-",
        "--/--",
        108
    );

    create_forecast_column(
        &widget,
        parent,
        3,
        "---",
        "-",
        "--/--",
        162
    );

    return widget;
}

void weather_widget_set_data(
    weather_widget_t *widget,
    const weather_measurement_t *measurement)
{
    if (widget == NULL || measurement == NULL) {
        return;
    }

    char buffer[64];

    if (!measurement->valid) {
        set_weather_icon(widget, false, 0);
        lv_label_set_text(widget->temperature_label, "--.- C");
        lv_label_set_text(widget->description_label, "Brak danych");
        lv_obj_set_style_text_color(
            widget->description_label,
            ST_COLOR_TEXT_DIM,
            LV_PART_MAIN
        );
        update_time_status(widget, measurement, false);
        lv_label_set_text(widget->rain_value, "--");
        lv_label_set_text(widget->wind_value, "--");
        lv_label_set_text(widget->humidity_value, "--");

        for (uint8_t index = 0U; index < WEATHER_FORECAST_DAYS; index++) {
            lv_label_set_text(widget->forecast_day[index], "---");
            lv_label_set_text(widget->forecast_icon[index], "-");
            lv_label_set_text(widget->forecast_temp[index], "--/--");
            lv_obj_set_style_text_color(
                widget->forecast_icon[index],
                ST_COLOR_TEXT_DIM,
                LV_PART_MAIN
            );
        }
        return;
    }

    const bool stale = measurement_is_stale(measurement);
    set_weather_icon(widget, true, measurement->weather_code);

    snprintf(
        buffer,
        sizeof(buffer),
        "%.1f C",
        measurement->temperature_c
    );
    lv_label_set_text(widget->temperature_label, buffer);

    snprintf(
        buffer,
        sizeof(buffer),
        "%d%%",
        measurement->rain_percent
    );
    lv_label_set_text(widget->rain_value, buffer);

    snprintf(
        buffer,
        sizeof(buffer),
        "%.0f km/h",
        measurement->wind_kmh
    );
    lv_label_set_text(widget->wind_value, buffer);

    snprintf(
        buffer,
        sizeof(buffer),
        "%d%%",
        measurement->humidity_percent
    );
    lv_label_set_text(widget->humidity_value, buffer);

    lv_label_set_text(widget->description_label, measurement->description);
    lv_obj_set_style_text_color(
        widget->description_label,
        stale ? WEATHER_YELLOW : ST_COLOR_TEXT_DIM,
        LV_PART_MAIN
    );
    update_time_status(widget, measurement, stale);

    for (uint8_t index = 0U; index < WEATHER_FORECAST_DAYS; index++) {
        if (index >= measurement->forecast_count ||
            !measurement->forecast[index].valid) {
            lv_label_set_text(widget->forecast_day[index], "---");
            lv_label_set_text(widget->forecast_icon[index], "-");
            lv_label_set_text(widget->forecast_temp[index], "--/--");
            lv_obj_set_style_text_color(
                widget->forecast_icon[index],
                ST_COLOR_TEXT_DIM,
                LV_PART_MAIN
            );
            continue;
        }

        const weather_forecast_day_t *forecast = &measurement->forecast[index];
        lv_label_set_text(widget->forecast_day[index], forecast->day);
        lv_label_set_text(
            widget->forecast_icon[index],
            forecast_icon_for_code(forecast->weather_code)
        );
        lv_obj_set_style_text_color(
            widget->forecast_icon[index],
            forecast_color_for_code(forecast->weather_code),
            LV_PART_MAIN
        );

        snprintf(
            buffer,
            sizeof(buffer),
            "%.0f/%.0f",
            forecast->temperature_max_c,
            forecast->temperature_min_c
        );
        lv_label_set_text(widget->forecast_temp[index], buffer);
    }
}
