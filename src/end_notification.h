#pragma once

#include <stdint.h>

#include "notification_sound.h"

enum class NotificationTarget : uint8_t {
    KitchenTimer,
    PomodoroTimer,
    ScheduledAlarm,
};

enum class NotificationMode : uint8_t {
    SoundAndVibration,
    SoundOnly,
    VibrationOnly,
};

struct NotificationOutputState {
    NotificationTarget target = NotificationTarget::KitchenTimer;
    NotificationSoundPreset sound_preset =
        kDefaultNotificationSoundPreset;
    bool sound_active = false;
    bool vibration_active = false;
};

const char *notificationModeLabel(NotificationMode mode);
bool isValidNotificationMode(uint8_t value);

class EndNotification {
public:
    static constexpr uint32_t kDurationMs = 30U * 1000U;
    static constexpr uint32_t kPhaseMs = 1000U;

    void start(NotificationTarget target, NotificationMode mode,
               NotificationSoundPreset sound_preset,
               uint32_t now_ms);
    void stop();
    void setMode(NotificationMode mode);
    void setSoundPreset(NotificationSoundPreset sound_preset);
    void update(uint32_t now_ms);

    bool active() const;
    NotificationTarget target() const;
    NotificationMode mode() const;
    NotificationSoundPreset soundPreset() const;
    NotificationOutputState output(uint32_t now_ms) const;

private:
    NotificationTarget target_ = NotificationTarget::KitchenTimer;
    NotificationMode mode_ = NotificationMode::SoundAndVibration;
    NotificationSoundPreset sound_preset_ =
        kDefaultNotificationSoundPreset;
    uint32_t started_ms_ = 0;
    bool active_ = false;
};
