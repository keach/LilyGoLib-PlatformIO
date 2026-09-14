#include "gotify_fetcher.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <string.h>

namespace {
extern const uint8_t x509_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t x509_crt_bundle_end[] asm("_binary_x509_crt_bundle_end");

template <size_t N>
void copyText(char (&output)[N], const char *input)
{
    if (input == nullptr) input = "";
    strncpy(output, input, N - 1);
    output[N - 1] = '\0';
}
}

GotifyFetchResult fetchGotifyMessages(const char *server_url,
                                      const char *client_token,
                                      uint64_t last_message_id,
                                      bool initialize_only)
{
    GotifyFetchResult result;
    if (server_url == nullptr || client_token == nullptr ||
        strncmp(server_url, "https://", 8) != 0 || client_token[0] == '\0' ||
        strncmp(client_token, "YOUR_", 5) == 0) {
        result.error = GotifyFetchError::InvalidConfiguration;
        return result;
    }
    String url(server_url);
    while (url.endsWith("/")) url.remove(url.length() - 1);
    url += "/message?limit=";
    url += initialize_only ? "1" : "8";

    WiFiClientSecure client;
    client.setCACertBundle(x509_crt_bundle_start,
                           x509_crt_bundle_end - x509_crt_bundle_start);
    HTTPClient request;
    request.setConnectTimeout(10000);
    request.setTimeout(15000);
    if (!request.begin(client, url)) {
        result.error = GotifyFetchError::Transport;
        return result;
    }
    request.addHeader("X-Gotify-Key", client_token);
    const int status = request.GET();
    result.http_status = status > 0 ? static_cast<int16_t>(status) : 0;
    if (status < 0) {
        request.end();
        result.error = GotifyFetchError::Transport;
        return result;
    }
    if (status == HTTP_CODE_UNAUTHORIZED || status == HTTP_CODE_FORBIDDEN) {
        request.end();
        result.error = GotifyFetchError::Authentication;
        return result;
    }
    if (status != HTTP_CODE_OK) {
        request.end();
        result.error = GotifyFetchError::Http;
        return result;
    }
    JsonDocument document;
    const DeserializationError json_error = deserializeJson(document,
        request.getStream());
    request.end();
    if (json_error) {
        result.error = GotifyFetchError::Json;
        return result;
    }
    JsonArrayConst messages = document["messages"].as<JsonArrayConst>();
    if (messages.isNull()) {
        result.error = GotifyFetchError::Response;
        return result;
    }
    for (JsonObjectConst item : messages) {
        if (result.message_count >= kGotifyMaximumMessages) break;
        JsonVariantConst id = item["id"];
        JsonVariantConst priority = item["priority"];
        JsonVariantConst message = item["message"];
        JsonVariantConst date = item["date"];
        if (!id.is<uint64_t>() || id.as<uint64_t>() == 0 ||
            !priority.is<int>() || !message.is<const char *>() ||
            !date.is<const char *>()) {
            result.error = GotifyFetchError::Response;
            return result;
        }
        GotifyMessage &output = result.messages[result.message_count++];
        output.id = id.as<uint64_t>();
        output.priority = static_cast<int16_t>(priority.as<int>());
        copyText(output.title, item["title"] | "Gotify");
        copyText(output.body, message.as<const char *>());
        copyText(output.date, date.as<const char *>());
    }
    result.message_count = filterNewGotifyMessages(
        result.messages, result.message_count, last_message_id);
    result.success = true;
    return result;
}

const char *gotifyFetchErrorText(GotifyFetchError error)
{
    switch (error) {
    case GotifyFetchError::InvalidConfiguration: return "CHECK CONFIG";
    case GotifyFetchError::Transport: return "NETWORK FAILED";
    case GotifyFetchError::Http: return "HTTP FAILED";
    case GotifyFetchError::Authentication: return "TOKEN REJECTED";
    case GotifyFetchError::Json: return "JSON DATA ERROR";
    case GotifyFetchError::Response: return "MESSAGE DATA ERROR";
    default: return "CHECK FAILED";
    }
}
