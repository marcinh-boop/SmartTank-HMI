idf_component_register(
    SRCS
        "main.c"
        "lvgl_port.c"
        "waveshare_rgb_lcd_port.c"

        "app/app.c"

        "drivers/rs485_port.c"
        "drivers/rtc_pcf85063.c"
        "drivers/waveshare_analog_8ch.c"

        "model/app_model.c"
        "model/measurement_history.c"

        "protocol/modbus_rtu_client.c"

        "services/alarm_service.c"
        "services/analog_module_service.c"
        "services/clock_service.c"
        "services/data_simulator.c"
        "services/ntp_service.c"
        "services/settings_storage.c"
        "services/weather_geocoding.c"
        "services/weather_location_storage.c"
        "services/weather_service.c"
        "services/wifi_service.c"

        "screens/screen_alarms.c"
        "screens/screen_boot.c"
        "screens/screen_dashboard.c"
        "screens/screen_info.c"
        "screens/screen_tank_detail.c"
        "screens/screen_tank_calibration.c"
        "screens/screen_well_detail.c"
        "screens/well_detail_integration.c"
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
        "widgets/weather_widget_compat.c"
        "widgets/bottom_nav.c"
        "widgets/top_status_bar.c"

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
        app_update
        driver
        esp-tls
        esp_app_format
        esp_event
        esp_http_client
        esp_netif
        esp_wifi
        json
        lwip
        mbedtls
        nvs_flash
        spi_flash
)
