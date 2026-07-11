idf_component_register(
    SRCS
        "main.c"
        "lvgl_port.c"
        "waveshare_rgb_lcd_port.c"

        "app/app.c"

        "drivers/rs485_port.c"
        "drivers/rtc_pcf85063.c"

        "model/app_model.c"
        "model/measurement_history.c"

        "protocol/modbus_rtu_client.c"

        "services/clock_service.c"
        "services/data_simulator.c"
        "services/ntp_service.c"
        "services/settings_storage.c"
        "services/weather_geocoding.c"
        "services/weather_location_storage.c"
        "services/wifi_service.c"

        "screens/screen_boot.c"
        "screens/screen_dashboard.c"
        "screens/screen_tank_detail.c"
        "screens/screen_tank_calibration.c"
        "screens/screen_service.c"
        "screens/screen_settings.c"
        "screens/screen_settings_weather.c"
        "screens/screen_weather_location.c"
        "screens/screen_history.c"
        "screens/screen_display_test.c"

        "ui/panel_icons.c"
        "ui/theme.c"

        "widgets/widget_common.c"
        "widgets/tank_widget.c"
        "widgets/well_widget.c"
        "widgets/weather_widget.c"
        "widgets/bottom_nav.c"

    INCLUDE_DIRS
        "."
        "app"
        "drivers"
        "model"
        "protocol"
        "services"
        "screens"
        "ui"
        "widgets"

    PRIV_REQUIRES
        driver
        esp_event
        esp_http_client
        esp_netif
        esp_wifi
        json
        lwip
        mbedtls
        nvs_flash
)
