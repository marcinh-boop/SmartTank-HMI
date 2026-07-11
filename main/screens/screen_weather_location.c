#include "screen_weather_location.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "theme.h"
#include "weather_geocoding.h"
#include "weather_location_storage.h"

#define LOCATION_BLUE    lv_color_hex(0x2EA8FF)
#define LOCATION_GREEN   lv_color_hex(0x39D12F)
#define LOCATION_YELLOW  lv_color_hex(0xFFC247)
#define LOCATION_RED     lv_color_hex(0xFF3333)
#define LOCATION_PANEL   lv_color_hex(0x0B1825)
#define LOCATION_BORDER  lv_color_hex(0x24384A)

#define LOCATION_RESULT_ROWS WEATHER_GEOCODING_MAX_RESULTS

typedef struct {
    uint8_t index;
} location_result_ctx_t;

static lv_obj_t *s_root;
static lv_timer_t *s_refresh_timer;
static lv_obj_t *s_current_label;
static lv_obj_t *s_query_textarea;
static lv_obj_t *s_status_label;
static lv_obj_t *s_keyboard;
static lv_obj_t *s_results_layer;
static lv_obj_t *s_results_info;
static lv_obj_t *s_result_buttons[LOCATION_RESULT_ROWS];
static lv_obj_t *s_result_labels[LOCATION_RESULT_ROWS];
static location_result_ctx_t s_result_contexts[LOCATION_RESULT_ROWS];
static uint32_t s_last_revision;

static lv_obj_t *create_label(
    lv_obj_t *parent,
    const char *text,
    lv_color_t color,
    lv_align_t align,
    int x,
    int y,
    const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_align(label, align, x, y);
    return label;
}

static lv_obj_t *create_panel(lv_obj_t *parent, int x, int width)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, width, 320);
    lv_obj_align(panel, LV_ALIGN_TOP_LEFT, x, 68);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(panel, LOCATION_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 12, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, LOCATION_BORDER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 12, LV_PART_MAIN);
    return panel;
}

static lv_obj_t *create_action_button(
    lv_obj_t *parent,
    const char *text,
    lv_align_t align,
    lv_color_t color,
    lv_event_cb_t callback)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_size(button, 130, 42);
    lv_obj_align(button, align, 0, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x12314A), LV_PART_MAIN);
    lv_obj_set_style_bg_color(
        button,
        lv_color_hex(0x1A4566),
        LV_PART_MAIN | LV_STATE_PRESSED
    );
    lv_obj_set_style_radius(button, 7, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(button, color, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(button, callback, LV_EVENT_RELEASED, NULL);

    create_label(
        button,
        text,
        ST_COLOR_TEXT,
        LV_ALIGN_CENTER,
        0,
        0,
        &lv_font_montserrat_14
    );
    return button;
}

static void set_status(const char *text, lv_color_t color)
{
    if (s_status_label == NULL) {
        return;
    }

    lv_label_set_text(s_status_label, text);
    lv_obj_set_style_text_color(s_status_label, color, LV_PART_MAIN);
}

static void format_location_name(
    const weather_location_t *location,
    char *buffer,
    size_t buffer_size)
{
    if (location == NULL || buffer == NULL || buffer_size == 0U) {
        return;
    }

    if (location->admin1[0] != '\0' && location->country[0] != '\0') {
        snprintf(
            buffer,
            buffer_size,
            "%s, %s, %s",
            location->name,
            location->admin1,
            location->country
        );
    } else if (location->country[0] != '\0') {
        snprintf(
            buffer,
            buffer_size,
            "%s, %s",
            location->name,
            location->country
        );
    } else {
        snprintf(buffer, buffer_size, "%s", location->name);
    }
}

static void refresh_current_location(void)
{
    weather_location_t location;
    const esp_err_t result = weather_location_storage_load(&location);

    if (result == ESP_OK) {
        char name[176];
        char buffer[256];
        format_location_name(&location, name, sizeof(name));
        snprintf(
            buffer,
            sizeof(buffer),
            "%s\n%.5f, %.5f\n%s",
            name,
            location.latitude,
            location.longitude,
            location.timezone[0] != '\0' ? location.timezone : "Strefa: auto"
        );
        lv_label_set_text(s_current_label, buffer);
        lv_obj_set_style_text_color(s_current_label, ST_COLOR_TEXT, LV_PART_MAIN);
    } else if (result == ESP_ERR_NVS_NOT_FOUND) {
        lv_label_set_text(
            s_current_label,
            "Brak zapisanej lokalizacji.\nWyszukaj miejscowosc lub kod pocztowy."
        );
        lv_obj_set_style_text_color(s_current_label, ST_COLOR_TEXT_DIM, LV_PART_MAIN);
    } else {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "Blad odczytu NVS: 0x%X", (unsigned int)result);
        lv_label_set_text(s_current_label, buffer);
        lv_obj_set_style_text_color(s_current_label, LOCATION_RED, LV_PART_MAIN);
    }
}

static void clear_result_rows(void)
{
    for (uint8_t index = 0U; index < LOCATION_RESULT_ROWS; index++) {
        lv_label_set_text(s_result_labels[index], "");
        lv_obj_add_flag(s_result_buttons[index], LV_OBJ_FLAG_HIDDEN);
    }
}

static void show_keyboard_view(void)
{
    if (s_keyboard == NULL || s_results_layer == NULL) {
        return;
    }

    lv_obj_add_flag(s_results_layer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(s_keyboard, s_query_textarea);
}

static void show_results_view(void)
{
    if (s_keyboard == NULL || s_results_layer == NULL) {
        return;
    }

    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_results_layer, LV_OBJ_FLAG_HIDDEN);
}

static void query_focus_event_cb(lv_event_t *event)
{
    (void)event;
    show_keyboard_view();
}

static void search_button_event_cb(lv_event_t *event)
{
    (void)event;

    const char *query = lv_textarea_get_text(s_query_textarea);
    const esp_err_t result = weather_geocoding_request(query);

    if (result == ESP_ERR_INVALID_ARG) {
        set_status("Wpisz co najmniej 2 znaki.", LOCATION_RED);
        return;
    }

    if (result != ESP_OK) {
        char buffer[64];
        snprintf(
            buffer,
            sizeof(buffer),
            "Nie mozna uruchomic wyszukiwania: 0x%X",
            (unsigned int)result
        );
        set_status(buffer, LOCATION_RED);
        return;
    }

    clear_result_rows();
    lv_label_set_text(s_results_info, "Wyszukiwanie lokalizacji...");
    show_results_view();
    set_status("Laczenie z Open-Meteo...", LOCATION_BLUE);
}

static void back_button_event_cb(lv_event_t *event)
{
    (void)event;
    screen_weather_location_hide();
}

static void keyboard_event_cb(lv_event_t *event)
{
    const lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_READY) {
        search_button_event_cb(event);
    } else if (code == LV_EVENT_CANCEL) {
        screen_weather_location_hide();
    }
}

static void result_row_event_cb(lv_event_t *event)
{
    const location_result_ctx_t *context = lv_event_get_user_data(event);
    if (context == NULL) {
        return;
    }

    weather_geocoding_snapshot_t snapshot;
    weather_geocoding_get_snapshot(&snapshot);
    if (context->index >= snapshot.result_count) {
        return;
    }

    const weather_location_t *location = &snapshot.results[context->index];
    const esp_err_t result = weather_location_storage_save(location);
    if (result != ESP_OK) {
        char buffer[64];
        snprintf(
            buffer,
            sizeof(buffer),
            "Blad zapisu lokalizacji: 0x%X",
            (unsigned int)result
        );
        set_status(buffer, LOCATION_RED);
        return;
    }

    char name[176];
    char status[224];
    format_location_name(location, name, sizeof(name));
    snprintf(status, sizeof(status), "ZAPISANO: %s", name);
    set_status(status, LOCATION_GREEN);
    refresh_current_location();
}

static void populate_results(const weather_geocoding_snapshot_t *snapshot)
{
    clear_result_rows();

    if (snapshot->searching) {
        lv_label_set_text(s_results_info, "Wyszukiwanie lokalizacji...");
        set_status("Laczenie z Open-Meteo...", LOCATION_BLUE);
        return;
    }

    if (snapshot->waiting_for_wifi) {
        lv_label_set_text(s_results_info, "Brak polaczenia z internetem.");
        set_status("Najpierw polacz panel z Wi-Fi.", LOCATION_YELLOW);
        return;
    }

    if (snapshot->last_error != ESP_OK) {
        char buffer[96];
        if (snapshot->http_status != 0) {
            snprintf(
                buffer,
                sizeof(buffer),
                "Blad API: HTTP %d, kod 0x%X",
                snapshot->http_status,
                (unsigned int)snapshot->last_error
            );
        } else {
            snprintf(
                buffer,
                sizeof(buffer),
                "Blad wyszukiwania: 0x%X",
                (unsigned int)snapshot->last_error
            );
        }
        lv_label_set_text(s_results_info, buffer);
        set_status("Wyszukiwanie nie powiodlo sie.", LOCATION_RED);
        return;
    }

    if (snapshot->result_count == 0U) {
        lv_label_set_text(s_results_info, "Brak wynikow. Doprecyzuj nazwe.");
        set_status("Nie znaleziono lokalizacji.", LOCATION_YELLOW);
        return;
    }

    lv_label_set_text(s_results_info, "Dotknij wyniku, aby zapisac lokalizacje.");
    set_status("Wybierz prawidlowa pozycje z listy.", ST_COLOR_TEXT_DIM);

    char line[256];
    char name[176];
    for (uint8_t index = 0U; index < snapshot->result_count; index++) {
        const weather_location_t *location = &snapshot->results[index];
        format_location_name(location, name, sizeof(name));
        snprintf(
            line,
            sizeof(line),
            "%u. %s\n    %.4f, %.4f  %s",
            (unsigned int)(index + 1U),
            name,
            location->latitude,
            location->longitude,
            location->timezone
        );
        lv_label_set_text(s_result_labels[index], line);
        lv_obj_clear_flag(s_result_buttons[index], LV_OBJ_FLAG_HIDDEN);
    }
}

static void refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (s_root == NULL || lv_obj_has_flag(s_root, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    weather_geocoding_snapshot_t snapshot;
    weather_geocoding_get_snapshot(&snapshot);
    if (snapshot.revision == s_last_revision) {
        return;
    }

    s_last_revision = snapshot.revision;
    populate_results(&snapshot);
}

static void build_screen(lv_obj_t *parent_screen)
{
    s_root = lv_obj_create(parent_screen);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, 800, 398);
    lv_obj_align(s_root, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_root, ST_COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *top = lv_obj_create(s_root);
    lv_obj_set_size(top, 760, 50);
    lv_obj_align(top, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_clear_flag(top, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(top, lv_color_hex(0x08131F), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(top, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(top, 10, LV_PART_MAIN);
    lv_obj_set_style_border_width(top, 0, LV_PART_MAIN);

    create_label(
        top,
        "SmartTank HMI",
        ST_COLOR_TEXT,
        LV_ALIGN_LEFT_MID,
        12,
        0,
        &lv_font_montserrat_14
    );
    create_label(
        top,
        "LOKALIZACJA POGODY",
        ST_COLOR_ACCENT,
        LV_ALIGN_CENTER,
        0,
        0,
        &lv_font_montserrat_14
    );
    create_label(
        top,
        "OPEN-METEO",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_RIGHT_MID,
        -12,
        0,
        &lv_font_montserrat_14
    );

    lv_obj_t *form_panel = create_panel(s_root, 20, 300);
    create_label(
        form_panel,
        "WYBRANA LOKALIZACJA",
        LOCATION_YELLOW,
        LV_ALIGN_TOP_LEFT,
        0,
        0,
        &lv_font_montserrat_14
    );

    s_current_label = create_label(
        form_panel,
        "Brak zapisanej lokalizacji.",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_TOP_LEFT,
        0,
        28,
        &lv_font_montserrat_12
    );
    lv_obj_set_width(s_current_label, 270);
    lv_label_set_long_mode(s_current_label, LV_LABEL_LONG_WRAP);

    create_label(
        form_panel,
        "MIEJSCOWOSC LUB KOD POCZTOWY",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_TOP_LEFT,
        0,
        104,
        &lv_font_montserrat_12
    );

    s_query_textarea = lv_textarea_create(form_panel);
    lv_obj_set_size(s_query_textarea, 270, 42);
    lv_obj_align(s_query_textarea, LV_ALIGN_TOP_LEFT, 0, 122);
    lv_textarea_set_one_line(s_query_textarea, true);
    lv_textarea_set_max_length(
        s_query_textarea,
        WEATHER_GEOCODING_QUERY_MAX_LEN
    );
    lv_textarea_set_placeholder_text(s_query_textarea, "np. Krakow lub 30-001");
    lv_obj_add_event_cb(
        s_query_textarea,
        query_focus_event_cb,
        LV_EVENT_FOCUSED,
        NULL
    );

    s_status_label = create_label(
        form_panel,
        "Wpisz nazwe i nacisnij SZUKAJ.",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_TOP_LEFT,
        0,
        174,
        &lv_font_montserrat_12
    );
    lv_obj_set_width(s_status_label, 270);
    lv_label_set_long_mode(s_status_label, LV_LABEL_LONG_WRAP);

    create_action_button(
        form_panel,
        "SZUKAJ",
        LV_ALIGN_BOTTOM_LEFT,
        LOCATION_GREEN,
        search_button_event_cb
    );
    create_action_button(
        form_panel,
        "WROC",
        LV_ALIGN_BOTTOM_RIGHT,
        LOCATION_RED,
        back_button_event_cb
    );

    lv_obj_t *right_panel = create_panel(s_root, 332, 448);
    create_label(
        right_panel,
        "WYSZUKIWANIE",
        LOCATION_BLUE,
        LV_ALIGN_TOP_LEFT,
        0,
        0,
        &lv_font_montserrat_14
    );

    s_keyboard = lv_keyboard_create(right_panel);
    lv_obj_set_size(s_keyboard, 422, 270);
    lv_obj_align(s_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_mode(s_keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(s_keyboard, s_query_textarea);
    lv_obj_add_event_cb(s_keyboard, keyboard_event_cb, LV_EVENT_ALL, NULL);

    s_results_layer = lv_obj_create(right_panel);
    lv_obj_remove_style_all(s_results_layer);
    lv_obj_set_size(s_results_layer, 422, 270);
    lv_obj_align(s_results_layer, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(s_results_layer, LV_OBJ_FLAG_SCROLLABLE);

    s_results_info = create_label(
        s_results_layer,
        "Wyniki pojawia sie po wyszukaniu.",
        ST_COLOR_TEXT_DIM,
        LV_ALIGN_TOP_LEFT,
        0,
        0,
        &lv_font_montserrat_12
    );
    lv_obj_set_width(s_results_info, 422);
    lv_label_set_long_mode(s_results_info, LV_LABEL_LONG_DOT);

    for (uint8_t index = 0U; index < LOCATION_RESULT_ROWS; index++) {
        s_result_contexts[index].index = index;

        s_result_buttons[index] = lv_btn_create(s_results_layer);
        lv_obj_remove_style_all(s_result_buttons[index]);
        lv_obj_set_size(s_result_buttons[index], 422, 44);
        lv_obj_align(
            s_result_buttons[index],
            LV_ALIGN_TOP_LEFT,
            0,
            28 + ((int)index * 48)
        );
        lv_obj_set_style_bg_color(
            s_result_buttons[index],
            lv_color_hex(0x12314A),
            LV_PART_MAIN | LV_STATE_DEFAULT
        );
        lv_obj_set_style_bg_color(
            s_result_buttons[index],
            lv_color_hex(0x1A4566),
            LV_PART_MAIN | LV_STATE_PRESSED
        );
        lv_obj_set_style_bg_opa(
            s_result_buttons[index],
            LV_OPA_COVER,
            LV_PART_MAIN
        );
        lv_obj_set_style_radius(s_result_buttons[index], 6, LV_PART_MAIN);
        lv_obj_set_style_border_width(s_result_buttons[index], 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(
            s_result_buttons[index],
            LOCATION_BORDER,
            LV_PART_MAIN
        );
        lv_obj_add_event_cb(
            s_result_buttons[index],
            result_row_event_cb,
            LV_EVENT_RELEASED,
            &s_result_contexts[index]
        );

        s_result_labels[index] = create_label(
            s_result_buttons[index],
            "",
            ST_COLOR_TEXT,
            LV_ALIGN_LEFT_MID,
            6,
            0,
            &lv_font_montserrat_12
        );
        lv_obj_set_width(s_result_labels[index], 408);
        lv_label_set_long_mode(s_result_labels[index], LV_LABEL_LONG_DOT);
        lv_obj_add_flag(s_result_buttons[index], LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_add_flag(s_results_layer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);

    s_refresh_timer = lv_timer_create(refresh_timer_cb, 400, NULL);
}

void screen_weather_location_open(lv_obj_t *parent_screen)
{
    if (parent_screen == NULL) {
        return;
    }

    if (s_root == NULL) {
        build_screen(parent_screen);
    }

    refresh_current_location();
    show_keyboard_view();
    s_last_revision = 0U;
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_root);
    lv_obj_add_state(s_query_textarea, LV_STATE_FOCUSED);
}

void screen_weather_location_hide(void)
{
    if (s_root == NULL) {
        return;
    }

    lv_keyboard_set_textarea(s_keyboard, NULL);
    lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
}
