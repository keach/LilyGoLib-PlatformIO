#pragma once

#include "weather_logic.h"

enum class WeatherFetchError : uint8_t {
    None,
    InvalidConfiguration,
    WiFi,
    Transport,
    Http,
    Authentication,
    Json,
    CurrentResponse,
    ForecastResponse,
};

struct WeatherFetchResult {
    bool success = false;
    WeatherFetchError error = WeatherFetchError::None;
    int16_t http_status = 0;
    WeatherSnapshot snapshot = {};
};

WeatherFetchResult fetchOpenWeather(const char *api_key,
                                    const char *latitude,
                                    const char *longitude,
                                    time_t fetched_epoch);
const char *weatherFetchErrorText(WeatherFetchError error);
