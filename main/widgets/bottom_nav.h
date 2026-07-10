#pragma once

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

typedef struct {
    lv_obj_t *root;
    lv_obj_t *buttons[NAV_ITEM_COUNT];
    lv_obj_t *labels[NAV_ITEM_COUNT];
    bottom_nav_page_t active_page;
} bottom_nav_t;

bottom_nav_t bottom_nav_create(
    lv_obj_t *parent,
    bottom_nav_page_t active_page
);

void bottom_nav_set_active(
    bottom_nav_t *nav,
    bottom_nav_page_t active_page
);