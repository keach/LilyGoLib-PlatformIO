#include "gotify_store.h"

#include <Preferences.h>

void GotifyStore::load(bool &initialized, uint64_t &last_message_id) const
{
    Preferences preferences;
    if (!preferences.begin("gotify", true)) return;
    initialized = preferences.getBool("initialized", false);
    last_message_id = preferences.getULong64("last_id", 0);
    preferences.end();
}

bool GotifyStore::save(bool initialized, uint64_t last_message_id) const
{
    Preferences preferences;
    if (!preferences.begin("gotify", false)) return false;
    const bool initialized_saved =
        preferences.putBool("initialized", initialized) > 0;
    const bool id_saved = preferences.putULong64("last_id", last_message_id) > 0;
    preferences.end();
    return initialized_saved && id_saved;
}
