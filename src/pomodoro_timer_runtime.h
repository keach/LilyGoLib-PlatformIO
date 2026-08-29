#pragma once

#include <stdint.h>

#include "end_notification.h"
#include "pomodoro_timer.h"

class PomodoroTimerRuntime {
public:
    using WakeCallback = void (*)(void *context);
    using AlertOutputCallback = void (*)(NotificationOutputState output,
                                         void *context);

    explicit PomodoroTimerRuntime(PomodoroTimer &timer);

    void setWakeCallback(WakeCallback callback, void *context = nullptr);
    void setAlertOutputCallback(AlertOutputCallback callback,
                                void *context = nullptr);
    void update(uint32_t now_ms);

    void setNotificationMode(NotificationMode mode);
    NotificationMode notificationMode() const;
    void setNotificationSoundPreset(NotificationSoundPreset preset);
    NotificationSoundPreset notificationSoundPreset() const;

    bool nextWakeDelayMilliseconds(uint32_t now_ms,
                                   uint32_t &delay_ms) const;
    bool requiresAwake() const;

private:
    void setAlertOutput(NotificationOutputState output);

    PomodoroTimer &timer_;
    WakeCallback wake_callback_ = nullptr;
    void *wake_context_ = nullptr;
    AlertOutputCallback alert_output_callback_ = nullptr;
    void *alert_output_context_ = nullptr;
    EndNotification notification_;
    NotificationMode notification_mode_ =
        NotificationMode::SoundAndVibration;
    NotificationSoundPreset notification_sound_preset_ =
        kDefaultNotificationSoundPreset;
    PomodoroState previous_state_ = PomodoroState::Idle;
    NotificationOutputState alert_output_ = {
        NotificationTarget::PomodoroTimer,
        kDefaultNotificationSoundPreset,
        false,
        false,
    };
};
