/*
 * Widżet top_status_bar.h: wielokrotny komponent LVGL używany przez ekrany do spójnej prezentacji danych.
 * Ten nagłówek określa publiczne typy i funkcje dostępne dla innych części programu.
 * Oddzielenie odpowiedzialności ułatwia testowanie, diagnostykę i późniejszą rozbudowę urządzenia.
 */
#pragma once

#include <stdbool.h>

#include "lvgl.h"

typedef enum {
    TOP_STATUS_MQTT_DISABLED = 0,
    TOP_STATUS_MQTT_CONNECTING,
    TOP_STATUS_MQTT_ONLINE,
    TOP_STATUS_MQTT_ERROR,
} top_status_mqtt_state_t;

bool top_status_bar_attach(lv_obj_t *screen);
void top_status_bar_set_mqtt_state(top_status_mqtt_state_t state);
void top_status_bar_set_page_title(const char *title);
void top_status_bar_clear_page_override(void);
