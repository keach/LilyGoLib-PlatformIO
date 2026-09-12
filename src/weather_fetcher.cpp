#include "weather_fetcher.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <math.h>

namespace {
constexpr size_t kMaximumForecastPoints = 40;
constexpr double kMinimumTemperature = -100.0;
constexpr double kMaximumTemperature = 100.0;
constexpr int32_t kMaximumTimezoneOffsetSeconds = 18 * 60 * 60;
extern const uint8_t x509_crt_bundle_start[]
    asm("_binary_x509_crt_bundle_start");
extern const uint8_t x509_crt_bundle_end[]
    asm("_binary_x509_crt_bundle_end");

int16_t temperatureTenths(double value)
{
    return static_cast<int16_t>(lround(value * 10.0));
}

String endpoint(const char *path, const char *api_key,
                const char *latitude, const char *longitude)
{
    String url("https://api.openweathermap.org/data/2.5/");
    url += path;
    url += "?lat=";
    url += latitude;
    url += "&lon=";
    url += longitude;
    url += "&appid=";
    url += api_key;
    url += "&units=metric&lang=ja";
    return url;
}

struct JsonRequestResult {
    WeatherFetchError error = WeatherFetchError::None;
    int16_t http_status = 0;

    bool succeeded() const { return error == WeatherFetchError::None; }
};

JsonRequestResult getJson(const String &url, JsonDocument &document)
{
    WiFiClientSecure client;
    client.setCACertBundle(x509_crt_bundle_start,
                           x509_crt_bundle_end - x509_crt_bundle_start);
    HTTPClient request;
    request.setConnectTimeout(10000);
    request.setTimeout(15000);
    if (!request.begin(client, url)) {
        return {WeatherFetchError::Transport, 0};
    }
    const int status = request.GET();
    if (status < 0) {
        request.end();
        return {WeatherFetchError::Transport, 0};
    }
    if (status == HTTP_CODE_UNAUTHORIZED || status == HTTP_CODE_FORBIDDEN) {
        request.end();
        return {WeatherFetchError::Authentication,
                static_cast<int16_t>(status)};
    }
    if (status != HTTP_CODE_OK) {
        request.end();
        return {WeatherFetchError::Http, static_cast<int16_t>(status)};
    }
    const DeserializationError error = deserializeJson(
        document, request.getStream());
    request.end();
    if (error) {
        return {WeatherFetchError::Json, static_cast<int16_t>(status)};
    }
    return {WeatherFetchError::None, static_cast<int16_t>(status)};
}

bool validTemperature(double value)
{
    return isfinite(value) && value >= kMinimumTemperature &&
           value <= kMaximumTemperature;
}

bool validConditionId(int value)
{
    return value >= 100 && value <= 999;
}

bool parseForecastPoint(JsonObjectConst point, time_t epoch,
                        WeatherForecastPoint &output)
{
    const JsonVariantConst minimum_value = point["main"]["temp_min"];
    const JsonVariantConst maximum_value = point["main"]["temp_max"];
    const JsonVariantConst precipitation_value = point["pop"];
    const JsonVariantConst condition_value = point["weather"][0]["id"];
    if (!minimum_value.is<double>() || !maximum_value.is<double>() ||
        !precipitation_value.is<double>() || !condition_value.is<int>()) {
        return false;
    }
    const double minimum = minimum_value.as<double>();
    const double maximum = maximum_value.as<double>();
    const double precipitation = precipitation_value.as<double>();
    const int condition_id = condition_value.as<int>();
    if (!validTemperature(minimum) || !validTemperature(maximum) ||
        minimum > maximum ||
        !isfinite(precipitation) || precipitation < 0.0 ||
        precipitation > 1.0 || !validConditionId(condition_id)) {
        return false;
    }
    output.epoch = epoch;
    output.minimum_temperature_tenths = temperatureTenths(minimum);
    output.maximum_temperature_tenths = temperatureTenths(maximum);
    output.precipitation_percent = static_cast<uint8_t>(
        lround(precipitation * 100.0));
    output.condition_id = static_cast<int16_t>(condition_id);
    return true;
}
}

WeatherFetchResult fetchOpenWeather(const char *api_key,
                                    const char *latitude,
                                    const char *longitude,
                                    time_t fetched_epoch)
{
    WeatherFetchResult result;
    if (api_key == nullptr || latitude == nullptr || longitude == nullptr ||
        api_key[0] == '\0' || latitude[0] == '\0' || longitude[0] == '\0') {
        result.error = WeatherFetchError::InvalidConfiguration;
        return result;
    }

    JsonDocument current;
    const JsonRequestResult current_request = getJson(
        endpoint("weather", api_key, latitude, longitude), current);
    if (!current_request.succeeded()) {
        result.error = current_request.error;
        result.http_status = current_request.http_status;
        return result;
    }
    result.http_status = current_request.http_status;
    const JsonVariantConst current_epoch_value = current["dt"];
    const JsonVariantConst timezone_value = current["timezone"];
    const JsonVariantConst temperature_value = current["main"]["temp"];
    const JsonVariantConst feels_like_value = current["main"]["feels_like"];
    const JsonVariantConst humidity_value = current["main"]["humidity"];
    const JsonVariantConst current_condition_value = current["weather"][0]["id"];
    if (!current_epoch_value.is<time_t>() || !timezone_value.is<int32_t>() ||
        !temperature_value.is<double>() || !feels_like_value.is<double>() ||
        !humidity_value.is<int>() || !current_condition_value.is<int>()) {
        result.error = WeatherFetchError::CurrentResponse;
        return result;
    }

    const time_t current_epoch = current_epoch_value.as<time_t>();
    const int32_t timezone_offset = timezone_value.as<int32_t>();
    const double temperature = temperature_value.as<double>();
    const double feels_like = feels_like_value.as<double>();
    const int humidity = humidity_value.as<int>();
    const int current_condition = current_condition_value.as<int>();
    if (current_epoch <= 0 ||
        timezone_offset < -kMaximumTimezoneOffsetSeconds ||
        timezone_offset > kMaximumTimezoneOffsetSeconds ||
        !validTemperature(temperature) || !validTemperature(feels_like) ||
        humidity < 0 || humidity > 100 ||
        !validConditionId(current_condition)) {
        result.error = WeatherFetchError::CurrentResponse;
        return result;
    }

    WeatherSnapshot snapshot;
    snapshot.updated_epoch = fetched_epoch;
    snapshot.current_epoch = current_epoch;
    snapshot.timezone_offset_seconds = timezone_offset;
    snapshot.current_temperature_tenths = temperatureTenths(temperature);
    snapshot.feels_like_temperature_tenths = temperatureTenths(feels_like);
    snapshot.humidity_percent = static_cast<uint8_t>(humidity);
    snapshot.current_condition_id = static_cast<int16_t>(current_condition);

    JsonDocument forecast;
    const JsonRequestResult forecast_request = getJson(
        endpoint("forecast", api_key, latitude, longitude), forecast);
    if (!forecast_request.succeeded()) {
        result.error = forecast_request.error;
        result.http_status = forecast_request.http_status;
        return result;
    }
    result.http_status = forecast_request.http_status;
    const JsonArray list = forecast["list"].as<JsonArray>();
    if (list.isNull() || list.size() == 0) {
        result.error = WeatherFetchError::ForecastResponse;
        return result;
    }
    const JsonVariantConst forecast_timezone = forecast["city"]["timezone"];
    if (!forecast_timezone.isNull()) {
        if (!forecast_timezone.is<int32_t>()) {
            result.error = WeatherFetchError::ForecastResponse;
            return result;
        }
        const int32_t offset = forecast_timezone.as<int32_t>();
        if (offset < -kMaximumTimezoneOffsetSeconds ||
            offset > kMaximumTimezoneOffsetSeconds) {
            result.error = WeatherFetchError::ForecastResponse;
            return result;
        }
        snapshot.timezone_offset_seconds = offset;
    }
    WeatherForecastPoint points[kMaximumForecastPoints] = {};
    size_t point_count = 0;
    bool today_complete = true;
    bool tomorrow_complete = true;
    for (JsonObjectConst point : list) {
        const JsonVariantConst epoch_value = point["dt"];
        if (!epoch_value.is<time_t>() || epoch_value.as<time_t>() <= 0) {
            result.error = WeatherFetchError::ForecastResponse;
            return result;
        }
        const time_t epoch = epoch_value.as<time_t>();
        const int8_t day_offset = weatherForecastDayOffset(
            epoch, snapshot.current_epoch, snapshot.timezone_offset_seconds);
        if (day_offset != 0 && day_offset != 1) continue;
        WeatherForecastPoint parsed;
        if (!parseForecastPoint(point, epoch, parsed)) {
            if (day_offset == 0) today_complete = false;
            if (day_offset == 1) tomorrow_complete = false;
            continue;
        }
        if (point_count < kMaximumForecastPoints) {
            points[point_count++] = parsed;
        }
    }
    aggregateWeatherForecast(points, point_count, snapshot.current_epoch,
                             snapshot.timezone_offset_seconds,
                             snapshot.today, snapshot.tomorrow);
    invalidateIncompleteWeatherDays(today_complete, tomorrow_complete,
                                    snapshot.today, snapshot.tomorrow);
    snapshot.valid = true;
    result.success = true;
    result.snapshot = snapshot;
    return result;
}

const char *weatherFetchErrorText(WeatherFetchError error)
{
    switch (error) {
    case WeatherFetchError::InvalidConfiguration: return "CHECK CONFIG";
    case WeatherFetchError::WiFi: return "WI-FI FAILED";
    case WeatherFetchError::Transport: return "NETWORK FAILED";
    case WeatherFetchError::Http: return "HTTP FAILED";
    case WeatherFetchError::Authentication: return "API KEY REJECTED";
    case WeatherFetchError::Json: return "JSON DATA ERROR";
    case WeatherFetchError::CurrentResponse: return "CURRENT DATA ERROR";
    case WeatherFetchError::ForecastResponse: return "FORECAST DATA ERROR";
    default: return "UPDATE FAILED";
    }
}
