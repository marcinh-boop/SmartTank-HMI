#include "screen_tank_detail.h"

#include <stdio.h>

#include "theme.h"

#define DETAIL_GREEN       lv_color_hex(0x39D12F)
#define DETAIL_YELLOW      lv_color_hex(0xFFC247)
#define DETAIL_RED         lv_color_hex(0xFF3333)
#define DETAIL_PANEL       lv_color_hex(0x0B1825)
#define DETAIL_BORDER      lv_color_hex(0x24384A)
#define DETAIL_ROW_BORDER  lv_color_hex(0x183046)

static lv_obj_t *s_root;
static lv_obj_t *s_arc;
static lv_obj_t *s_percent_label;
static lv_obj_t *s_volume_label;
static lv_obj_t *s_status_label;
static lv_obj_t *s_distance_value;
static lv_obj_t *s_current_value;
static lv_obj_t *s_sample_value;
static lv_obj_t *s_source_value;
static lv_obj_t *s_sensor_value;
static lv_obj_t *s_input_value;
static lv_obj_t *s_channel_value;
static lv_obj_t *s_empty_value;
static lv_obj_t *s_full_value;
static lv_obj_t *s_warning_value;
static lv_obj_t *s_critical_value;
static screen_tank_detail_back_cb_t s_back_cb;

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
    lv_obj_set_style_bg_color(panel, DETAIL_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, DETAIL_BORDER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 12, LV_PART_MAIN);
    return panel;
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

    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_set_size(line, lv_pct(100), 1);
    lv_obj_align(line, LV_ALIGN_TOP_MID, 0, y + 24);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(line, DETAIL_ROW_BORDER, LV_PART_MAIN);
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

static const char *health_text(sensor_health_t health, bool valid)
{
    if (!valid) {
        return "BRAK DANYCH";
    }

    switch (health) {
        case SENSOR_HEALTH_WARNING:
            return "OSTRZEZENIE";
        case SENSOR_HEALTH_CRITICAL:
            return "ALARM KRYTYCZNY";
        case SENSOR_HEALTH_OFFLINE:
            return "CZUJNIK OFFLINE";
        case SENSOR_HEALTH_FAULT:
            return "BLAD CZUJNIKA";
        case SENSOR_HEALTH_OK:
        default:
            return "POZIOM OK";
    }
}

static lv_color_t health_color(sensor_health_t health, bool valid)
{
    if (!valid || health == SENSOR_HEALTH_OFFLINE || health == SENSOR_HEALTH_FAULT) {
        return DETAIL_RED;
    }

    if (health == SENSOR_HEALTH_CRITICAL) {
        return DETAIL_RED;
    }

    if (health == SENSOR_HEALTH_WARNING) {
        return DETAIL_YELLOW;
    }

    return DETAIL_GREEN;
}

lv_obj_t *screen_tank_detail_create(
    lv_obj_t *parent,
    screen_tank_detail_back_cb_t back_cb)
{
    s_back_cb = back_cb;

    s_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, 800, 340);
    lv_obj_align(s_root, LV_ALIGN_TOP_MID, 0, 58);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *back_button = lv_btn_create(s_root);
    lv_obj_set_size(back_button, 105, 34);
    lv_obj_align(back_button, LV_ALIGN_TOP_LEFT, 20, 5);
    lv_obj_set_style_bg_color(back_button, lv_color_hex(0x12314A), LV_PART_MAIN);
    lv_obj_set_style_bg_color(back_button, lv_color_hex(0x1A4566), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(back_button, 7, LV_PART_MAIN);
    lv_obj_set_style_border_width(back_button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(back_button, lv_color_hex(0x2EA8FF), LV_PART_MAIN);
    lv_obj_add_event_cb(back_button, back_button_event_cb, LV_EVENT_RELEASED, NULL);
    create_label(back_button, "< WSTECZ", ST_COLOR_TEXT, LV_ALIGN_CENTER, 0, 0);

    create_label(
        s_root,
        "mic+130 / wejscie analogowe / diagnostyka",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_TOP_RIGHT,
        -20,
        14
    );

    lv_obj_t *state_panel = create_panel(s_root, 20, 245);
    create_label(state_panel, "AKTUALNY STAN", DETAIL_GREEN, LV_ALIGN_TOP_LEFT, 0, 0);

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
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(0x12301A), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc, DETAIL_GREEN, LV_PART_INDICATOR);

    s_percent_label = create_label(
        state_panel,
        "--%",
        ST_COLOR_TEXT,
        LV_ALIGN_TOP_MID,
        0,
        94
    );
    s_volume_label = create_label(
        state_panel,
        "--.-- m3",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_TOP_MID,
        0,
        124
    );
    s_status_label = create_label(
        state_panel,
        "BRAK DANYCH",
        DETAIL_RED,
        LV_ALIGN_BOTTOM_MID,
        0,
        -10
    );

    lv_obj_t *measurement_panel = create_panel(s_root, 278, 245);
    create_label(measurement_panel, "POMIAR", lv_color_hex(0x2EA8FF), LV_ALIGN_TOP_LEFT, 0, 0);
    s_distance_value = create_value_row(measurement_panel, "Odleglosc", "-- mm", 38);
    s_current_value = create_value_row(measurement_panel, "Prad petli", "-- mA", 83);
    s_sample_value = create_value_row(measurement_panel, "Probka", "--", 128);
    s_source_value = create_value_row(measurement_panel, "Zrodlo", "--", 173);
    create_label(
        measurement_panel,
        "Dane surowe beda pochodziły z Modbus RTU.",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_BOTTOM_LEFT,
        0,
        -4
    );

    lv_obj_t *config_panel = create_panel(s_root, 536, 244);
    create_label(config_panel, "CZUJNIK I KALIBRACJA", DETAIL_YELLOW, LV_ALIGN_TOP_LEFT, 0, 0);
    s_sensor_value = create_value_row(config_panel, "Model", "--", 32);
    s_input_value = create_value_row(config_panel, "Wejscie", "--", 65);
    s_channel_value = create_value_row(config_panel, "Kanal", "--", 98);
    s_empty_value = create_value_row(config_panel, "PUSTE", "-- mm", 131);
    s_full_value = create_value_row(config_panel, "PELNE", "-- mm", 164);
    s_warning_value = create_value_row(config_panel, "Ostrzezenie", "--%", 197);
    s_critical_value = create_value_row(config_panel, "Alarm", "--%", 230);

    lv_obj_t *calibration_button = lv_btn_create(config_panel);
    lv_obj_set_size(calibration_button, 210, 32);
    lv_obj_align(calibration_button, LV_ALIGN_BOTTOM_MID, 0, 2);
    lv_obj_set_style_bg_color(calibration_button, lv_color_hex(0x17212A), LV_PART_MAIN);
    lv_obj_set_style_radius(calibration_button, 7, LV_PART_MAIN);
    lv_obj_set_style_border_width(calibration_button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(calibration_button, DETAIL_BORDER, LV_PART_MAIN);
    lv_obj_add_state(calibration_button, LV_STATE_DISABLED);
    create_label(
        calibration_button,
        "KALIBRACJA - NASTEPNY ETAP",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_CENTER,
        0,
        0
    );

    lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    return s_root;
}

void screen_tank_detail_update(const smarttank_state_t *state)
{
    if (state == NULL || s_root == NULL) {
        return;
    }

    int percent = state->tank.level_percent;
    if (percent < 0) {
        percent = 0;
    }
    if (percent > 100) {
        percent = 100;
    }

    const lv_color_t status_color = health_color(state->tank.health, state->tank.valid);
    char buffer[64];

    lv_arc_set_value(s_arc, percent);
    lv_obj_set_style_arc_color(s_arc, status_color, LV_PART_INDICATOR);

    snprintf(buffer, sizeof(buffer), "%d%%", percent);
    lv_label_set_text(s_percent_label, buffer);

    snprintf(buffer, sizeof(buffer), "%.2f / %.2f m3", state->tank.volume_m3, state->tank.capacity_m3);
    lv_label_set_text(s_volume_label, buffer);

    lv_label_set_text(s_status_label, health_text(state->tank.health, state->tank.valid));
    lv_obj_set_style_text_color(s_status_label, status_color, LV_PART_MAIN);

    snprintf(buffer, sizeof(buffer), "%.0f mm", state->tank.distance_mm);
    lv_label_set_text(s_distance_value, buffer);

    snprintf(buffer, sizeof(buffer), "%.2f mA", state->tank.current_ma);
    lv_label_set_text(s_current_value, buffer);

    snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)state->tank.sample_counter);
    lv_label_set_text(s_sample_value, buffer);

    if (state->system.simulation_active) {
        lv_label_set_text(s_source_value, "Symulacja");
    } else if (state->system.modbus_connected) {
        lv_label_set_text(s_source_value, "Modbus RTU");
    } else {
        lv_label_set_text(s_source_value, "Offline");
    }

    lv_label_set_text(s_sensor_value, state->tank_config.sensor_model);
    lv_label_set_text(s_input_value, state->tank_config.input_mode);

    snprintf(buffer, sizeof(buffer), "AI%u", (unsigned int)state->tank_config.analog_channel);
    lv_label_set_text(s_channel_value, buffer);

    snprintf(buffer, sizeof(buffer), "%.0f mm", state->tank_config.distance_empty_mm);
    lv_label_set_text(s_empty_value, buffer);

    snprintf(buffer, sizeof(buffer), "%.0f mm", state->tank_config.distance_full_mm);
    lv_label_set_text(s_full_value, buffer);

    snprintf(buffer, sizeof(buffer), "%d%%", state->tank_config.warning_percent);
    lv_label_set_text(s_warning_value, buffer);

    snprintf(buffer, sizeof(buffer), "%d%%", state->tank_config.critical_percent);
    lv_label_set_text(s_critical_value, buffer);
}
