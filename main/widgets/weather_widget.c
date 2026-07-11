#include "weather_widget.h"
#include "widget_common.h"
#include "theme.h"

#include <stdio.h>

#define WEATHER_YELLOW  lv_color_hex(0xFFC247)
#define WEATHER_BLUE    lv_color_hex(0x2EA8FF)
#define WEATHER_BORDER  lv_color_hex(0x665015)
#define WEATHER_RED     lv_color_hex(0xFF5A5A)
#define WEATHER_CLOUD   lv_color_hex(0xDCEAF4)

static void create_separator(
    lv_obj_t *parent,
    int width,
    int y)
{
    lv_obj_t *line = lv_obj_create(parent);

    lv_obj_set_size(line, width, 1);
    lv_obj_align(line, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);

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

    widget_label_create(
        parent,
        name,
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_TOP_LEFT,
        x + 20,
        181
    );

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

    widget_label_create(
        parent,
        "SUN",
        WEATHER_YELLOW,
        LV_ALIGN_TOP_LEFT,
        7,
        63
    );

    widget_label_create(
        parent,
        "CLOUD",
        ST_COLOR_TEXT,
        LV_ALIGN_TOP_LEFT,
        0,
        87
    );

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

    create_separator(
        parent,
        205,
        143
    );

    widget.rain_value = create_metric(
        parent,
        "%",
        "--",
        "Opad",
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
        lv_label_set_text(widget->temperature_label, "--.- C");
        lv_label_set_text(widget->description_label, "Ustaw lokalizacje");
        lv_obj_set_style_text_color(
            widget->description_label,
            ST_COLOR_TEXT_DIM,
            LV_PART_MAIN
        );
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

    snprintf(
        buffer,
        sizeof(buffer),
        "%s%s",
        measurement->description,
        measurement->stale ? " *" : ""
    );
    lv_label_set_text(widget->description_label, buffer);
    lv_obj_set_style_text_color(
        widget->description_label,
        measurement->stale ? WEATHER_YELLOW : ST_COLOR_TEXT_DIM,
        LV_PART_MAIN
    );

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
