#pragma once

#include "weather_logic.h"

enum class WeatherFetchError : uint8_t {
    None,
    InvalidConfiguration,
    CurrentRequest,
    CurrentResponse,
    ForecastRequest,
    ForecastResponse,
};

struct WeatherFetchResult {
    bool success = false;
    WeatherFetchError error = WeatherFetchError::None;
    WeatherSnapshot snapshot = {};
};

WeatherFetchResult fetchOpenWeather(const char *api_key,
                                    const char *latitude,
                                    const char *longitude,
                                    time_t fetched_epoch);
const char *weatherFetchErrorText(WeatherFetchError error);
