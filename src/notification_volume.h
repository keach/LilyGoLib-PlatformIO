#pragma once

#include <stddef.h>
#include <stdint.h>

enum class NotificationVolumeLevel : uint8_t {
    Level1 = 0,
    Level2 = 1,
    Level3 = 2,
    Level4 = 3,
    Level5 = 4,
    Count = 5,
};

constexpr NotificationVolumeLevel kDefaultNotificationVolumeLevel =
    NotificationVolumeLevel::Level3;

size_t notificationVolumeLevelCount();
bool isValidNotificationVolumeLevel(uint8_t value);
NotificationVolumeLevel resolveNotificationVolumeLevel(uint8_t value);
NotificationVolumeLevel decreaseNotificationVolumeLevel(
    NotificationVolumeLevel level);
NotificationVolumeLevel increaseNotificationVolumeLevel(
    NotificationVolumeLevel level);
uint8_t notificationVolumePercent(NotificationVolumeLevel level);
float notificationVolumeGain(NotificationVolumeLevel level);
