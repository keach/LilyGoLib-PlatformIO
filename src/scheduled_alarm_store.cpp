#include "scheduled_alarm_store.h"

#include <Preferences.h>

namespace {

constexpr const char *kNamespace = "scheduled_alarm";

}  // namespace

bool ScheduledAlarmStore::load(ScheduledAlarm &alarm) const
{
    Preferences preferences;
    if (!preferences.begin(kNamespace, true)) {
        alarm.restore(7, 0, false, 0);
        return false;
    }
    const uint8_t hour = preferences.getUChar("hour", 7);
    const uint8_t minute = preferences.getUChar("minute", 0);
    const bool enabled = preferences.getBool("enabled", false);
    const time_t trigger = static_cast<time_t>(
        preferences.getULong64("trigger", 0));
    preferences.end();
    if (alarm.restore(hour, minute, enabled, trigger)) {
        return true;
    }
    alarm.restore(7, 0, false, 0);
    return false;
}

bool ScheduledAlarmStore::save(const ScheduledAlarm &alarm) const
{
    Preferences preferences;
    if (!preferences.begin(kNamespace, false)) {
        return false;
    }
    const bool hour_saved = preferences.putUChar("hour", alarm.hour()) > 0;
    const bool minute_saved =
        preferences.putUChar("minute", alarm.minute()) > 0;
    const bool enabled_saved =
        preferences.putBool("enabled", alarm.enabled()) > 0;
    const bool trigger_saved = preferences.putULong64(
        "trigger", static_cast<uint64_t>(alarm.triggerEpoch())) > 0;
    preferences.end();
    return hour_saved && minute_saved && enabled_saved && trigger_saved;
}
