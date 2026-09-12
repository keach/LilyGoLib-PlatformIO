#include "weather_store.h"

#include <Preferences.h>

namespace {
constexpr const char *kNamespace = "weather";
constexpr const char *kSnapshotKey = "snapshot";
constexpr uint16_t kVersion = 1;

struct StoredWeatherSnapshot {
    uint16_t version;
    WeatherSnapshot snapshot;
};
}

bool WeatherStore::load(WeatherSnapshot &snapshot) const
{
    Preferences preferences;
    if (!preferences.begin(kNamespace, true)) return false;
    StoredWeatherSnapshot stored = {};
    const size_t bytes = preferences.getBytes(
        kSnapshotKey, &stored, sizeof(stored));
    preferences.end();
    if (bytes != sizeof(stored) || stored.version != kVersion ||
        !stored.snapshot.valid) {
        return false;
    }
    snapshot = stored.snapshot;
    return true;
}

bool WeatherStore::save(const WeatherSnapshot &snapshot) const
{
    Preferences preferences;
    if (!preferences.begin(kNamespace, false)) return false;
    const StoredWeatherSnapshot stored = {kVersion, snapshot};
    const bool saved = preferences.putBytes(
        kSnapshotKey, &stored, sizeof(stored)) == sizeof(stored);
    preferences.end();
    return saved;
}
