#include "app.h"
#include "lvgl_port.h"
#include "theme.h"
#include "screen_dashboard.h"
#include "screen_history.h"

void app_start(void)
{
    if (lvgl_port_lock(-1)) {
        theme_init();

        /*
         * Budujemy oba ekrany podczas startu, zanim LVGL narysuje pierwsza klatke.
         * Pozniejsze przelaczanie tylko podmienia wskaznik aktywnego ekranu.
         */
        screen_history_create();
        screen_dashboard_create();

        lvgl_port_unlock();
    }
}
