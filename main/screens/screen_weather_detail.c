#include "screen_weather_detail.h"

#include <stdio.h>
#include <time.h>

#include "theme.h"
#include "weather_location_storage.h"

#define WEATHER_BLUE       lv_color_hex(0x2EA8FF)
#define WEATHER_YELLOW     lv_color_hex(0xFFC247)
#define WEATHER_GREEN      lv_color_hex(0x39D12F)
#define WEATHER_RED        lv_color_hex(0xFF4D4D)
#define WEATHER_CLOUD      lv_color_hex(0xDCEAF4)
#define WEATHER_PANEL      lv_color_hex(0x0B1825)
#define WEATHER_BORDER     lv_color_hex(0x24384A)
#define WEATHER_ROW_BORDER lv_color_hex(0x183046)
#define WEATHER_DIM        lv_color_hex(0x708394)
#define WEATHER_EPOCH_MIN  1704067200LL

#define FORECAST_ROW_COUNT WEATHER_FORECAST_DAYS

typedef enum {
    WEATHER_VARIANT_CLEAR = 0,
    WEATHER_VARIANT_PARTLY_CLOUDY,
    WEATHER_VARIANT_CLOUDY,
    WEATHER_VARIANT_FOG,
    WEATHER_VARIANT_RAIN,
    WEATHER_VARIANT_SNOW,
    WEATHER_VARIANT_STORM,
} weather_variant_t;

typedef struct {
    lv_obj_t *day;
    lv_obj_t *icon;
    lv_obj_t *temperature;
    lv_obj_t *rain;
    int last_code;
    bool last_valid;
} forecast_row_t;

static lv_obj_t *s_root;
static lv_obj_t *s_location_label;
static lv_obj_t *s_current_icon;
static lv_obj_t *s_temperature_label;
static lv_obj_t *s_description_label;
static lv_obj_t *s_update_label;
static lv_obj_t *s_data_status_label;
static lv_obj_t *s_rain_value;
static lv_obj_t *s_wind_value;
static lv_obj_t *s_humidity_value;
static lv_obj_t *s_code_value;
static lv_obj_t *s_sample_value;
static forecast_row_t s_forecast[FORECAST_ROW_COUNT];
static screen_weather_detail_back_cb_t s_back_cb;
static int s_last_current_code = -1;
static bool s_last_current_valid;

static void make_noninteractive(lv_obj_t *object)
{
    if (object == NULL) {
        return;
    }

    lv_obj_clear_flag(object, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t *create_label(
    lv_obj_t *parent,
    const char *text,
    lv_color_t color,
    const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    return label;
}

static lv_obj_t *create_panel(lv_obj_t *parent, int x, int width)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, width, 285);
    lv_obj_align(panel, LV_ALIGN_TOP_LEFT, x, 45);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(panel, WEATHER_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, WEATHER_BORDER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 12, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(panel, 0, LV_PART_MAIN);
    return panel;
}

static lv_obj_t *create_button(
    lv_obj_t *parent,
    const char *text,
    int width,
    int height)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_size(button, width, height);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x3A3218), LV_PART_MAIN);
    lv_obj_set_style_bg_color(
        button,
        lv_color_hex(0x514522),
        LV_PART_MAIN | LV_STATE_PRESSED
    );
    lv_obj_set_style_radius(button, 7, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(button, WEATHER_YELLOW, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);

    lv_obj_t *label = create_label(
        button,
        text,
        ST_COLOR_TEXT,
        &lv_font_montserrat_12
    );
    lv_obj_center(label);
    make_noninteractive(label);
    return button;
}

static lv_obj_t *create_shape(
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

static weather_variant_t variant_for_code(int code)
{
    if (code == 0) {
        return WEATHER_VARIANT_CLEAR;
    }
    if (code == 1 || code == 2) {
        return WEATHER_VARIANT_PARTLY_CLOUDY;
    }
    if (code == 3) {
        return WEATHER_VARIANT_CLOUDY;
    }
    if (code == 45 || code == 48) {
        return WEATHER_VARIANT_FOG;
    }
    if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) {
        return WEATHER_VARIANT_RAIN;
    }
    if ((code >= 71 && code <= 77) || (code >= 85 && code <= 86)) {
        return WEATHER_VARIANT_SNOW;
    }
    if (code >= 95 && code <= 99) {
        return WEATHER_VARIANT_STORM;
    }
    return WEATHER_VARIANT_CLOUDY;
}

static void draw_sun(lv_obj_t *root, int scale, int x, int y)
{
    create_shape(root, 13 * scale, 13 * scale, x + 10 * scale, y + 9 * scale,
                 WEATHER_YELLOW, LV_RADIUS_CIRCLE);
    create_shape(root, 2 * scale, 5 * scale, x + 15 * scale, y + 2 * scale,
                 WEATHER_YELLOW, 1);
    create_shape(root, 2 * scale, 5 * scale, x + 15 * scale, y + 24 * scale,
                 WEATHER_YELLOW, 1);
    create_shape(root, 5 * scale, 2 * scale, x + 3 * scale, y + 14 * scale,
                 WEATHER_YELLOW, 1);
    create_shape(root, 5 * scale, 2 * scale, x + 25 * scale, y + 14 * scale,
                 WEATHER_YELLOW, 1);
}

static void draw_cloud(lv_obj_t *root, int scale, int x, int y)
{
    create_shape(root, 25 * scale, 8 * scale, x + 4 * scale, y + 16 * scale,
                 WEATHER_CLOUD, 4 * scale);
    create_shape(root, 11 * scale, 11 * scale, x + 7 * scale, y + 11 * scale,
                 WEATHER_CLOUD, LV_RADIUS_CIRCLE);
    create_shape(root, 15 * scale, 15 * scale, x + 14 * scale, y + 8 * scale,
                 WEATHER_CLOUD, LV_RADIUS_CIRCLE);
}

static void draw_weather_icon(lv_obj_t *root, bool valid, int code, int scale)
{
    if (root == NULL) {
        return;
    }

    lv_obj_clean(root);
    if (!valid) {
        lv_obj_t *label = create_label(root, "--", WEATHER_DIM, &lv_font_montserrat_20);
        lv_obj_center(label);
        make_noninteractive(label);
        return;
    }

    switch (variant_for_code(code)) {
        case WEATHER_VARIANT_CLEAR:
            draw_sun(root, scale, 0, 0);
            break;

        case WEATHER_VARIANT_PARTLY_CLOUDY:
            create_shape(root, 9 * scale, 9 * scale, 21 * scale, 3 * scale,
                         WEATHER_YELLOW, LV_RADIUS_CIRCLE);
            draw_cloud(root, scale, 0, 0);
            break;

        case WEATHER_VARIANT_CLOUDY:
            draw_cloud(root, scale, 0, 0);
            break;

        case WEATHER_VARIANT_FOG:
            create_shape(root, 23 * scale, 3 * scale, 5 * scale, 7 * scale,
                         WEATHER_CLOUD, 1);
            create_shape(root, 29 * scale, 3 * scale, 2 * scale, 14 * scale,
                         WEATHER_CLOUD, 1);
            create_shape(root, 21 * scale, 3 * scale, 7 * scale, 21 * scale,
                         WEATHER_CLOUD, 1);
            break;

        case WEATHER_VARIANT_RAIN:
            draw_cloud(root, scale, 0, -5 * scale);
            create_shape(root, 3 * scale, 7 * scale, 8 * scale, 24 * scale,
                         WEATHER_BLUE, 1);
            create_shape(root, 3 * scale, 7 * scale, 16 * scale, 26 * scale,
                         WEATHER_BLUE, 1);
            create_shape(root, 3 * scale, 7 * scale, 24 * scale, 24 * scale,
                         WEATHER_BLUE, 1);
            break;

        case WEATHER_VARIANT_SNOW:
            draw_cloud(root, scale, 0, -5 * scale);
            create_shape(root, 4 * scale, 4 * scale, 7 * scale, 25 * scale,
                         WEATHER_BLUE, LV_RADIUS_CIRCLE);
            create_shape(root, 4 * scale, 4 * scale, 15 * scale, 28 * scale,
                         WEATHER_BLUE, LV_RADIUS_CIRCLE);
            create_shape(root, 4 * scale, 4 * scale, 23 * scale, 25 * scale,
                         WEATHER_BLUE, LV_RADIUS_CIRCLE);
            break;

        case WEATHER_VARIANT_STORM:
            draw_cloud(root, scale, 0, -5 * scale);
            create_shape(root, 6 * scale, 10 * scale, 15 * scale, 21 * scale,
                         WEATHER_YELLOW, 1);
            create_shape(root, 10 * scale, 4 * scale, 12 * scale, 25 * scale,
                         WEATHER_YELLOW, 1);
            create_shape(root, 4 * scale, 7 * scale, 12 * scale, 27 * scale,
                         WEATHER_YELLOW, 1);
            break;
    }
}

static lv_obj_t *create_icon_root(lv_obj_t *parent, int size)
{
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, size, size);
    make_noninteractive(root);
    return root;
}

static lv_obj_t *create_value_row(
    lv_obj_t *parent,
    const char *name,
    const char *value,
    int y)
{
    lv_obj_t *name_label = create_label(
        parent,
        name,
        ST_COLOR_TEXT_DIM,
        &lv_font_montserrat_12
    );
    lv_obj_align(name_label, LV_ALIGN_TOP_LEFT, 0, y);

    lv_obj_t *value_label = create_label(
        parent,
        value,
        ST_COLOR_TEXT,
        &lv_font_montserrat_14
    );
    lv_obj_align(value_label, LV_ALIGN_TOP_RIGHT, 0, y - 1);
    lv_obj_set_width(value_label, 105);
    lv_obj_set_style_text_align(value_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_label_set_long_mode(value_label, LV_LABEL_LONG_DOT);

    lv_obj_t *line = create_shape(
        parent,
        lv_pct(100),
        1,
        0,
        y + 24,
        WEATHER_ROW_BORDER,
        0
    );
    lv_obj_align(line, LV_ALIGN_TOP_MID, 0, y + 24);
    return value_label;
}

static void back_button_event_cb(lv_event_t *event)
{
    (void)event;
    if (s_back_cb != NULL) {
        s_back_cb();
    }
}

static void refresh_location(void)
{
    weather_location_t location;
    const esp_err_t result = weather_location_storage_load(&location);

    if (result == ESP_OK) {
        char buffer[176];
        if (location.admin1[0] != '\0') {
            snprintf(
                buffer,
                sizeof(buffer),
                "%s, %s\n%s",
                location.name,
                location.admin1,
                location.country
            );
        } else if (location.country[0] != '\0') {
            snprintf(buffer, sizeof(buffer), "%s\n%s", location.name, location.country);
        } else {
            snprintf(buffer, sizeof(buffer), "%s", location.name);
        }
        lv_label_set_text(s_location_label, buffer);
        lv_obj_set_style_text_color(s_location_label, ST_COLOR_TEXT, LV_PART_MAIN);
    } else {
        lv_label_set_text(s_location_label, "Brak zapisanej miejscowosci");
        lv_obj_set_style_text_color(s_location_label, WEATHER_YELLOW, LV_PART_MAIN);
    }
}

static void format_update_time(
    const weather_measurement_t *weather,
    char *buffer,
    size_t buffer_size)
{
    if (weather == NULL || weather->updated_epoch < WEATHER_EPOCH_MIN) {
        snprintf(buffer, buffer_size, "Aktualizacja: --");
        return;
    }

    const time_t epoch = (time_t)weather->updated_epoch;
    struct tm local_time = {0};
    if (localtime_r(&epoch, &local_time) == NULL) {
        snprintf(buffer, buffer_size, "Aktualizacja: --");
        return;
    }

    snprintf(
        buffer,
        buffer_size,
        "Aktualizacja: %02d.%02d %02d:%02d",
        local_time.tm_mday,
        local_time.tm_mon + 1,
        local_time.tm_hour,
        local_time.tm_min
    );
}

static void create_forecast_row(lv_obj_t *parent, uint8_t index, int y)
{
    s_forecast[index].day = create_label(
        parent,
        "---",
        ST_COLOR_TEXT,
        &lv_font_montserrat_14
    );
    lv_obj_align(s_forecast[index].day, LV_ALIGN_TOP_LEFT, 0, y + 9);
    lv_obj_set_width(s_forecast[index].day, 42);

    s_forecast[index].icon = create_icon_root(parent, 34);
    lv_obj_align(s_forecast[index].icon, LV_ALIGN_TOP_LEFT, 45, y + 1);
    draw_weather_icon(s_forecast[index].icon, false, 0, 1);

    s_forecast[index].temperature = create_label(
        parent,
        "--/-- C",
        ST_COLOR_TEXT,
        &lv_font_montserrat_14
    );
    lv_obj_align(s_forecast[index].temperature, LV_ALIGN_TOP_LEFT, 88, y + 8);
    lv_obj_set_width(s_forecast[index].temperature, 88);

    s_forecast[index].rain = create_label(
        parent,
        "--%",
        WEATHER_BLUE,
        &lv_font_montserrat_12
    );
    lv_obj_align(s_forecast[index].rain, LV_ALIGN_TOP_RIGHT, 0, y + 10);
    lv_obj_set_width(s_forecast[index].rain, 55);
    lv_obj_set_style_text_align(s_forecast[index].rain, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

    if (index + 1U < FORECAST_ROW_COUNT) {
        lv_obj_t *line = create_shape(
            parent,
            lv_pct(100),
            1,
            0,
            y + 45,
            WEATHER_ROW_BORDER,
            0
        );
        lv_obj_align(line, LV_ALIGN_TOP_MID, 0, y + 45);
    }

    s_forecast[index].last_code = -1;
    s_forecast[index].last_valid = false;
}

lv_obj_t *screen_weather_detail_create(
    lv_obj_t *parent,
    screen_weather_detail_back_cb_t back_cb)
{
    s_back_cb = back_cb;

    s_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, 800, 340);
    lv_obj_align(s_root, LV_ALIGN_TOP_MID, 0, 58);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *back_button = create_button(s_root, "< WSTECZ", 105, 34);
    lv_obj_align(back_button, LV_ALIGN_TOP_LEFT, 20, 5);
    lv_obj_add_event_cb(back_button, back_button_event_cb, LV_EVENT_RELEASED, NULL);

    lv_obj_t *subtitle = create_label(
        s_root,
        "Open-Meteo / warunki aktualne / prognoza 4 dni",
        ST_COLOR_TEXT_DIM,
        &lv_font_montserrat_12
    );
    lv_obj_align(subtitle, LV_ALIGN_TOP_RIGHT, -20, 14);

    lv_obj_t *current_panel = create_panel(s_root, 20, 250);
    lv_obj_t *current_title = create_label(
        current_panel,
        "AKTUALNA POGODA",
        WEATHER_YELLOW,
        &lv_font_montserrat_14
    );
    lv_obj_align(current_title, LV_ALIGN_TOP_LEFT, 0, 0);

    s_location_label = create_label(
        current_panel,
        "Brak zapisanej miejscowosci",
        ST_COLOR_TEXT_DIM,
        &lv_font_montserrat_12
    );
    lv_obj_align(s_location_label, LV_ALIGN_TOP_LEFT, 0, 27);
    lv_obj_set_width(s_location_label, 220);
    lv_label_set_long_mode(s_location_label, LV_LABEL_LONG_WRAP);

    s_current_icon = create_icon_root(current_panel, 68);
    lv_obj_align(s_current_icon, LV_ALIGN_TOP_LEFT, 0, 75);
    draw_weather_icon(s_current_icon, false, 0, 2);

    s_temperature_label = create_label(
        current_panel,
        "--.- C",
        ST_COLOR_TEXT,
        &lv_font_montserrat_20
    );
    lv_obj_align(s_temperature_label, LV_ALIGN_TOP_RIGHT, 0, 82);

    s_description_label = create_label(
        current_panel,
        "Brak danych",
        ST_COLOR_TEXT_DIM,
        &lv_font_montserrat_14
    );
    lv_obj_align(s_description_label, LV_ALIGN_TOP_RIGHT, 0, 116);
    lv_obj_set_width(s_description_label, 140);
    lv_obj_set_style_text_align(s_description_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_label_set_long_mode(s_description_label, LV_LABEL_LONG_WRAP);

    s_update_label = create_label(
        current_panel,
        "Aktualizacja: --",
        ST_COLOR_TEXT_DIM,
        &lv_font_montserrat_12
    );
    lv_obj_align(s_update_label, LV_ALIGN_BOTTOM_LEFT, 0, -28);

    s_data_status_label = create_label(
        current_panel,
        "OCZEKIWANIE NA DANE",
        WEATHER_YELLOW,
        &lv_font_montserrat_12
    );
    lv_obj_align(s_data_status_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t *metrics_panel = create_panel(s_root, 282, 210);
    lv_obj_t *metrics_title = create_label(
        metrics_panel,
        "PARAMETRY",
        WEATHER_BLUE,
        &lv_font_montserrat_14
    );
    lv_obj_align(metrics_title, LV_ALIGN_TOP_LEFT, 0, 0);

    s_rain_value = create_value_row(metrics_panel, "Szansa opadu", "--%", 34);
    s_wind_value = create_value_row(metrics_panel, "Wiatr", "-- km/h", 77);
    s_humidity_value = create_value_row(metrics_panel, "Wilgotnosc", "--%", 120);
    s_code_value = create_value_row(metrics_panel, "Kod pogody", "--", 163);
    s_sample_value = create_value_row(metrics_panel, "Probka", "--", 206);

    lv_obj_t *forecast_panel = create_panel(s_root, 504, 276);
    lv_obj_t *forecast_title = create_label(
        forecast_panel,
        "PROGNOZA",
        WEATHER_GREEN,
        &lv_font_montserrat_14
    );
    lv_obj_align(forecast_title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *rain_header = create_label(
        forecast_panel,
        LV_SYMBOL_TINT " OPAD",
        WEATHER_BLUE,
        &lv_font_montserrat_12
    );
    lv_obj_align(rain_header, LV_ALIGN_TOP_RIGHT, 0, 2);

    for (uint8_t index = 0U; index < FORECAST_ROW_COUNT; index++) {
        create_forecast_row(forecast_panel, index, 28 + ((int)index * 55));
    }

    lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    return s_root;
}

void screen_weather_detail_begin(const smarttank_state_t *state)
{
    if (s_root == NULL) {
        return;
    }

    refresh_location();
    screen_weather_detail_update(state);
}

void screen_weather_detail_update(const smarttank_state_t *state)
{
    if (state == NULL || s_root == NULL) {
        return;
    }

    const weather_measurement_t *weather = &state->weather;
    char buffer[64];

    if (s_last_current_code != weather->weather_code ||
        s_last_current_valid != weather->valid) {
        draw_weather_icon(s_current_icon, weather->valid, weather->weather_code, 2);
        s_last_current_code = weather->weather_code;
        s_last_current_valid = weather->valid;
    }

    if (!weather->valid) {
        lv_label_set_text(s_temperature_label, "--.- C");
        lv_label_set_text(s_description_label, "Brak danych");
        lv_label_set_text(s_update_label, "Aktualizacja: --");
        lv_label_set_text(s_data_status_label, "OCZEKIWANIE NA DANE");
        lv_obj_set_style_text_color(s_data_status_label, WEATHER_YELLOW, LV_PART_MAIN);
        lv_label_set_text(s_rain_value, "--%");
        lv_label_set_text(s_wind_value, "-- km/h");
        lv_label_set_text(s_humidity_value, "--%");
        lv_label_set_text(s_code_value, "--");
        lv_label_set_text(s_sample_value, "--");
    } else {
        snprintf(buffer, sizeof(buffer), "%.1f C", weather->temperature_c);
        lv_label_set_text(s_temperature_label, buffer);
        lv_label_set_text(s_description_label, weather->description);

        format_update_time(weather, buffer, sizeof(buffer));
        lv_label_set_text(s_update_label, buffer);

        lv_label_set_text(
            s_data_status_label,
            weather->stale ? "DANE NIEAKTUALNE" : "DANE AKTUALNE"
        );
        lv_obj_set_style_text_color(
            s_data_status_label,
            weather->stale ? WEATHER_YELLOW : WEATHER_GREEN,
            LV_PART_MAIN
        );

        snprintf(buffer, sizeof(buffer), "%d%%", weather->rain_percent);
        lv_label_set_text(s_rain_value, buffer);

        snprintf(buffer, sizeof(buffer), "%.0f km/h", weather->wind_kmh);
        lv_label_set_text(s_wind_value, buffer);

        snprintf(buffer, sizeof(buffer), "%d%%", weather->humidity_percent);
        lv_label_set_text(s_humidity_value, buffer);

        snprintf(buffer, sizeof(buffer), "%d", weather->weather_code);
        lv_label_set_text(s_code_value, buffer);

        snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)weather->sample_counter);
        lv_label_set_text(s_sample_value, buffer);
    }

    for (uint8_t index = 0U; index < FORECAST_ROW_COUNT; index++) {
        const bool valid = index < weather->forecast_count &&
                           weather->forecast[index].valid;
        const weather_forecast_day_t *forecast = &weather->forecast[index];

        if (s_forecast[index].last_code != forecast->weather_code ||
            s_forecast[index].last_valid != valid) {
            draw_weather_icon(
                s_forecast[index].icon,
                valid,
                forecast->weather_code,
                1
            );
            s_forecast[index].last_code = forecast->weather_code;
            s_forecast[index].last_valid = valid;
        }

        if (!valid) {
            lv_label_set_text(s_forecast[index].day, "---");
            lv_label_set_text(s_forecast[index].temperature, "--/-- C");
            lv_label_set_text(s_forecast[index].rain, "--%");
            continue;
        }

        lv_label_set_text(s_forecast[index].day, forecast->day);
        snprintf(
            buffer,
            sizeof(buffer),
            "%.0f/%.0f C",
            forecast->temperature_max_c,
            forecast->temperature_min_c
        );
        lv_label_set_text(s_forecast[index].temperature, buffer);

        snprintf(buffer, sizeof(buffer), "%d%%", forecast->rain_percent);
        lv_label_set_text(s_forecast[index].rain, buffer);
    }
}
