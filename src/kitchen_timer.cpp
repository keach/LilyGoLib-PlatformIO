#include "kitchen_timer.h"

#include <algorithm>

namespace {

bool deadlineReached(uint32_t now_ms, uint32_t deadline_ms)
{
    return static_cast<int32_t>(now_ms - deadline_ms) >= 0;
}

}  // namespace

uint32_t KitchenTimer::clampDurationMs(int64_t duration_ms)
{
    return static_cast<uint32_t>(std::clamp<int64_t>(
        duration_ms, kMinimumDurationMs, kMaximumDurationMs));
}

bool KitchenTimer::setDurationSeconds(uint32_t duration_seconds)
{
    if (state_ != KitchenTimerState::Idle) {
        return false;
    }
    configured_duration_ms_ = clampDurationMs(
        static_cast<int64_t>(duration_seconds) * 1000);
    return true;
}

void KitchenTimer::adjustSeconds(int32_t delta_seconds)
{
    if (state_ != KitchenTimerState::Idle) {
        return;
    }
    configured_duration_ms_ = clampDurationMs(
        static_cast<int64_t>(configured_duration_ms_) +
        static_cast<int64_t>(delta_seconds) * 1000);
}

bool KitchenTimer::start(uint32_t now_ms)
{
    if (state_ != KitchenTimerState::Idle || configured_duration_ms_ == 0) {
        return false;
    }
    end_ms_ = now_ms + configured_duration_ms_;
    paused_remaining_ms_ = 0;
    state_ = KitchenTimerState::Running;
    return true;
}

void KitchenTimer::reset()
{
    if (state_ == KitchenTimerState::Running ||
        state_ == KitchenTimerState::Alerting) {
        return;
    }
    cancel();
    configured_duration_ms_ = 0;
}

bool KitchenTimer::pause(uint32_t now_ms)
{
    if (state_ != KitchenTimerState::Running) {
        return false;
    }

    // Process expiry before pausing so an elapsed timer cannot become a
    // zero-duration paused timer and suppress its notification.
    update(now_ms);
    if (state_ != KitchenTimerState::Running) {
        return false;
    }

    paused_remaining_ms_ = remainingMilliseconds(now_ms);
    state_ = KitchenTimerState::Paused;
    return true;
}

bool KitchenTimer::resume(uint32_t now_ms)
{
    if (state_ != KitchenTimerState::Paused) {
        return false;
    }
    end_ms_ = now_ms + paused_remaining_ms_;
    state_ = KitchenTimerState::Running;
    return true;
}

void KitchenTimer::cancel()
{
    state_ = KitchenTimerState::Idle;
    end_ms_ = 0;
    paused_remaining_ms_ = 0;
}

void KitchenTimer::stopAlert()
{
    if (state_ == KitchenTimerState::Alerting) {
        cancel();
    }
}

void KitchenTimer::update(uint32_t now_ms)
{
    if (state_ == KitchenTimerState::Running && deadlineReached(now_ms, end_ms_)) {
        state_ = KitchenTimerState::Alerting;
        paused_remaining_ms_ = 0;
    }
}

KitchenTimerState KitchenTimer::state() const
{
    return state_;
}

uint32_t KitchenTimer::configuredSeconds() const
{
    return configured_duration_ms_ / 1000;
}

uint32_t KitchenTimer::remainingMilliseconds(uint32_t now_ms) const
{
    if (state_ == KitchenTimerState::Paused) {
        return paused_remaining_ms_;
    }
    if (state_ != KitchenTimerState::Running || deadlineReached(now_ms, end_ms_)) {
        return 0;
    }
    return end_ms_ - now_ms;
}

uint32_t KitchenTimer::remainingSeconds(uint32_t now_ms) const
{
    const uint32_t remaining_ms = remainingMilliseconds(now_ms);
    return (remaining_ms + 999) / 1000;
}
