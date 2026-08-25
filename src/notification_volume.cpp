#include "notification_volume.h"

size_t notificationVolumeLevelCount()
{
    return static_cast<size_t>(NotificationVolumeLevel::Count);
}

bool isValidNotificationVolumeLevel(uint8_t value)
{
    return value < static_cast<uint8_t>(NotificationVolumeLevel::Count);
}

NotificationVolumeLevel resolveNotificationVolumeLevel(uint8_t value)
{
    return isValidNotificationVolumeLevel(value)
               ? static_cast<NotificationVolumeLevel>(value)
               : kDefaultNotificationVolumeLevel;
}

NotificationVolumeLevel decreaseNotificationVolumeLevel(
    NotificationVolumeLevel level)
{
    const uint8_t value = static_cast<uint8_t>(
        resolveNotificationVolumeLevel(static_cast<uint8_t>(level)));
    return static_cast<NotificationVolumeLevel>(value == 0 ? 0 : value - 1);
}

NotificationVolumeLevel increaseNotificationVolumeLevel(
    NotificationVolumeLevel level)
{
    const uint8_t value = static_cast<uint8_t>(
        resolveNotificationVolumeLevel(static_cast<uint8_t>(level)));
    const uint8_t maximum =
        static_cast<uint8_t>(NotificationVolumeLevel::Count) - 1;
    return static_cast<NotificationVolumeLevel>(
        value == maximum ? maximum : value + 1);
}

uint8_t notificationVolumePercent(NotificationVolumeLevel level)
{
    const uint8_t value = static_cast<uint8_t>(
        resolveNotificationVolumeLevel(static_cast<uint8_t>(level)));
    return static_cast<uint8_t>((value + 1) * 20);
}

float notificationVolumeGain(NotificationVolumeLevel level)
{
    constexpr float kDefaultGain = 0.35F;
    constexpr float kDefaultPercent = 60.0F;
    return notificationVolumePercent(level) *
           (kDefaultGain / kDefaultPercent);
}
