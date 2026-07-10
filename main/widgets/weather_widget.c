#include "weather_widget.h"
#include "widget_common.h"
#include "theme.h"

#include <stdio.h>

#define WEATHER_YELLOW  lv_color_hex(0xFFC247)
#define WEATHER_BLUE    lv_color_hex(0x2EA8FF)
#define WEATHER_BORDER  lv_color_hex(0x665015)

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
    widget_label_create(
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
        "18.6 C",
        ST_COLOR_TEXT,
        LV_ALIGN_TOP_RIGHT,
        0,
        67
    );

    widget.description_label = widget_label_create(
        parent,
        "Zachmurzenie",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_TOP_RIGHT,
        0,
        101
    );

    create_separator(
        parent,
        205,
        143
    );

    widget.rain_value = create_metric(
        parent,
        "%",
        "10%",
        "Opad",
        0
    );

    widget.wind_value = create_metric(
        parent,
        ">",
        "12 km/h",
        "Wiatr",
        67
    );

    widget.humidity_value = create_metric(
        parent,
        "H",
        "62%",
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
        "SOB",
        "R",
        "19/11",
        0
    );

    create_forecast_column(
        &widget,
        parent,
        1,
        "NIE",
        "S",
        "21/12",
        54
    );

    create_forecast_column(
        &widget,
        parent,
        2,
        "PON",
        "S",
        "22/13",
        108
    );

    create_forecast_column(
        &widget,
        parent,
        3,
        "WTO",
        "C",
        "20/12",
        162
    );

    return widget;
}

void weather_widget_set_current(
    weather_widget_t *widget,
    float temperature_c,
    int rain_percent,
    float wind_kmh,
    int humidity_percent,
    const char *description)
{
    if (widget == NULL) {
        return;
    }

    char buffer[48];

    snprintf(
        buffer,
        sizeof(buffer),
        "%.1f C",
        temperature_c
    );

    lv_label_set_text(
        widget->temperature_label,
        buffer
    );

    snprintf(
        buffer,
        sizeof(buffer),
        "%d%%",
        rain_percent
    );

    lv_label_set_text(
        widget->rain_value,
        buffer
    );

    snprintf(
        buffer,
        sizeof(buffer),
        "%.0f km/h",
        wind_kmh
    );

    lv_label_set_text(
        widget->wind_value,
        buffer
    );

    snprintf(
        buffer,
        sizeof(buffer),
        "%d%%",
        humidity_percent
    );

    lv_label_set_text(
        widget->humidity_value,
        buffer
    );

    if (description != NULL) {
        lv_label_set_text(
            widget->description_label,
            description
        );
    }
}