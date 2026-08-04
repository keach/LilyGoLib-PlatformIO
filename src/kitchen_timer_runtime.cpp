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
    const KitchenTimerState current_state = timer_.state();

    if (current_state == KitchenTimerState::Alerting &&
        previous_state_ != KitchenTimerState::Alerting &&
        wake_callback_ != nullptr) {
        wake_callback_(wake_context_);
    }

    const bool output_active =
        current_state == KitchenTimerState::Alerting &&
        timer_.alertOutputActive(now_ms);
    setAlertOutput(output_active);

    previous_state_ = current_state;
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

void KitchenTimerRuntime::setAlertOutput(bool active)
{
    if (alert_output_active_ == active) {
        return;
    }

    alert_output_active_ = active;
    if (alert_output_callback_ != nullptr) {
        alert_output_callback_(active, alert_output_context_);
    }
}
