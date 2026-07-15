/*
 * Moduł well_detail_integration.c należy do warstwy głównej programu SmartTank.
 * Realizuje logikę modułu i ukrywa jej szczegóły za publicznym interfejsem.
 * Oddzielenie tej odpowiedzialności ułatwia diagnostykę, testy i dalszą rozbudowę.
 */
#include "well_detail_integration.h"

#include <stdint.h>
#include <stdlib.h>

#include "app_model.h"
#include "esp_log.h"
#include "screen_well_calibration.h"
#include "screen_well_detail.h"
#include "top_status_bar.h"
#include "well_settings.h"

#define DASHBOARD_LAYER_WIDTH   800
#define DASHBOARD_LAYER_HEIGHT  340
#define DASHBOARD_LAYER_Y       58
#define DASHBOARD_CARD_WIDTH    245
#define DASHBOARD_CARD_HEIGHT   330
#define BOTTOM_BAR_WIDTH        760
#define BOTTOM_BAR_HEIGHT       50
#define NAV_BUTTON_COUNT        6U
#define REFRESH_INTERVAL_MS     500U

static const char *TAG = "well_integration";
static lv_obj_t *s_screen;
static lv_obj_t *s_dashboard_content;
static lv_obj_t *s_well_card;
static lv_obj_t *s_detail_content;
static lv_obj_t *s_calibration_content;
static lv_timer_t *s_refresh_timer;
static uint8_t s_nav_indices[NAV_BUTTON_COUNT];

static bool approximately_equal(int value, int expected, int tolerance)
{
    return abs(value - expected) <= tolerance;
}

static bool find_dashboard_and_well_card(
    lv_obj_t *screen,
    lv_obj_t **dashboard_content,
    lv_obj_t **well_card)
{
    if (screen == NULL || dashboard_content == NULL || well_card == NULL) {
        return false;
    }

    lv_obj_update_layout(screen);

    const uint32_t screen_child_count = lv_obj_get_child_cnt(screen);
    for (uint32_t i = 0U; i < screen_child_count; i++) {
        lv_obj_t *candidate_layer = lv_obj_get_child(screen, (int32_t)i);
        if (candidate_layer == NULL) {
            continue;
        }

        if (!approximately_equal(lv_obj_get_width(candidate_layer), DASHBOARD_LAYER_WIDTH, 4) ||
            !approximately_equal(lv_obj_get_height(candidate_layer), DASHBOARD_LAYER_HEIGHT, 4) ||
            !approximately_equal(lv_obj_get_y(candidate_layer), DASHBOARD_LAYER_Y, 4)) {
            continue;
        }

        uint32_t card_count = 0U;
        lv_obj_t *middle_card = NULL;
        const uint32_t layer_child_count = lv_obj_get_child_cnt(candidate_layer);

        for (uint32_t j = 0U; j < layer_child_count; j++) {
            lv_obj_t *candidate_card = lv_obj_get_child(candidate_layer, (int32_t)j);
            if (candidate_card == NULL) {
                continue;
            }

            if (!approximately_equal(lv_obj_get_width(candidate_card), DASHBOARD_CARD_WIDTH, 4) ||
                !approximately_equal(lv_obj_get_height(candidate_card), DASHBOARD_CARD_HEIGHT, 4)) {
                continue;
            }

            card_count++;
            const int x = lv_obj_get_x(candidate_card);
            if (x >= 240 && x <= 320) {
                middle_card = candidate_card;
            }
        }

        if (card_count >= 3U && middle_card != NULL) {
            *dashboard_content = candidate_layer;
            *well_card = middle_card;
            return true;
        }
    }

    return false;
}

static lv_obj_t *find_bottom_bar(lv_obj_t *screen)
{
    if (screen == NULL) {
        return NULL;
    }

    lv_obj_update_layout(screen);

    const uint32_t child_count = lv_obj_get_child_cnt(screen);
    for (uint32_t i = 0U; i < child_count; i++) {
        lv_obj_t *child = lv_obj_get_child(screen, (int32_t)i);
        if (child == NULL) {
            continue;
        }

        if (approximately_equal(lv_obj_get_width(child), BOTTOM_BAR_WIDTH, 4) &&
            approximately_equal(lv_obj_get_height(child), BOTTOM_BAR_HEIGHT, 4) &&
            lv_obj_get_y(child) > 350) {
            return child;
        }
    }

    return NULL;
}

static void refresh_visible_content(void)
{
    smarttank_state_t state;
    app_model_get_snapshot(&state);

    if (s_detail_content != NULL &&
        !lv_obj_has_flag(s_detail_content, LV_OBJ_FLAG_HIDDEN)) {
        screen_well_detail_update(&state);
    }

    if (s_calibration_content != NULL &&
        !lv_obj_has_flag(s_calibration_content, LV_OBJ_FLAG_HIDDEN)) {
        screen_well_calibration_update_live(&state);
    }
}

static void refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    refresh_visible_content();
}

static void show_dashboard(void)
{
    if (s_dashboard_content != NULL) {
        lv_obj_clear_flag(s_dashboard_content, LV_OBJ_FLAG_HIDDEN);
    }
}

static void hide_well_screens(void)
{
    if (s_detail_content != NULL) {
        lv_obj_add_flag(s_detail_content, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_calibration_content != NULL) {
        lv_obj_add_flag(s_calibration_content, LV_OBJ_FLAG_HIDDEN);
    }
    top_status_bar_clear_page_override();
}

static void show_detail(void)
{
    smarttank_state_t state;
    app_model_get_snapshot(&state);
    screen_well_detail_update(&state);

    if (s_dashboard_content != NULL) {
        lv_obj_add_flag(s_dashboard_content, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_calibration_content != NULL) {
        lv_obj_add_flag(s_calibration_content, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_clear_flag(s_detail_content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_detail_content);
    top_status_bar_set_page_title("STUDNIA");
}

static void show_calibration(void)
{
    smarttank_state_t state;
    app_model_get_snapshot(&state);
    screen_well_calibration_begin(&state);

    lv_obj_add_flag(s_detail_content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_calibration_content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_calibration_content);
    top_status_bar_set_page_title("KALIBRACJA STUDNI");
}

static void detail_back_cb(void)
{
    hide_well_screens();
    show_dashboard();
}

static void detail_calibration_cb(void)
{
    show_calibration();
}

static void calibration_back_cb(void)
{
    show_detail();
}

static void calibration_save_cb(const well_settings_t *settings)
{
    const esp_err_t err = well_settings_save(settings);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Unable to save well calibration: %s", esp_err_to_name(err));
        return;
    }

    show_detail();
}

static void well_card_event_cb(lv_event_t *event)
{
    (void)event;

    if (s_dashboard_content == NULL || s_detail_content == NULL) {
        return;
    }

    show_detail();
}

static void nav_button_event_cb(lv_event_t *event)
{
    const uint8_t *index = lv_event_get_user_data(event);
    hide_well_screens();

    if (index != NULL && *index == 0U) {
        show_dashboard();
    }
}

static bool attach_navigation_callbacks(lv_obj_t *bottom_bar)
{
    if (bottom_bar == NULL) {
        return false;
    }

    const uint32_t child_count = lv_obj_get_child_cnt(bottom_bar);
    if (child_count < NAV_BUTTON_COUNT) {
        return false;
    }

    for (uint32_t i = 0U; i < NAV_BUTTON_COUNT; i++) {
        lv_obj_t *button = lv_obj_get_child(bottom_bar, (int32_t)i);
        if (button == NULL) {
            return false;
        }

        s_nav_indices[i] = (uint8_t)i;
        lv_obj_add_event_cb(
            button,
            nav_button_event_cb,
            LV_EVENT_RELEASED,
            &s_nav_indices[i]
        );
    }

    return true;
}

bool well_detail_integration_attach(lv_obj_t *screen)
{
    if (screen == NULL) {
        return false;
    }

    if (s_detail_content != NULL) {
        return s_screen == screen;
    }

    lv_obj_t *dashboard_content = NULL;
    lv_obj_t *well_card = NULL;
    if (!find_dashboard_and_well_card(screen, &dashboard_content, &well_card)) {
        return false;
    }

    lv_obj_t *bottom_bar = find_bottom_bar(screen);
    if (bottom_bar == NULL) {
        return false;
    }

    s_screen = screen;
    s_dashboard_content = dashboard_content;
    s_well_card = well_card;
    s_detail_content = screen_well_detail_create(
        screen,
        detail_back_cb,
        detail_calibration_cb
    );
    s_calibration_content = screen_well_calibration_create(
        screen,
        calibration_back_cb,
        calibration_save_cb
    );

    if (s_detail_content == NULL || s_calibration_content == NULL) {
        if (s_detail_content != NULL) lv_obj_del(s_detail_content);
        if (s_calibration_content != NULL) lv_obj_del(s_calibration_content);
        s_screen = NULL;
        s_dashboard_content = NULL;
        s_well_card = NULL;
        s_detail_content = NULL;
        s_calibration_content = NULL;
        return false;
    }

    lv_obj_add_flag(s_well_card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(
        s_well_card,
        lv_color_hex(0x10233A),
        LV_PART_MAIN | LV_STATE_PRESSED
    );
    lv_obj_add_event_cb(
        s_well_card,
        well_card_event_cb,
        LV_EVENT_RELEASED,
        NULL
    );

    if (!attach_navigation_callbacks(bottom_bar)) {
        lv_obj_del(s_detail_content);
        lv_obj_del(s_calibration_content);
        s_screen = NULL;
        s_dashboard_content = NULL;
        s_well_card = NULL;
        s_detail_content = NULL;
        s_calibration_content = NULL;
        return false;
    }

    s_refresh_timer = lv_timer_create(
        refresh_timer_cb,
        REFRESH_INTERVAL_MS,
        NULL
    );

    return s_refresh_timer != NULL;
}
