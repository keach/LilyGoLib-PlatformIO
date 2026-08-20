#include "notification_settings_store.h"

#include <Preferences.h>

namespace {

constexpr const char *kPreferencesNamespace = "notifications";

}  // namespace

NotificationMode NotificationSettingsStore::loadMode(
    NotificationTarget target) const
{
    Preferences preferences;
    if (!preferences.begin(kPreferencesNamespace, true)) {
        return NotificationMode::SoundAndVibration;
    }

    const uint8_t stored = preferences.getUChar(
        modeKeyForTarget(target),
        static_cast<uint8_t>(NotificationMode::SoundAndVibration));
    preferences.end();
    return isValidNotificationMode(stored)
               ? static_cast<NotificationMode>(stored)
               : NotificationMode::SoundAndVibration;
}

bool NotificationSettingsStore::saveMode(NotificationTarget target,
                                         NotificationMode mode) const
{
    Preferences preferences;
    if (!preferences.begin(kPreferencesNamespace, false)) {
        return false;
    }
    const bool saved = preferences.putUChar(
        modeKeyForTarget(target), static_cast<uint8_t>(mode)) > 0;
    preferences.end();
    return saved;
}

NotificationSoundPreset NotificationSettingsStore::loadSoundPreset(
    NotificationTarget target) const
{
    Preferences preferences;
    if (!preferences.begin(kPreferencesNamespace, true)) {
        return kDefaultNotificationSoundPreset;
    }

    const uint8_t stored = preferences.getUChar(
        soundKeyForTarget(target),
        static_cast<uint8_t>(kDefaultNotificationSoundPreset));
    preferences.end();
    return resolveNotificationSoundPreset(stored);
}

bool NotificationSettingsStore::saveSoundPreset(
    NotificationTarget target, NotificationSoundPreset preset) const
{
    Preferences preferences;
    if (!preferences.begin(kPreferencesNamespace, false)) {
        return false;
    }
    const bool saved = preferences.putUChar(
        soundKeyForTarget(target),
        static_cast<uint8_t>(resolveNotificationSoundPreset(
            static_cast<uint8_t>(preset)))) > 0;
    preferences.end();
    return saved;
}

const char *NotificationSettingsStore::modeKeyForTarget(
    NotificationTarget target)
{
    switch (target) {
    case NotificationTarget::KitchenTimer:
        return "kitchen_mode";
    case NotificationTarget::PomodoroTimer:
        return "pomodoro_mode";
    case NotificationTarget::ScheduledAlarm:
        return "alarm_mode";
    }
    return "kitchen_mode";
}

const char *NotificationSettingsStore::soundKeyForTarget(
    NotificationTarget target)
{
    switch (target) {
    case NotificationTarget::KitchenTimer:
        return "kitchen_sound";
    case NotificationTarget::PomodoroTimer:
        return "pomodoro_sound";
    case NotificationTarget::ScheduledAlarm:
        return "alarm_sound";
    }
    return "kitchen_sound";
}
