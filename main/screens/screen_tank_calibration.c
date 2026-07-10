#include "screen_tank_calibration.h"

#include <stdint.h>
#include <stdio.h>

#include "theme.h"

#define CAL_GREEN       lv_color_hex(0x39D12F)
#define CAL_BLUE        lv_color_hex(0x2EA8FF)
#define CAL_YELLOW      lv_color_hex(0xFFC247)
#define CAL_RED         lv_color_hex(0xFF3333)
#define CAL_PANEL       lv_color_hex(0x0B1825)
#define CAL_BORDER      lv_color_hex(0x24384A)
#define CAL_BUTTON      lv_color_hex(0x12314A)
#define CAL_BUTTON_DOWN lv_color_hex(0x1A4566)

#define DISTANCE_STEP_MM 10.0f
#define CAPACITY_STEP_M3 0.10f

typedef enum {
    CAL_ACTION_EMPTY_MINUS = 0,
    CAL_ACTION_EMPTY_PLUS,
    CAL_ACTION_FULL_MINUS,
    CAL_ACTION_FULL_PLUS,
    CAL_ACTION_CAPACITY_MINUS,
    CAL_ACTION_CAPACITY_PLUS,
    CAL_ACTION_WARNING_MINUS,
    CAL_ACTION_WARNING_PLUS,
    CAL_ACTION_CRITICAL_MINUS,
    CAL_ACTION_CRITICAL_PLUS,
    CAL_ACTION_CAPTURE_EMPTY,
    CAL_ACTION_CAPTURE_FULL,
} calibration_action_t;

static lv_obj_t *s_root;
static lv_obj_t *s_live_distance_value;
static lv_obj_t *s_live_current_value;
static lv_obj_t *s_live_sample_value;
static lv_obj_t *s_live_source_value;
static lv_obj_t *s_empty_value;
static lv_obj_t *s_full_value;
static lv_obj_t *s_capacity_value;
static lv_obj_t *s_warning_value;
static lv_obj_t *s_critical_value;
static lv_obj_t *s_validation_label;
static lv_obj_t *s_save_button;

static tank_channel_config_t s_draft;
static float s_live_distance_mm;
static screen_tank_calibration_back_cb_t s_back_cb;
static screen_tank_calibration_save_cb_t s_save_cb;

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
    lv_obj_set_style_bg_color(panel, CAL_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, CAL_BORDER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 12, LV_PART_MAIN);
    return panel;
}

static lv_obj_t *create_simple_button(
    lv_obj_t *parent,
    const char *text,
    int width,
    int height)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_size(button, width, height);
    lv_obj_set_style_bg_color(button, CAL_BUTTON, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, CAL_BUTTON_DOWN, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(button, 7, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(button, CAL_BLUE, LV_PART_MAIN);
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
    return create_label(parent, value, ST_COLOR_TEXT, LV_ALIGN_TOP_RIGHT, 0, y);
}

static bool draft_is_valid(void)
{
    return s_draft.distance_empty_mm > s_draft.distance_full_mm &&
           s_draft.distance_full_mm >= 0.0f &&
           s_draft.capacity_m3 > 0.0f &&
           s_draft.warning_percent >= 1 &&
           s_draft.warning_percent < s_draft.critical_percent &&
           s_draft.critical_percent <= 100;
}

static void refresh_draft_labels(void)
{
    if (s_root == NULL) {
        return;
    }

    char buffer[48];

    snprintf(buffer, sizeof(buffer), "%.0f mm", s_draft.distance_empty_mm);
    lv_label_set_text(s_empty_value, buffer);

    snprintf(buffer, sizeof(buffer), "%.0f mm", s_draft.distance_full_mm);
    lv_label_set_text(s_full_value, buffer);

    snprintf(buffer, sizeof(buffer), "%.2f m3", s_draft.capacity_m3);
    lv_label_set_text(s_capacity_value, buffer);

    snprintf(buffer, sizeof(buffer), "%d%%", s_draft.warning_percent);
    lv_label_set_text(s_warning_value, buffer);

    snprintf(buffer, sizeof(buffer), "%d%%", s_draft.critical_percent);
    lv_label_set_text(s_critical_value, buffer);

    if (draft_is_valid()) {
        lv_label_set_text(s_validation_label, "Ustawienia poprawne");
        lv_obj_set_style_text_color(s_validation_label, CAL_GREEN, LV_PART_MAIN);
        lv_obj_clear_state(s_save_button, LV_STATE_DISABLED);
    } else {
        lv_label_set_text(s_validation_label, "Sprawdz PUSTE/PELNE i progi");
        lv_obj_set_style_text_color(s_validation_label, CAL_RED, LV_PART_MAIN);
        lv_obj_add_state(s_save_button, LV_STATE_DISABLED);
    }
}

static void apply_action(calibration_action_t action)
{
    switch (action) {
        case CAL_ACTION_EMPTY_MINUS:
            if (s_draft.distance_empty_mm - DISTANCE_STEP_MM > s_draft.distance_full_mm) {
                s_draft.distance_empty_mm -= DISTANCE_STEP_MM;
            }
            break;
        case CAL_ACTION_EMPTY_PLUS:
            if (s_draft.distance_empty_mm < 10000.0f) {
                s_draft.distance_empty_mm += DISTANCE_STEP_MM;
            }
            break;
        case CAL_ACTION_FULL_MINUS:
            if (s_draft.distance_full_mm >= DISTANCE_STEP_MM) {
                s_draft.distance_full_mm -= DISTANCE_STEP_MM;
            }
            break;
        case CAL_ACTION_FULL_PLUS:
            if (s_draft.distance_full_mm + DISTANCE_STEP_MM < s_draft.distance_empty_mm) {
                s_draft.distance_full_mm += DISTANCE_STEP_MM;
            }
            break;
        case CAL_ACTION_CAPACITY_MINUS:
            if (s_draft.capacity_m3 > CAPACITY_STEP_M3) {
                s_draft.capacity_m3 -= CAPACITY_STEP_M3;
            }
            break;
        case CAL_ACTION_CAPACITY_PLUS:
            if (s_draft.capacity_m3 < 100.0f) {
                s_draft.capacity_m3 += CAPACITY_STEP_M3;
            }
            break;
        case CAL_ACTION_WARNING_MINUS:
            if (s_draft.warning_percent > 1) {
                s_draft.warning_percent--;
            }
            break;
        case CAL_ACTION_WARNING_PLUS:
            if (s_draft.warning_percent + 1 < s_draft.critical_percent) {
                s_draft.warning_percent++;
            }
            break;
        case CAL_ACTION_CRITICAL_MINUS:
            if (s_draft.critical_percent - 1 > s_draft.warning_percent) {
                s_draft.critical_percent--;
            }
            break;
        case CAL_ACTION_CRITICAL_PLUS:
            if (s_draft.critical_percent < 100) {
                s_draft.critical_percent++;
            }
            break;
        case CAL_ACTION_CAPTURE_EMPTY:
            if (s_live_distance_mm > s_draft.distance_full_mm) {
                s_draft.distance_empty_mm = s_live_distance_mm;
            }
            break;
        case CAL_ACTION_CAPTURE_FULL:
            if (s_live_distance_mm >= 0.0f && s_live_distance_mm < s_draft.distance_empty_mm) {
                s_draft.distance_full_mm = s_live_distance_mm;
            }
            break;
        default:
            break;
    }

    refresh_draft_labels();
}

static void action_event_cb(lv_event_t *event)
{
    const calibration_action_t action =
        (calibration_action_t)(intptr_t)lv_event_get_user_data(event);
    apply_action(action);
}

static lv_obj_t *create_adjust_button(
    lv_obj_t *parent,
    const char *text,
    calibration_action_t action)
{
    lv_obj_t *button = create_simple_button(parent, text, 44, 32);
    lv_obj_add_event_cb(
        button,
        action_event_cb,
        LV_EVENT_RELEASED,
        (void *)(intptr_t)action
    );
    return button;
}

static void create_adjust_row(
    lv_obj_t *parent,
    const char *name,
    lv_obj_t **value_label,
    int y,
    calibration_action_t minus_action,
    calibration_action_t plus_action)
{
    create_label(parent, name, ST_COLOR_TEXT_DIM, LV_ALIGN_TOP_LEFT, 0, y);

    lv_obj_t *minus_button = create_adjust_button(parent, "-", minus_action);
    lv_obj_align(minus_button, LV_ALIGN_TOP_LEFT, 0, y + 20);

    *value_label = create_label(
        parent,
        "--",
        ST_COLOR_TEXT,
        LV_ALIGN_TOP_MID,
        0,
        y + 28
    );

    lv_obj_t *plus_button = create_adjust_button(parent, "+", plus_action);
    lv_obj_align(plus_button, LV_ALIGN_TOP_RIGHT, 0, y + 20);
}

static void back_event_cb(lv_event_t *event)
{
    (void)event;
    if (s_back_cb != NULL) {
        s_back_cb();
    }
}

static void save_event_cb(lv_event_t *event)
{
    (void)event;
    if (draft_is_valid() && s_save_cb != NULL) {
        s_save_cb(&s_draft);
    }
}

lv_obj_t *screen_tank_calibration_create(
    lv_obj_t *parent,
    screen_tank_calibration_back_cb_t back_cb,
    screen_tank_calibration_save_cb_t save_cb)
{
    s_back_cb = back_cb;
    s_save_cb = save_cb;

    s_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, 800, 340);
    lv_obj_align(s_root, LV_ALIGN_TOP_MID, 0, 58);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *back_button = create_simple_button(s_root, "< WSTECZ", 105, 34);
    lv_obj_align(back_button, LV_ALIGN_TOP_LEFT, 20, 5);
    lv_obj_add_event_cb(back_button, back_event_cb, LV_EVENT_RELEASED, NULL);

    create_label(
        s_root,
        "Kalibracja mic+130 / kanal AI1",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_TOP_RIGHT,
        -20,
        14
    );

    lv_obj_t *live_panel = create_panel(s_root, 20, 245);
    create_label(live_panel, "POMIAR NA ZYWO", CAL_BLUE, LV_ALIGN_TOP_LEFT, 0, 0);
    s_live_distance_value = create_value_row(live_panel, "Odleglosc", "-- mm", 40);
    s_live_current_value = create_value_row(live_panel, "Prad petli", "-- mA", 76);
    s_live_sample_value = create_value_row(live_panel, "Probka", "--", 112);
    s_live_source_value = create_value_row(live_panel, "Zrodlo", "--", 148);

    lv_obj_t *capture_empty = create_simple_button(live_panel, "USTAW JAKO PUSTE", 210, 30);
    lv_obj_align(capture_empty, LV_ALIGN_BOTTOM_MID, 0, -35);
    lv_obj_add_event_cb(
        capture_empty,
        action_event_cb,
        LV_EVENT_RELEASED,
        (void *)(intptr_t)CAL_ACTION_CAPTURE_EMPTY
    );

    lv_obj_t *capture_full = create_simple_button(live_panel, "USTAW JAKO PELNE", 210, 30);
    lv_obj_align(capture_full, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(
        capture_full,
        action_event_cb,
        LV_EVENT_RELEASED,
        (void *)(intptr_t)CAL_ACTION_CAPTURE_FULL
    );

    lv_obj_t *geometry_panel = create_panel(s_root, 278, 245);
    create_label(geometry_panel, "ZBIORNIK", CAL_GREEN, LV_ALIGN_TOP_LEFT, 0, 0);
    create_adjust_row(
        geometry_panel,
        "PUSTE - odleglosc",
        &s_empty_value,
        32,
        CAL_ACTION_EMPTY_MINUS,
        CAL_ACTION_EMPTY_PLUS
    );
    create_adjust_row(
        geometry_panel,
        "PELNE - odleglosc",
        &s_full_value,
        98,
        CAL_ACTION_FULL_MINUS,
        CAL_ACTION_FULL_PLUS
    );
    create_adjust_row(
        geometry_panel,
        "Pojemnosc",
        &s_capacity_value,
        164,
        CAL_ACTION_CAPACITY_MINUS,
        CAL_ACTION_CAPACITY_PLUS
    );

    lv_obj_t *alarm_panel = create_panel(s_root, 536, 244);
    create_label(alarm_panel, "ALARMY", CAL_YELLOW, LV_ALIGN_TOP_LEFT, 0, 0);
    create_adjust_row(
        alarm_panel,
        "Ostrzezenie",
        &s_warning_value,
        32,
        CAL_ACTION_WARNING_MINUS,
        CAL_ACTION_WARNING_PLUS
    );
    create_adjust_row(
        alarm_panel,
        "Alarm krytyczny",
        &s_critical_value,
        98,
        CAL_ACTION_CRITICAL_MINUS,
        CAL_ACTION_CRITICAL_PLUS
    );

    s_validation_label = create_label(
        alarm_panel,
        "--",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_TOP_MID,
        0,
        172
    );
    lv_obj_set_style_text_font(s_validation_label, &lv_font_montserrat_12, LV_PART_MAIN);

    lv_obj_t *cancel_button = create_simple_button(alarm_panel, "ANULUJ", 96, 34);
    lv_obj_align(cancel_button, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_add_event_cb(cancel_button, back_event_cb, LV_EVENT_RELEASED, NULL);

    s_save_button = create_simple_button(alarm_panel, "ZAPISZ", 96, 34);
    lv_obj_align(s_save_button, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(s_save_button, lv_color_hex(0x145A28), LV_PART_MAIN);
    lv_obj_set_style_border_color(s_save_button, CAL_GREEN, LV_PART_MAIN);
    lv_obj_add_event_cb(s_save_button, save_event_cb, LV_EVENT_RELEASED, NULL);

    lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    return s_root;
}

void screen_tank_calibration_begin(const smarttank_state_t *state)
{
    if (state == NULL || s_root == NULL) {
        return;
    }

    s_draft = state->tank_config;
    screen_tank_calibration_update_live(state);
    refresh_draft_labels();
}

void screen_tank_calibration_update_live(const smarttank_state_t *state)
{
    if (state == NULL || s_root == NULL) {
        return;
    }

    s_live_distance_mm = state->tank.distance_mm;

    char buffer[48];
    snprintf(buffer, sizeof(buffer), "%.0f mm", state->tank.distance_mm);
    lv_label_set_text(s_live_distance_value, buffer);

    snprintf(buffer, sizeof(buffer), "%.2f mA", state->tank.current_ma);
    lv_label_set_text(s_live_current_value, buffer);

    snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)state->tank.sample_counter);
    lv_label_set_text(s_live_sample_value, buffer);

    if (state->system.simulation_active) {
        lv_label_set_text(s_live_source_value, "Symulacja");
    } else if (state->system.modbus_connected) {
        lv_label_set_text(s_live_source_value, "Modbus RTU");
    } else {
        lv_label_set_text(s_live_source_value, "Offline");
    }
}
