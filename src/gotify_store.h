#pragma once

#include <stdint.h>

class GotifyStore {
public:
    void load(bool &initialized, uint64_t &last_message_id) const;
    bool save(bool initialized, uint64_t last_message_id) const;
};
