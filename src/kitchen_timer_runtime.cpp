#include "kitchen_timer_runtime.h"

KitchenTimerRuntime::KitchenTimerRuntime(KitchenTimer &timer)
    : timer_(timer), previous_state_(timer.state())
{
}

void KitchenTimerRuntime::setWakeCallback(WakeCallback callback, void *context)
{
    wake_callback_ = callback;
    wake_context_ = context;
}

void KitchenTimerRuntime::setAlertOutputCallback(AlertOutputCallback callback,
                                                 void *context)
{
    alert_output_callback_ = callback;
    alert_output_context_ = context;
}

void KitchenTimerRuntime::update(uint32_t now_ms)
{
    timer_.update(now_ms);
    KitchenTimerState current_state = timer_.state();

    if (current_state == KitchenTimerState::Alerting &&
        previous_state_ != KitchenTimerState::Alerting) {
        notification_.start(NotificationTarget::KitchenTimer,
                            notification_mode_, now_ms);
        if (wake_callback_ != nullptr) {
            wake_callback_(wake_context_);
        }
    } else if (current_state != KitchenTimerState::Alerting &&
               notification_.active()) {
        notification_.stop();
    }

    notification_.update(now_ms);
    if (current_state == KitchenTimerState::Alerting &&
        !notification_.active()) {
        timer_.stopAlert();
        current_state = timer_.state();
    }
    setAlertOutput(notification_.output(now_ms));

    previous_state_ = current_state;
}

void KitchenTimerRuntime::setNotificationMode(NotificationMode mode)
{
    notification_mode_ = mode;
    notification_.setMode(mode);
}

NotificationMode KitchenTimerRuntime::notificationMode() const
{
    return notification_mode_;
}

bool KitchenTimerRuntime::nextWakeDelayMilliseconds(
    uint32_t now_ms, uint32_t &delay_ms) const
{
    if (timer_.state() != KitchenTimerState::Running) {
        return false;
    }

    delay_ms = timer_.remainingMilliseconds(now_ms);
    return true;
}

bool KitchenTimerRuntime::requiresAwake() const
{
    return timer_.state() == KitchenTimerState::Alerting;
}

void KitchenTimerRuntime::setAlertOutput(NotificationOutputState output)
{
    if (alert_output_.target == output.target &&
        alert_output_.sound_active == output.sound_active &&
        alert_output_.vibration_active == output.vibration_active) {
        return;
    }

    alert_output_ = output;
    if (alert_output_callback_ != nullptr) {
        alert_output_callback_(output, alert_output_context_);
    }
}
