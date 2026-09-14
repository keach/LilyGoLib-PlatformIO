#pragma once

#include "gotify_logic.h"

enum class GotifyFetchError : uint8_t {
    None, InvalidConfiguration, Transport, Http, Authentication, Json, Response,
};

struct GotifyFetchResult {
    bool success = false;
    GotifyFetchError error = GotifyFetchError::None;
    int16_t http_status = 0;
    size_t message_count = 0;
    GotifyMessage messages[kGotifyMaximumMessages] = {};
};

GotifyFetchResult fetchGotifyMessages(const char *server_url,
                                      const char *client_token,
                                      uint64_t last_message_id,
                                      bool initialize_only);
const char *gotifyFetchErrorText(GotifyFetchError error);
