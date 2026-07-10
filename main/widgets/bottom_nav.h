#pragma once

#include <stdbool.h>
#include "lvgl.h"

typedef enum {
    NAV_DASHBOARD = 0,
    NAV_HISTORY,
    NAV_ALARMS,
    NAV_SETTINGS,
    NAV_SERVICE,
    NAV_INFO,
    NAV_ITEM_COUNT
} bottom_nav_page_t;

typedef bool (*bottom_nav_change_cb_t)(bottom_nav_page_t page);

typedef struct bottom_nav bottom_nav_t;

typedef struct {
    bottom_nav_t *nav;
    bottom_nav_page_t page;
} bottom_nav_button_ctx_t;

struct bottom_nav {
    lv_obj_t *root;
    lv_obj_t *buttons[NAV_ITEM_COUNT];
    lv_obj_t *labels[NAV_ITEM_COUNT];
    bottom_nav_button_ctx_t contexts[NAV_ITEM_COUNT];
    bottom_nav_page_t active_page;
    bottom_nav_change_cb_t change_cb;
};

bottom_nav_t *bottom_nav_create(
    lv_obj_t *parent,
    bottom_nav_page_t active_page,
    bottom_nav_change_cb_t change_cb
);

void bottom_nav_set_active(
    bottom_nav_t *nav,
    bottom_nav_page_t active_page
);
