#pragma once

#include <time.h>

#include "end_notification.h"
#include "scheduled_alarm.h"

class ScheduledAlarmRuntime {
public:
    using WakeCallback = void (*)(void *context);
    using AlertOutputCallback = void (*)(NotificationOutputState output,
                                         void *context);

    explicit ScheduledAlarmRuntime(ScheduledAlarm &alarm);
    void setWakeCallback(WakeCallback callback, void *context = nullptr);
    void setAlertOutputCallback(AlertOutputCallback callback,
                                void *context = nullptr);
    void update(time_t now_epoch, uint32_t now_ms);

    void setNotificationMode(NotificationMode mode);
    NotificationMode notificationMode() const;
    void setNotificationSoundPreset(NotificationSoundPreset preset);
    NotificationSoundPreset notificationSoundPreset() const;
    bool nextWakeDelaySeconds(time_t now_epoch, uint32_t &delay_seconds) const;
    bool requiresAwake() const;

private:
    void setAlertOutput(NotificationOutputState output);

    ScheduledAlarm &alarm_;
    WakeCallback wake_callback_ = nullptr;
    void *wake_context_ = nullptr;
    AlertOutputCallback alert_output_callback_ = nullptr;
    void *alert_output_context_ = nullptr;
    EndNotification notification_;
    NotificationMode notification_mode_ =
        NotificationMode::SoundAndVibration;
    NotificationSoundPreset notification_sound_preset_ =
        kDefaultNotificationSoundPreset;
    bool previous_alerting_ = false;
    NotificationOutputState alert_output_ = {
        NotificationTarget::ScheduledAlarm,
        kDefaultNotificationSoundPreset,
        false,
        false,
    };
};
