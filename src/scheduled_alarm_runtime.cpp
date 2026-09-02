#include "scheduled_alarm_runtime.h"

ScheduledAlarmRuntime::ScheduledAlarmRuntime(ScheduledAlarm &alarm)
    : alarm_(alarm), previous_alerting_(alarm.alerting())
{
}

void ScheduledAlarmRuntime::setWakeCallback(WakeCallback callback,
                                            void *context)
{
    wake_callback_ = callback;
    wake_context_ = context;
}

void ScheduledAlarmRuntime::setAlertOutputCallback(
    AlertOutputCallback callback, void *context)
{
    alert_output_callback_ = callback;
    alert_output_context_ = context;
}

void ScheduledAlarmRuntime::update(time_t now_epoch, uint32_t now_ms)
{
    alarm_.update(now_epoch);
    const bool alerting = alarm_.alerting();
    if (alerting && !previous_alerting_) {
        notification_.start(NotificationTarget::ScheduledAlarm,
                            notification_mode_, notification_sound_preset_,
                            now_ms);
        if (wake_callback_ != nullptr) {
            wake_callback_(wake_context_);
        }
    } else if (!alerting && notification_.active()) {
        notification_.stop();
    }
    notification_.update(now_ms);
    NotificationOutputState output = notification_.output(now_ms);
    output.target = NotificationTarget::ScheduledAlarm;
    setAlertOutput(output);
    previous_alerting_ = alerting;
}

void ScheduledAlarmRuntime::setNotificationMode(NotificationMode mode)
{
    notification_mode_ = mode;
    notification_.setMode(mode);
}

NotificationMode ScheduledAlarmRuntime::notificationMode() const
{
    return notification_mode_;
}

void ScheduledAlarmRuntime::setNotificationSoundPreset(
    NotificationSoundPreset preset)
{
    notification_sound_preset_ = resolveNotificationSoundPreset(
        static_cast<uint8_t>(preset));
    notification_.setSoundPreset(notification_sound_preset_);
}

NotificationSoundPreset ScheduledAlarmRuntime::notificationSoundPreset() const
{
    return notification_sound_preset_;
}

bool ScheduledAlarmRuntime::nextWakeDelaySeconds(
    time_t now_epoch, uint32_t &delay_seconds) const
{
    if (!alarm_.enabled()) {
        return false;
    }
    delay_seconds = alarm_.secondsUntilTrigger(now_epoch);
    return true;
}

bool ScheduledAlarmRuntime::requiresAwake() const
{
    return notification_.active();
}

void ScheduledAlarmRuntime::setAlertOutput(NotificationOutputState output)
{
    if (alert_output_.target == output.target &&
        alert_output_.sound_preset == output.sound_preset &&
        alert_output_.sound_active == output.sound_active &&
        alert_output_.vibration_active == output.vibration_active) {
        return;
    }
    alert_output_ = output;
    if (alert_output_callback_ != nullptr) {
        alert_output_callback_(output, alert_output_context_);
    }
}
