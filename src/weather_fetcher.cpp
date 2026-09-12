#include "weather_fetcher.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <math.h>

namespace {
constexpr size_t kMaximumForecastPoints = 40;
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

bool getJson(const String &url, JsonDocument &document)
{
    WiFiClientSecure client;
    client.setCACertBundle(x509_crt_bundle_start,
                           x509_crt_bundle_end - x509_crt_bundle_start);
    HTTPClient request;
    request.setConnectTimeout(10000);
    request.setTimeout(15000);
    if (!request.begin(client, url)) return false;
    const int status = request.GET();
    if (status != HTTP_CODE_OK) {
        request.end();
        return false;
    }
    const DeserializationError error = deserializeJson(
        document, request.getStream());
    request.end();
    return !error;
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
    if (!getJson(endpoint("weather", api_key, latitude, longitude), current)) {
        result.error = WeatherFetchError::CurrentRequest;
        return result;
    }
    if (!current["dt"].is<time_t>() || !current["main"]["temp"].is<double>() ||
        !current["weather"][0]["id"].is<int>()) {
        result.error = WeatherFetchError::CurrentResponse;
        return result;
    }

    WeatherSnapshot snapshot;
    snapshot.updated_epoch = fetched_epoch;
    snapshot.current_epoch = current["dt"].as<time_t>();
    snapshot.timezone_offset_seconds = current["timezone"] | 0;
    snapshot.current_temperature_tenths = temperatureTenths(
        current["main"]["temp"].as<double>());
    snapshot.feels_like_temperature_tenths = temperatureTenths(
        current["main"]["feels_like"].as<double>());
    snapshot.humidity_percent = current["main"]["humidity"] | 0;
    snapshot.current_condition_id = current["weather"][0]["id"] | 0;

    JsonDocument forecast;
    if (!getJson(endpoint("forecast", api_key, latitude, longitude), forecast)) {
        result.error = WeatherFetchError::ForecastRequest;
        return result;
    }
    const JsonArray list = forecast["list"].as<JsonArray>();
    if (list.isNull() || list.size() == 0) {
        result.error = WeatherFetchError::ForecastResponse;
        return result;
    }
    snapshot.timezone_offset_seconds =
        forecast["city"]["timezone"] | snapshot.timezone_offset_seconds;
    WeatherForecastPoint points[kMaximumForecastPoints] = {};
    size_t point_count = 0;
    for (JsonObject point : list) {
        if (point_count >= kMaximumForecastPoints) break;
        auto &output = points[point_count++];
        output.epoch = point["dt"] | 0;
        output.minimum_temperature_tenths = temperatureTenths(
            point["main"]["temp_min"] | 0.0);
        output.maximum_temperature_tenths = temperatureTenths(
            point["main"]["temp_max"] | 0.0);
        const double probability = point["pop"] | 0.0;
        output.precipitation_percent = static_cast<uint8_t>(
            lround(probability * 100.0));
        output.condition_id = point["weather"][0]["id"] | 0;
    }
    if (!aggregateWeatherForecast(points, point_count, snapshot.current_epoch,
                                  snapshot.timezone_offset_seconds,
                                  snapshot.today, snapshot.tomorrow)) {
        result.error = WeatherFetchError::ForecastResponse;
        return result;
    }
    snapshot.valid = true;
    result.success = true;
    result.snapshot = snapshot;
    return result;
}

const char *weatherFetchErrorText(WeatherFetchError error)
{
    switch (error) {
    case WeatherFetchError::InvalidConfiguration: return "CHECK CONFIG";
    case WeatherFetchError::CurrentRequest: return "CURRENT FAILED";
    case WeatherFetchError::CurrentResponse: return "CURRENT DATA ERROR";
    case WeatherFetchError::ForecastRequest: return "FORECAST FAILED";
    case WeatherFetchError::ForecastResponse: return "FORECAST DATA ERROR";
    default: return "UPDATE FAILED";
    }
}
