#include "weather_logic.h"

#include <limits.h>

namespace {
constexpr time_t kAutomaticRefreshSeconds = 12 * 60 * 60;
constexpr time_t kStaleSeconds = 24 * 60 * 60;
constexpr int64_t kSecondsPerDay = 24 * 60 * 60;
constexpr int64_t kNoonSeconds = 12 * 60 * 60;

int64_t floorDivision(int64_t value, int64_t divisor)
{
    int64_t quotient = value / divisor;
    const int64_t remainder = value % divisor;
    if (remainder != 0 && ((remainder < 0) != (divisor < 0))) {
        --quotient;
    }
    return quotient;
}

void aggregateDay(const WeatherForecastPoint *points,
                  size_t point_count,
                  time_t current_epoch,
                  int32_t timezone_offset_seconds,
                  int64_t target_day,
                  bool discard_past,
                  DailyWeather &output)
{
    int32_t closest_noon_distance = INT32_MAX;
    for (size_t index = 0; index < point_count; ++index) {
        const auto &point = points[index];
        const int64_t local_epoch = static_cast<int64_t>(point.epoch) +
                                    timezone_offset_seconds;
        if (floorDivision(local_epoch, kSecondsPerDay) != target_day ||
            (discard_past && point.epoch < current_epoch)) {
            continue;
        }
        const int32_t seconds_of_day = static_cast<int32_t>(
            local_epoch - target_day * kSecondsPerDay);
        const int32_t noon_distance =
            seconds_of_day >= kNoonSeconds
                ? seconds_of_day - kNoonSeconds
                : kNoonSeconds - seconds_of_day;
        if (!output.valid) {
            output.valid = true;
            output.minimum_temperature_tenths =
                point.minimum_temperature_tenths;
            output.maximum_temperature_tenths =
                point.maximum_temperature_tenths;
        } else {
            if (point.minimum_temperature_tenths <
                output.minimum_temperature_tenths) {
                output.minimum_temperature_tenths =
                    point.minimum_temperature_tenths;
            }
            if (point.maximum_temperature_tenths >
                output.maximum_temperature_tenths) {
                output.maximum_temperature_tenths =
                    point.maximum_temperature_tenths;
            }
        }
        if (point.precipitation_percent >
            output.maximum_precipitation_percent) {
            output.maximum_precipitation_percent =
                point.precipitation_percent;
        }
        if (noon_distance < closest_noon_distance) {
            closest_noon_distance = noon_distance;
            output.condition_id = point.condition_id;
        }
    }
}
}  // namespace

const char *weatherConditionJapanese(int condition_id)
{
    if (condition_id >= 200 && condition_id <= 299) return "雷雨";
    if (condition_id >= 300 && condition_id <= 399) return "霧雨";
    if (condition_id >= 500 && condition_id <= 599) return "雨";
    if (condition_id >= 600 && condition_id <= 699) return "雪";
    switch (condition_id) {
    case 701: case 741: return "霧";
    case 711: return "煙";
    case 721: return "かすみ";
    case 731: case 751: case 761: return "砂塵";
    case 762: return "降灰";
    case 771: return "強風";
    case 781: return "竜巻";
    case 800: return "晴れ";
    case 801: return "薄曇り";
    case 802: case 803: case 804: return "曇り";
    default: return "不明";
    }
}

bool aggregateWeatherForecast(const WeatherForecastPoint *points,
                              size_t point_count,
                              time_t current_epoch,
                              int32_t timezone_offset_seconds,
                              DailyWeather &today,
                              DailyWeather &tomorrow)
{
    today = {};
    tomorrow = {};
    if (points == nullptr || point_count == 0 || current_epoch <= 0) {
        return false;
    }
    const int64_t current_day = floorDivision(
        static_cast<int64_t>(current_epoch) + timezone_offset_seconds,
        kSecondsPerDay);
    aggregateDay(points, point_count, current_epoch, timezone_offset_seconds,
                 current_day, true, today);
    aggregateDay(points, point_count, current_epoch, timezone_offset_seconds,
                 current_day + 1, false, tomorrow);
    return today.valid || tomorrow.valid;
}

bool weatherRefreshDue(const WeatherSnapshot &snapshot, time_t now)
{
    return !snapshot.valid || snapshot.updated_epoch <= 0 ||
           now < snapshot.updated_epoch ||
           now - snapshot.updated_epoch >= kAutomaticRefreshSeconds;
}

bool weatherCacheStale(const WeatherSnapshot &snapshot, time_t now)
{
    return !snapshot.valid || snapshot.updated_epoch <= 0 ||
           now < snapshot.updated_epoch ||
           now - snapshot.updated_epoch > kStaleSeconds;
}
