idf_component_register(
    SRCS
        "main.c"
        "lvgl_port.c"
        "waveshare_rgb_lcd_port.c"

        "app/app.c"

        "model/app_model.c"
        "model/measurement_history.c"
        "services/data_simulator.c"

        "screens/screen_boot.c"
        "screens/screen_dashboard.c"
        "screens/screen_tank_detail.c"
        "screens/screen_tank_calibration.c"
        "screens/screen_history.c"
        "screens/screen_display_test.c"

        "ui/theme.c"

        "widgets/widget_common.c"
        "widgets/tank_widget.c"
        "widgets/well_widget.c"
        "widgets/weather_widget.c"
        "widgets/bottom_nav.c"

    INCLUDE_DIRS
        "."
        "app"
        "model"
        "services"
        "screens"
        "ui"
        "widgets"
)
