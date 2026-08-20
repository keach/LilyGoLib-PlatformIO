#pragma once

#include <stddef.h>
#include <stdint.h>

enum class NotificationSoundPreset : uint8_t {
    Classic,
    DoubleBeep,
    Ascending,
    Count,
};

constexpr NotificationSoundPreset kDefaultNotificationSoundPreset =
    NotificationSoundPreset::Classic;
constexpr uint32_t kNotificationSoundPatternDurationMs = 1000;

size_t notificationSoundPresetCount();
bool isValidNotificationSoundPreset(uint8_t value);
NotificationSoundPreset resolveNotificationSoundPreset(uint8_t value);
const char *notificationSoundPresetLabel(NotificationSoundPreset preset);
NotificationSoundPreset previousNotificationSoundPreset(
    NotificationSoundPreset preset);
NotificationSoundPreset nextNotificationSoundPreset(
    NotificationSoundPreset preset);
uint16_t notificationSoundFrequencyAt(NotificationSoundPreset preset,
                                      uint32_t elapsed_ms);
