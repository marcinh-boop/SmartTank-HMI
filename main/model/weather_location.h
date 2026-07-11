#pragma once

#include <stdbool.h>

#define WEATHER_LOCATION_NAME_MAX_LEN      63U
#define WEATHER_LOCATION_ADMIN_MAX_LEN     47U
#define WEATHER_LOCATION_COUNTRY_MAX_LEN   47U
#define WEATHER_LOCATION_TIMEZONE_MAX_LEN  39U

typedef struct {
    char name[WEATHER_LOCATION_NAME_MAX_LEN + 1U];
    char admin1[WEATHER_LOCATION_ADMIN_MAX_LEN + 1U];
    char country[WEATHER_LOCATION_COUNTRY_MAX_LEN + 1U];
    char timezone[WEATHER_LOCATION_TIMEZONE_MAX_LEN + 1U];
    double latitude;
    double longitude;
} weather_location_t;

static inline bool weather_location_is_valid(const weather_location_t *location)
{
    return location != NULL &&
           location->name[0] != '\0' &&
           location->latitude >= -90.0 &&
           location->latitude <= 90.0 &&
           location->longitude >= -180.0 &&
           location->longitude <= 180.0;
}
