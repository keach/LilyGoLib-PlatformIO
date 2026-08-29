#include "pomodoro_timer_runtime.h"

PomodoroTimerRuntime::PomodoroTimerRuntime(PomodoroTimer &timer)
    : timer_(timer), previous_state_(timer.state())
{
}

void PomodoroTimerRuntime::setWakeCallback(WakeCallback callback,
                                           void *context)
{
    wake_callback_ = callback;
    wake_context_ = context;
}

void PomodoroTimerRuntime::setAlertOutputCallback(
    AlertOutputCallback callback, void *context)
{
    alert_output_callback_ = callback;
    alert_output_context_ = context;
}

void PomodoroTimerRuntime::update(uint32_t now_ms)
{
    timer_.update(now_ms);
    const PomodoroState current_state = timer_.state();

    if (current_state == PomodoroState::Alerting &&
        previous_state_ != PomodoroState::Alerting) {
        notification_.start(NotificationTarget::PomodoroTimer,
                            notification_mode_, notification_sound_preset_,
                            now_ms);
        if (wake_callback_ != nullptr) {
            wake_callback_(wake_context_);
        }
    } else if (current_state != PomodoroState::Alerting &&
               notification_.active()) {
        notification_.stop();
    }

    notification_.update(now_ms);
    NotificationOutputState output = notification_.output(now_ms);
    output.target = NotificationTarget::PomodoroTimer;
    setAlertOutput(output);
    previous_state_ = current_state;
}

void PomodoroTimerRuntime::setNotificationMode(NotificationMode mode)
{
    notification_mode_ = mode;
    notification_.setMode(mode);
}

NotificationMode PomodoroTimerRuntime::notificationMode() const
{
    return notification_mode_;
}

void PomodoroTimerRuntime::setNotificationSoundPreset(
    NotificationSoundPreset preset)
{
    notification_sound_preset_ = resolveNotificationSoundPreset(
        static_cast<uint8_t>(preset));
    notification_.setSoundPreset(notification_sound_preset_);
}

NotificationSoundPreset PomodoroTimerRuntime::notificationSoundPreset() const
{
    return notification_sound_preset_;
}

bool PomodoroTimerRuntime::nextWakeDelayMilliseconds(
    uint32_t now_ms, uint32_t &delay_ms) const
{
    if (timer_.state() != PomodoroState::Running) {
        return false;
    }
    delay_ms = timer_.remainingMilliseconds(now_ms);
    return true;
}

bool PomodoroTimerRuntime::requiresAwake() const
{
    return notification_.active();
}

void PomodoroTimerRuntime::setAlertOutput(NotificationOutputState output)
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
