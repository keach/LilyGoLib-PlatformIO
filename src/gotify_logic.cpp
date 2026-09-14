#include "gotify_logic.h"

#include <string.h>

namespace {
constexpr uint32_t kAutomaticCheckIntervalMs = 5U * 60U * 1000U;
}

void sortGotifyMessagesOldestFirst(GotifyMessage *messages, size_t count)
{
    if (messages == nullptr) return;
    for (size_t i = 1; i < count; ++i) {
        GotifyMessage value = messages[i];
        size_t j = i;
        while (j > 0 && messages[j - 1].id > value.id) {
            messages[j] = messages[j - 1];
            --j;
        }
        messages[j] = value;
    }
}

size_t filterNewGotifyMessages(GotifyMessage *messages, size_t count,
                               uint64_t last_message_id)
{
    if (messages == nullptr) return 0;
    size_t output = 0;
    for (size_t i = 0; i < count; ++i) {
        if (messages[i].id > last_message_id) messages[output++] = messages[i];
    }
    sortGotifyMessagesOldestFirst(messages, output);
    return output;
}

bool gotifyAutomaticCheckDue(uint32_t now_ms, uint32_t last_started_ms,
                             bool has_started)
{
    return !has_started || now_ms - last_started_ms >= kAutomaticCheckIntervalMs;
}
