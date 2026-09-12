#pragma once

#include <stddef.h>
#include <stdint.h>
#include <time.h>

struct DailyWeather {
    bool valid = false;
    int16_t minimum_temperature_tenths = 0;
    int16_t maximum_temperature_tenths = 0;
    uint8_t maximum_precipitation_percent = 0;
    int16_t condition_id = 0;
};

struct WeatherSnapshot {
    bool valid = false;
    time_t updated_epoch = 0;
    time_t current_epoch = 0;
    int32_t timezone_offset_seconds = 0;
    int16_t current_temperature_tenths = 0;
    int16_t feels_like_temperature_tenths = 0;
    uint8_t humidity_percent = 0;
    int16_t current_condition_id = 0;
    DailyWeather today = {};
    DailyWeather tomorrow = {};
};

struct WeatherForecastPoint {
    time_t epoch = 0;
    int16_t minimum_temperature_tenths = 0;
    int16_t maximum_temperature_tenths = 0;
    uint8_t precipitation_percent = 0;
    int16_t condition_id = 0;
};

const char *weatherConditionJapanese(int condition_id);
bool aggregateWeatherForecast(const WeatherForecastPoint *points,
                              size_t point_count,
                              time_t current_epoch,
                              int32_t timezone_offset_seconds,
                              DailyWeather &today,
                              DailyWeather &tomorrow);
bool weatherRefreshDue(const WeatherSnapshot &snapshot, time_t now);
bool weatherCacheStale(const WeatherSnapshot &snapshot, time_t now);

