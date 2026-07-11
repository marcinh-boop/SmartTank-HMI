#include "weather_widget.h"

#include "app_model.h"

void weather_widget_set_current(
    weather_widget_t *widget,
    float temperature_c,
    int rain_percent,
    float wind_kmh,
    int humidity_percent,
    const char *description)
{
    (void)temperature_c;
    (void)rain_percent;
    (void)wind_kmh;
    (void)humidity_percent;
    (void)description;

    smarttank_state_t state;
    app_model_get_snapshot(&state);
    weather_widget_set_data(widget, &state.weather);
}
