#pragma once

#include <stddef.h>
#include <stdint.h>

constexpr size_t kGotifyMaximumMessages = 8;
constexpr size_t kGotifyTitleLength = 64;
constexpr size_t kGotifyBodyLength = 256;
constexpr size_t kGotifyDateLength = 32;

struct GotifyMessage {
    uint64_t id = 0;
    int16_t priority = 0;
    char title[kGotifyTitleLength + 1] = {};
    char body[kGotifyBodyLength + 1] = {};
    char date[kGotifyDateLength + 1] = {};
};

void sortGotifyMessagesOldestFirst(GotifyMessage *messages, size_t count);
size_t filterNewGotifyMessages(GotifyMessage *messages, size_t count,
                               uint64_t last_message_id);
bool gotifyAutomaticCheckDue(uint32_t now_ms, uint32_t last_started_ms,
                             bool has_started);
