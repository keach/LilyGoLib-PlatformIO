#pragma once

#include "weather_config_types.h"

// Copy this file to weather_config.h and fill in your OpenWeather settings.
// weather_config.h is intentionally excluded from Git.
inline constexpr WeatherConfig kWeatherConfig = {
    "YOUR_OPENWEATHER_API_KEY",
    "TOKYO",
    "35.6812",
    "139.7671",
};
