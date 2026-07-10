idf_component_register(
    SRCS
        "main.c"
        "lvgl_port.c"
        "waveshare_rgb_lcd_port.c"

        "app/app.c"

        "screens/screen_boot.c"
        "screens/screen_dashboard.c"
        "screens/screen_history.c"

        "ui/theme.c"

        "widgets/widget_common.c"
        "widgets/tank_widget.c"
        "widgets/well_widget.c"
        "widgets/weather_widget.c"
        "widgets/bottom_nav.c"

    INCLUDE_DIRS
        "."
        "app"
        "screens"
        "ui"
        "widgets"
)
