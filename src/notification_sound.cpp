#include "notification_sound.h"

size_t notificationSoundPresetCount()
{
    return static_cast<size_t>(NotificationSoundPreset::Count);
}

bool isValidNotificationSoundPreset(uint8_t value)
{
    return value < static_cast<uint8_t>(NotificationSoundPreset::Count);
}

NotificationSoundPreset resolveNotificationSoundPreset(uint8_t value)
{
    return isValidNotificationSoundPreset(value)
               ? static_cast<NotificationSoundPreset>(value)
               : kDefaultNotificationSoundPreset;
}

const char *notificationSoundPresetLabel(NotificationSoundPreset preset)
{
    switch (resolveNotificationSoundPreset(static_cast<uint8_t>(preset))) {
    case NotificationSoundPreset::Classic:
        return "CLASSIC";
    case NotificationSoundPreset::DoubleBeep:
        return "DOUBLE BEEP";
    case NotificationSoundPreset::Ascending:
        return "ASCENDING";
    case NotificationSoundPreset::Count:
        break;
    }
    return "CLASSIC";
}

NotificationSoundPreset previousNotificationSoundPreset(
    NotificationSoundPreset preset)
{
    const uint8_t value = static_cast<uint8_t>(
        resolveNotificationSoundPreset(static_cast<uint8_t>(preset)));
    const uint8_t count = static_cast<uint8_t>(
        NotificationSoundPreset::Count);
    return static_cast<NotificationSoundPreset>((value + count - 1) % count);
}

NotificationSoundPreset nextNotificationSoundPreset(
    NotificationSoundPreset preset)
{
    const uint8_t value = static_cast<uint8_t>(
        resolveNotificationSoundPreset(static_cast<uint8_t>(preset)));
    const uint8_t count = static_cast<uint8_t>(
        NotificationSoundPreset::Count);
    return static_cast<NotificationSoundPreset>((value + 1) % count);
}

uint16_t notificationSoundFrequencyAt(NotificationSoundPreset preset,
                                      uint32_t elapsed_ms)
{
    if (elapsed_ms >= kNotificationSoundPatternDurationMs) {
        return 0;
    }

    switch (resolveNotificationSoundPreset(static_cast<uint8_t>(preset))) {
    case NotificationSoundPreset::Classic:
        return 1000;
    case NotificationSoundPreset::DoubleBeep:
        if (elapsed_ms < 180 ||
            (elapsed_ms >= 300 && elapsed_ms < 480)) {
            return 880;
        }
        return 0;
    case NotificationSoundPreset::Ascending:
        if (elapsed_ms < 250) {
            return 659;
        }
        if (elapsed_ms < 500) {
            return 784;
        }
        if (elapsed_ms < 800) {
            return 1047;
        }
        return 0;
    case NotificationSoundPreset::Count:
        break;
    }
    return 1000;
}
