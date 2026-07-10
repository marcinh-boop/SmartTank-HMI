#include "screen_dashboard.h"
#include "screen_history.h"
#include "theme.h"
#include "lvgl.h"

#include "tank_widget.h"
#include "well_widget.h"
#include "weather_widget.h"
#include "bottom_nav.h"

static lv_obj_t *s_dashboard_screen = NULL;

static void load_history_async(void *user_data)
{
    (void)user_data;
    screen_history_create();
}

static bool dashboard_nav_change(bottom_nav_page_t page)
{
    if (page == NAV_HISTORY) {
        /*
         * Zmieniamy ekran dopiero po zakonczeniu obslugi dotyku.
         * Zapobiega to przeniesieniu zdarzenia RELEASED na nowy ekran.
         */
        lv_async_call(load_history_async, NULL);
    }

    /*
     * Nie zmieniamy stanu starego paska. Nowy ekran ma juz poprawnie
     * ustawiona aktywna zakladke.
     */
    return false;
}

static lv_obj_t *create_card(lv_obj_t *parent, lv_color_t color)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 245, 330);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(card, ST_COLOR_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 14, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, color, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 14, LV_PART_MAIN);
    return card;
}

static lv_obj_t *create_bar(lv_obj_t *screen, lv_align_t align, int y)
{
    lv_obj_t *bar = lv_obj_create(screen);
    lv_obj_set_size(bar, 760, 50);
    lv_obj_align(bar, align, 0, y);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x08131F), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 10, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
    return bar;
}

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

static void build_dashboard_screen(void)
{
    s_dashboard_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(s_dashboard_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_dashboard_screen, ST_COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_dashboard_screen, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *top = create_bar(s_dashboard_screen, LV_ALIGN_TOP_MID, 8);
    create_label(top, "SmartTank HMI", ST_COLOR_TEXT, LV_ALIGN_LEFT_MID, 12, 0);
    create_label(top, "12:30   09.07.2026", ST_COLOR_TEXT, LV_ALIGN_CENTER, 0, 0);
    create_label(top, "WiFi   MQTT   24.1 V", ST_COLOR_ACCENT, LV_ALIGN_RIGHT_MID, -12, 0);

    lv_obj_t *tank_card = create_card(s_dashboard_screen, lv_color_hex(0x39D12F));
    lv_obj_align(tank_card, LV_ALIGN_TOP_LEFT, 20, 68);
    tank_widget_t tank = tank_widget_create(tank_card);
    tank_widget_set_data(&tank, 72, 7.56f, 10.50f);

    lv_obj_t *well_card = create_card(s_dashboard_screen, lv_color_hex(0x2EA8FF));
    lv_obj_align(well_card, LV_ALIGN_TOP_MID, 0, 68);
    well_widget_t well = well_widget_create(well_card);
    well_widget_set_data(&well, 2.81f, 4.00f);

    lv_obj_t *weather_card = create_card(s_dashboard_screen, lv_color_hex(0xFFC247));
    lv_obj_align(weather_card, LV_ALIGN_TOP_RIGHT, -20, 68);
    weather_widget_t weather = weather_widget_create(weather_card);
    weather_widget_set_current(&weather, 18.6f, 10, 12.0f, 62, "Zachmurzenie");

    lv_obj_t *bottom = create_bar(s_dashboard_screen, LV_ALIGN_BOTTOM_MID, -8);
    bottom_nav_create(bottom, NAV_DASHBOARD, dashboard_nav_change);
}

void screen_dashboard_create(void)
{
    if (s_dashboard_screen == NULL) {
        build_dashboard_screen();
    }

    lv_scr_load(s_dashboard_screen);
}
