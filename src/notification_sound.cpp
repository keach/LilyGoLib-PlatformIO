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
    case NotificationSoundPreset::Success:
        return "SUCCESS";
    case NotificationSoundPreset::DoubleBeep:
        return "DOUBLE BEEP";
    case NotificationSoundPreset::Urgent:
        return "URGENT";
    case NotificationSoundPreset::Count:
        break;
    }
    return "SUCCESS";
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
    case NotificationSoundPreset::Success:
        if (elapsed_ms < 120) {
            return 1047;
        }
        if (elapsed_ms < 160) {
            return 0;
        }
        if (elapsed_ms < 280) {
            return 1319;
        }
        if (elapsed_ms < 320) {
            return 0;
        }
        if (elapsed_ms < 480) {
            return 1568;
        }
        if (elapsed_ms < 520) {
            return 0;
        }
        if (elapsed_ms < 640) {
            return 1319;
        }
        if (elapsed_ms < 680) {
            return 0;
        }
        return 2093;
    case NotificationSoundPreset::DoubleBeep:
        if (elapsed_ms < 180 ||
            (elapsed_ms >= 300 && elapsed_ms < 480)) {
            return 880;
        }
        return 0;
    case NotificationSoundPreset::Urgent:
        if (elapsed_ms < 120) {
            return 880;
        }
        if (elapsed_ms < 200) {
            return 0;
        }
        if (elapsed_ms < 320) {
            return 880;
        }
        if (elapsed_ms < 400) {
            return 0;
        }
        if (elapsed_ms < 520) {
            return 880;
        }
        if (elapsed_ms < 600) {
            return 0;
        }
        return 1319;
    case NotificationSoundPreset::Count:
        break;
    }
    return 1047;
}
