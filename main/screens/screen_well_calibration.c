#include "screen_well_calibration.h"

#include <stdint.h>
#include <stdio.h>

#include "theme.h"

#define CAL_BLUE        lv_color_hex(0x2EA8FF)
#define CAL_GREEN       lv_color_hex(0x39D12F)
#define CAL_YELLOW      lv_color_hex(0xFFC247)
#define CAL_RED         lv_color_hex(0xFF3333)
#define CAL_PANEL       lv_color_hex(0x0B1825)
#define CAL_BORDER      lv_color_hex(0x24384A)
#define CAL_BUTTON      lv_color_hex(0x12314A)
#define CAL_BUTTON_DOWN lv_color_hex(0x1A4566)

#define DISTANCE_STEP_MM 10.0f
#define DEPTH_STEP_M      0.10f

typedef enum {
    ACTION_EMPTY_MINUS = 0,
    ACTION_EMPTY_PLUS,
    ACTION_FULL_MINUS,
    ACTION_FULL_PLUS,
    ACTION_DEPTH_MINUS,
    ACTION_DEPTH_PLUS,
    ACTION_WARNING_MINUS,
    ACTION_WARNING_PLUS,
    ACTION_CRITICAL_MINUS,
    ACTION_CRITICAL_PLUS,
    ACTION_CAPTURE_EMPTY,
    ACTION_CAPTURE_FULL,
} well_calibration_action_t;

static lv_obj_t *s_root;
static lv_obj_t *s_live_distance_value;
static lv_obj_t *s_live_current_value;
static lv_obj_t *s_live_column_value;
static lv_obj_t *s_live_source_value;
static lv_obj_t *s_empty_value;
static lv_obj_t *s_full_value;
static lv_obj_t *s_depth_value;
static lv_obj_t *s_warning_value;
static lv_obj_t *s_critical_value;
static lv_obj_t *s_validation_label;
static lv_obj_t *s_save_button;

static well_settings_t s_draft;
static float s_live_distance_mm;
static screen_well_calibration_back_cb_t s_back_cb;
static screen_well_calibration_save_cb_t s_save_cb;

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

static lv_obj_t *create_button(
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
    lv_obj_t *label = create_label(parent, value, ST_COLOR_TEXT, LV_ALIGN_TOP_RIGHT, 0, y);
    lv_obj_set_width(label, 132);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    return label;
}

static lv_obj_t *create_adjust_button(
    lv_obj_t *parent,
    const char *text,
    well_calibration_action_t action);

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

    snprintf(buffer, sizeof(buffer), "%.2f m", s_draft.well_depth_m);
    lv_label_set_text(s_depth_value, buffer);

    snprintf(buffer, sizeof(buffer), "%d%%", s_draft.warning_percent);
    lv_label_set_text(s_warning_value, buffer);

    snprintf(buffer, sizeof(buffer), "%d%%", s_draft.critical_percent);
    lv_label_set_text(s_critical_value, buffer);

    if (well_settings_is_valid(&s_draft)) {
        lv_label_set_text(s_validation_label, "Ustawienia poprawne");
        lv_obj_set_style_text_color(s_validation_label, CAL_GREEN, LV_PART_MAIN);
        lv_obj_clear_state(s_save_button, LV_STATE_DISABLED);
    } else {
        lv_label_set_text(s_validation_label, "Sprawdz PUSTA/PELNA i progi");
        lv_obj_set_style_text_color(s_validation_label, CAL_RED, LV_PART_MAIN);
        lv_obj_add_state(s_save_button, LV_STATE_DISABLED);
    }
}

static void apply_action(well_calibration_action_t action)
{
    switch (action) {
        case ACTION_EMPTY_MINUS:
            if (s_draft.distance_empty_mm - DISTANCE_STEP_MM > s_draft.distance_full_mm) {
                s_draft.distance_empty_mm -= DISTANCE_STEP_MM;
            }
            break;
        case ACTION_EMPTY_PLUS:
            if (s_draft.distance_empty_mm < 10000.0f) {
                s_draft.distance_empty_mm += DISTANCE_STEP_MM;
            }
            break;
        case ACTION_FULL_MINUS:
            if (s_draft.distance_full_mm >= DISTANCE_STEP_MM) {
                s_draft.distance_full_mm -= DISTANCE_STEP_MM;
            }
            break;
        case ACTION_FULL_PLUS:
            if (s_draft.distance_full_mm + DISTANCE_STEP_MM < s_draft.distance_empty_mm) {
                s_draft.distance_full_mm += DISTANCE_STEP_MM;
            }
            break;
        case ACTION_DEPTH_MINUS:
            if (s_draft.well_depth_m > DEPTH_STEP_M) {
                s_draft.well_depth_m -= DEPTH_STEP_M;
            }
            break;
        case ACTION_DEPTH_PLUS:
            if (s_draft.well_depth_m < 100.0f) {
                s_draft.well_depth_m += DEPTH_STEP_M;
            }
            break;
        case ACTION_WARNING_MINUS:
            if (s_draft.warning_percent - 1 > s_draft.critical_percent) {
                s_draft.warning_percent--;
            }
            break;
        case ACTION_WARNING_PLUS:
            if (s_draft.warning_percent < 100) {
                s_draft.warning_percent++;
            }
            break;
        case ACTION_CRITICAL_MINUS:
            if (s_draft.critical_percent > 0) {
                s_draft.critical_percent--;
            }
            break;
        case ACTION_CRITICAL_PLUS:
            if (s_draft.critical_percent + 1 < s_draft.warning_percent) {
                s_draft.critical_percent++;
            }
            break;
        case ACTION_CAPTURE_EMPTY:
            if (s_live_distance_mm > s_draft.distance_full_mm) {
                s_draft.distance_empty_mm = s_live_distance_mm;
            }
            break;
        case ACTION_CAPTURE_FULL:
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
    const well_calibration_action_t action =
        (well_calibration_action_t)(intptr_t)lv_event_get_user_data(event);
    apply_action(action);
}

static lv_obj_t *create_adjust_button(
    lv_obj_t *parent,
    const char *text,
    well_calibration_action_t action)
{
    lv_obj_t *button = create_button(parent, text, 44, 32);
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
    well_calibration_action_t minus_action,
    well_calibration_action_t plus_action)
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
    if (well_settings_is_valid(&s_draft) && s_save_cb != NULL) {
        s_save_cb(&s_draft);
    }
}

lv_obj_t *screen_well_calibration_create(
    lv_obj_t *parent,
    screen_well_calibration_back_cb_t back_cb,
    screen_well_calibration_save_cb_t save_cb)
{
    s_back_cb = back_cb;
    s_save_cb = save_cb;

    s_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, 800, 340);
    lv_obj_align(s_root, LV_ALIGN_TOP_MID, 0, 58);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *back_button = create_button(s_root, "< WSTECZ", 105, 34);
    lv_obj_align(back_button, LV_ALIGN_TOP_LEFT, 20, 5);
    lv_obj_add_event_cb(back_button, back_event_cb, LV_EVENT_RELEASED, NULL);

    create_label(
        s_root,
        "Kalibracja studni / przyszly kanal AI2",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_TOP_RIGHT,
        -20,
        14
    );

    lv_obj_t *live_panel = create_panel(s_root, 20, 245);
    create_label(live_panel, "POMIAR NA ZYWO", CAL_BLUE, LV_ALIGN_TOP_LEFT, 0, 0);
    s_live_distance_value = create_value_row(live_panel, "Odleglosc", "-- mm", 36);
    s_live_current_value = create_value_row(live_panel, "Prad petli", "-- mA", 72);
    s_live_column_value = create_value_row(live_panel, "Slup wody", "-- m", 108);
    s_live_source_value = create_value_row(live_panel, "Zrodlo", "--", 144);

    lv_obj_t *capture_empty = create_button(live_panel, "USTAW JAKO PUSTA", 210, 30);
    lv_obj_align(capture_empty, LV_ALIGN_BOTTOM_MID, 0, -35);
    lv_obj_add_event_cb(
        capture_empty,
        action_event_cb,
        LV_EVENT_RELEASED,
        (void *)(intptr_t)ACTION_CAPTURE_EMPTY
    );

    lv_obj_t *capture_full = create_button(live_panel, "USTAW JAKO PELNA", 210, 30);
    lv_obj_align(capture_full, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(
        capture_full,
        action_event_cb,
        LV_EVENT_RELEASED,
        (void *)(intptr_t)ACTION_CAPTURE_FULL
    );

    lv_obj_t *geometry_panel = create_panel(s_root, 278, 245);
    create_label(geometry_panel, "STUDNIA", CAL_GREEN, LV_ALIGN_TOP_LEFT, 0, 0);
    create_adjust_row(
        geometry_panel,
        "PUSTA - odleglosc",
        &s_empty_value,
        32,
        ACTION_EMPTY_MINUS,
        ACTION_EMPTY_PLUS
    );
    create_adjust_row(
        geometry_panel,
        "PELNA - odleglosc",
        &s_full_value,
        98,
        ACTION_FULL_MINUS,
        ACTION_FULL_PLUS
    );
    create_adjust_row(
        geometry_panel,
        "Glebokosc studni",
        &s_depth_value,
        164,
        ACTION_DEPTH_MINUS,
        ACTION_DEPTH_PLUS
    );

    lv_obj_t *alarm_panel = create_panel(s_root, 536, 244);
    create_label(alarm_panel, "NISKI POZIOM", CAL_YELLOW, LV_ALIGN_TOP_LEFT, 0, 0);
    create_adjust_row(
        alarm_panel,
        "Ostrzezenie ponizej",
        &s_warning_value,
        32,
        ACTION_WARNING_MINUS,
        ACTION_WARNING_PLUS
    );
    create_adjust_row(
        alarm_panel,
        "Alarm ponizej",
        &s_critical_value,
        98,
        ACTION_CRITICAL_MINUS,
        ACTION_CRITICAL_PLUS
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

    lv_obj_t *cancel_button = create_button(alarm_panel, "ANULUJ", 96, 34);
    lv_obj_align(cancel_button, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_add_event_cb(cancel_button, back_event_cb, LV_EVENT_RELEASED, NULL);

    s_save_button = create_button(alarm_panel, "ZAPISZ", 96, 34);
    lv_obj_align(s_save_button, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(s_save_button, lv_color_hex(0x145A28), LV_PART_MAIN);
    lv_obj_set_style_border_color(s_save_button, CAL_GREEN, LV_PART_MAIN);
    lv_obj_add_event_cb(s_save_button, save_event_cb, LV_EVENT_RELEASED, NULL);

    lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    return s_root;
}

void screen_well_calibration_begin(const smarttank_state_t *state)
{
    if (state == NULL || s_root == NULL) {
        return;
    }

    well_settings_get(&s_draft);
    screen_well_calibration_update_live(state);
    refresh_draft_labels();
}

void screen_well_calibration_update_live(const smarttank_state_t *state)
{
    if (state == NULL || s_root == NULL) {
        return;
    }

    well_settings_t settings;
    well_settings_get(&settings);

    int percent = 0;
    if (state->well.well_depth_m > 0.0f) {
        percent = (int)(state->well.water_column_m * 100.0f / state->well.well_depth_m);
    }
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    const float span_mm = settings.distance_empty_mm - settings.distance_full_mm;
    s_live_distance_mm = settings.distance_empty_mm - span_mm * (float)percent / 100.0f;
    const float current_ma = 4.0f + (float)percent * 0.16f;

    char buffer[48];
    snprintf(buffer, sizeof(buffer), "%.0f mm", s_live_distance_mm);
    lv_label_set_text(s_live_distance_value, buffer);

    snprintf(buffer, sizeof(buffer), "%.2f mA", current_ma);
    lv_label_set_text(s_live_current_value, buffer);

    snprintf(buffer, sizeof(buffer), "%.2f m", state->well.water_column_m);
    lv_label_set_text(s_live_column_value, buffer);

    if (state->system.simulation_active) {
        lv_label_set_text(s_live_source_value, "Symulacja");
    } else if (state->system.modbus_connected) {
        lv_label_set_text(s_live_source_value, "Modbus RTU");
    } else {
        lv_label_set_text(s_live_source_value, "Offline");
    }
}
