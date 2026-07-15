/*
 * Szczegółowy ekran studni.
 * Pokazuje słup wody, głębokość, procent napełnienia, odległość od lustra,
 * prąd AI2 i diagnostykę źródła. Interpretacja wyniku korzysta wyłącznie
 * z ustawień studni, dzięki czemu ekran nie jest związany z jedną instalacją.
 */
#include "screen_well_detail.h"

#include <stdio.h>

#include "theme.h"
#include "well_settings.h"

#define WELL_BLUE        lv_color_hex(0x2EA8FF)
#define WELL_GREEN       lv_color_hex(0x39D12F)
#define WELL_YELLOW      lv_color_hex(0xFFC247)
#define WELL_RED         lv_color_hex(0xFF3333)
#define WELL_PANEL       lv_color_hex(0x0B1825)
#define WELL_BORDER      lv_color_hex(0x24384A)
#define WELL_ROW_BORDER  lv_color_hex(0x183046)

static lv_obj_t *s_root;
static lv_obj_t *s_arc;
static lv_obj_t *s_percent_label;
static lv_obj_t *s_column_label;
static lv_obj_t *s_status_label;
static lv_obj_t *s_water_column_value;
static lv_obj_t *s_depth_value;
static lv_obj_t *s_free_space_value;
static lv_obj_t *s_level_value;
static lv_obj_t *s_sample_value;
static lv_obj_t *s_source_value;
static lv_obj_t *s_sensor_value;
static lv_obj_t *s_input_value;
static lv_obj_t *s_channel_value;
static lv_obj_t *s_range_value;
static lv_obj_t *s_warning_value;
static lv_obj_t *s_critical_value;
static screen_well_detail_back_cb_t s_back_cb;
static screen_well_detail_calibration_cb_t s_calibration_cb;

static lv_obj_t *create_label(
    lv_obj_t *parent,
    const char *text,
    lv_color_t color,
    lv_align_t align,
    int x,
    int y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(label, align, x, y);
    return label;
}

static lv_obj_t *create_panel(lv_obj_t *parent, int x, int width)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, width, 285);
    lv_obj_align(panel, LV_ALIGN_TOP_LEFT, x, 45);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(panel, WELL_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, WELL_BORDER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 12, LV_PART_MAIN);
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
    lv_obj_set_style_bg_color(button, lv_color_hex(0x12314A), LV_PART_MAIN);
    lv_obj_set_style_bg_color(
        button,
        lv_color_hex(0x1A4566),
        LV_PART_MAIN | LV_STATE_PRESSED
    );
    lv_obj_set_style_radius(button, 7, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(button, WELL_BLUE, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    create_label(button, text, ST_COLOR_TEXT, LV_ALIGN_CENTER, 0, 0);
    return button;
}

static lv_obj_t *create_value_row(
    lv_obj_t *parent,
    const char *name,
    const char *value,
    int y)
{
    create_label(parent, name, ST_COLOR_TEXT_DIM, LV_ALIGN_TOP_LEFT, 0, y);

    lv_obj_t *value_label = create_label(
        parent,
        value,
        ST_COLOR_TEXT,
        LV_ALIGN_TOP_RIGHT,
        0,
        y
    );
    lv_obj_set_width(value_label, 132);
    lv_obj_set_style_text_align(value_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_label_set_long_mode(value_label, LV_LABEL_LONG_DOT);

    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_set_size(line, lv_pct(100), 1);
    lv_obj_align(line, LV_ALIGN_TOP_MID, 0, y + 22);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(line, WELL_ROW_BORDER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(line, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(line, 0, LV_PART_MAIN);

    return value_label;
}

static void back_button_event_cb(lv_event_t *event)
{
    (void)event;
    if (s_back_cb != NULL) {
        s_back_cb();
    }
}

static void calibration_button_event_cb(lv_event_t *event)
{
    (void)event;
    if (s_calibration_cb != NULL) {
        s_calibration_cb();
    }
}

static int level_percent(const well_measurement_t *well)
{
    if (well == NULL || well->well_depth_m <= 0.0f) {
        return 0;
    }

    int percent = (int)((well->water_column_m / well->well_depth_m) * 100.0f);
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    return percent;
}

static const char *health_text(
    sensor_health_t health,
    bool valid,
    int percent,
    const well_settings_t *settings)
{
    if (!valid) return "BRAK DANYCH";
    if (health == SENSOR_HEALTH_OFFLINE) return "CZUJNIK OFFLINE";
    if (health == SENSOR_HEALTH_FAULT) return "BLAD CZUJNIKA";
    if (health == SENSOR_HEALTH_CRITICAL || percent <= settings->critical_percent) {
        return "ALARM - NISKI POZIOM";
    }
    if (health == SENSOR_HEALTH_WARNING || percent <= settings->warning_percent) {
        return "NISKI POZIOM";
    }
    return "POZIOM OK";
}

static lv_color_t health_color(
    sensor_health_t health,
    bool valid,
    int percent,
    const well_settings_t *settings)
{
    if (!valid || health == SENSOR_HEALTH_OFFLINE || health == SENSOR_HEALTH_FAULT) {
        return WELL_RED;
    }
    if (health == SENSOR_HEALTH_CRITICAL || percent <= settings->critical_percent) {
        return WELL_RED;
    }
    if (health == SENSOR_HEALTH_WARNING || percent <= settings->warning_percent) {
        return WELL_YELLOW;
    }
    return WELL_BLUE;
}

lv_obj_t *screen_well_detail_create(
    lv_obj_t *parent,
    screen_well_detail_back_cb_t back_cb,
    screen_well_detail_calibration_cb_t calibration_cb)
{
    s_back_cb = back_cb;
    s_calibration_cb = calibration_cb;

    s_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, 800, 340);
    lv_obj_align(s_root, LV_ALIGN_TOP_MID, 0, 58);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *back_button = create_button(s_root, "< WSTECZ", 105, 34);
    lv_obj_align(back_button, LV_ALIGN_TOP_LEFT, 20, 5);
    lv_obj_add_event_cb(back_button, back_button_event_cb, LV_EVENT_RELEASED, NULL);

    lv_obj_t *calibration_button = create_button(s_root, "KALIBRACJA", 135, 34);
    lv_obj_align(calibration_button, LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_style_bg_color(calibration_button, lv_color_hex(0x514014), LV_PART_MAIN);
    lv_obj_set_style_border_color(calibration_button, WELL_YELLOW, LV_PART_MAIN);
    lv_obj_add_event_cb(
        calibration_button,
        calibration_button_event_cb,
        LV_EVENT_RELEASED,
        NULL
    );

    create_label(
        s_root,
        "Poziom wody / diagnostyka / kanal 4-20 mA",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_TOP_RIGHT,
        -20,
        14
    );

    lv_obj_t *state_panel = create_panel(s_root, 20, 245);
    create_label(state_panel, "AKTUALNY STAN", WELL_BLUE, LV_ALIGN_TOP_LEFT, 0, 0);

    s_arc = lv_arc_create(state_panel);
    lv_obj_set_size(s_arc, 150, 150);
    lv_obj_align(s_arc, LV_ALIGN_TOP_MID, 0, 34);
    lv_arc_set_range(s_arc, 0, 100);
    lv_arc_set_value(s_arc, 0);
    lv_arc_set_bg_angles(s_arc, 135, 45);
    lv_obj_remove_style(s_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(s_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(s_arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(0x0D2744), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc, WELL_BLUE, LV_PART_INDICATOR);

    s_percent_label = create_label(
        state_panel,
        "--%",
        ST_COLOR_TEXT,
        LV_ALIGN_TOP_MID,
        0,
        94
    );
    s_column_label = create_label(
        state_panel,
        "--.-- / --.-- m",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_TOP_MID,
        0,
        124
    );
    s_status_label = create_label(
        state_panel,
        "BRAK DANYCH",
        WELL_RED,
        LV_ALIGN_BOTTOM_MID,
        0,
        -10
    );

    lv_obj_t *measurement_panel = create_panel(s_root, 278, 245);
    create_label(measurement_panel, "POMIAR", WELL_BLUE, LV_ALIGN_TOP_LEFT, 0, 0);
    s_water_column_value = create_value_row(measurement_panel, "Slup wody", "-- m", 30);
    s_depth_value = create_value_row(measurement_panel, "Glebokosc", "-- m", 72);
    s_free_space_value = create_value_row(measurement_panel, "Do lustra", "-- m", 114);
    s_level_value = create_value_row(measurement_panel, "Wypelnienie", "--%", 156);
    s_sample_value = create_value_row(measurement_panel, "Probka", "--", 198);

    lv_obj_t *config_panel = create_panel(s_root, 536, 244);
    create_label(config_panel, "CZUJNIK I KALIBRACJA", WELL_YELLOW, LV_ALIGN_TOP_LEFT, 0, 0);
    s_source_value = create_value_row(config_panel, "Zrodlo", "--", 22);
    s_sensor_value = create_value_row(config_panel, "Czujnik", "--", 50);
    s_input_value = create_value_row(config_panel, "Wejscie", "--", 78);
    s_channel_value = create_value_row(config_panel, "Kanal", "--", 106);
    s_range_value = create_value_row(config_panel, "PUSTA / PELNA", "--", 134);
    s_warning_value = create_value_row(config_panel, "Ostrzezenie", "--", 162);
    s_critical_value = create_value_row(config_panel, "Alarm", "--", 190);

    lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    return s_root;
}

void screen_well_detail_update(const smarttank_state_t *state)
{
    if (state == NULL || s_root == NULL) {
        return;
    }

    well_settings_t settings;
    well_settings_get(&settings);

    const int percent = level_percent(&state->well);
    const lv_color_t status_color = health_color(
        state->well.health,
        state->well.valid,
        percent,
        &settings
    );
    const float free_space_m = state->well.distance_mm / 1000.0f;

    char buffer[64];

    lv_arc_set_value(s_arc, percent);
    lv_obj_set_style_arc_color(s_arc, status_color, LV_PART_INDICATOR);

    snprintf(buffer, sizeof(buffer), "%d%%", percent);
    lv_label_set_text(s_percent_label, buffer);

    snprintf(
        buffer,
        sizeof(buffer),
        "%.2f / %.2f m",
        state->well.water_column_m,
        state->well.well_depth_m
    );
    lv_label_set_text(s_column_label, buffer);

    lv_label_set_text(
        s_status_label,
        health_text(state->well.health, state->well.valid, percent, &settings)
    );
    lv_obj_set_style_text_color(s_status_label, status_color, LV_PART_MAIN);

    snprintf(buffer, sizeof(buffer), "%.2f m", state->well.water_column_m);
    lv_label_set_text(s_water_column_value, buffer);

    snprintf(buffer, sizeof(buffer), "%.2f m", state->well.well_depth_m);
    lv_label_set_text(s_depth_value, buffer);

    snprintf(buffer, sizeof(buffer), "%.2f m / %.2f mA", free_space_m, state->well.current_ma);
    lv_label_set_text(s_free_space_value, buffer);

    snprintf(buffer, sizeof(buffer), "%d%%", percent);
    lv_label_set_text(s_level_value, buffer);

    snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)state->well.sample_counter);
    lv_label_set_text(s_sample_value, buffer);

    if (state->system.modbus_connected) {
        lv_label_set_text(s_source_value, "Modbus RTU");
    } else if (state->system.simulation_active) {
        lv_label_set_text(s_source_value, "Symulacja");
    } else {
        lv_label_set_text(s_source_value, "Offline");
    }

    lv_label_set_text(s_sensor_value, settings.sensor_model);
    lv_label_set_text(s_input_value, settings.input_mode);

    snprintf(buffer, sizeof(buffer), "AI%u", (unsigned int)settings.analog_channel);
    lv_label_set_text(s_channel_value, buffer);

    snprintf(
        buffer,
        sizeof(buffer),
        "%.0f / %.0f mm",
        settings.distance_empty_mm,
        settings.distance_full_mm
    );
    lv_label_set_text(s_range_value, buffer);

    snprintf(buffer, sizeof(buffer), "ponizej %d%%", settings.warning_percent);
    lv_label_set_text(s_warning_value, buffer);

    snprintf(buffer, sizeof(buffer), "ponizej %d%%", settings.critical_percent);
    lv_label_set_text(s_critical_value, buffer);
}
