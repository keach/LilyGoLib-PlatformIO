#include "pomodoro_timer.h"

namespace {

bool deadlineReached(uint32_t now_ms, uint32_t deadline_ms)
{
    return static_cast<int32_t>(now_ms - deadline_ms) >= 0;
}

}  // namespace

bool PomodoroTimer::start(uint32_t now_ms)
{
    if (state_ != PomodoroState::Idle) {
        return false;
    }
    startCurrentPhase(now_ms);
    return true;
}

bool PomodoroTimer::pause(uint32_t now_ms)
{
    if (state_ != PomodoroState::Running) {
        return false;
    }
    update(now_ms);
    if (state_ != PomodoroState::Running) {
        return false;
    }
    paused_remaining_ms_ = remainingMilliseconds(now_ms);
    state_ = PomodoroState::Paused;
    return true;
}

bool PomodoroTimer::resume(uint32_t now_ms)
{
    if (state_ != PomodoroState::Paused) {
        return false;
    }
    end_ms_ = now_ms + paused_remaining_ms_;
    state_ = PomodoroState::Running;
    return true;
}

bool PomodoroTimer::startNextPhase(uint32_t now_ms)
{
    if (state_ != PomodoroState::Alerting) {
        return false;
    }
    phase_ = phase_ == PomodoroPhase::Focus
                 ? PomodoroPhase::Break
                 : PomodoroPhase::Focus;
    startCurrentPhase(now_ms);
    return true;
}

void PomodoroTimer::reset()
{
    phase_ = PomodoroPhase::Focus;
    state_ = PomodoroState::Idle;
    completed_focus_sessions_ = 0;
    end_ms_ = 0;
    paused_remaining_ms_ = 0;
}

void PomodoroTimer::update(uint32_t now_ms)
{
    if (state_ != PomodoroState::Running ||
        !deadlineReached(now_ms, end_ms_)) {
        return;
    }
    if (phase_ == PomodoroPhase::Focus) {
        ++completed_focus_sessions_;
    }
    state_ = PomodoroState::Alerting;
    paused_remaining_ms_ = 0;
}

PomodoroPhase PomodoroTimer::phase() const
{
    return phase_;
}

PomodoroState PomodoroTimer::state() const
{
    return state_;
}

uint32_t PomodoroTimer::completedFocusSessions() const
{
    return completed_focus_sessions_;
}

uint32_t PomodoroTimer::remainingMilliseconds(uint32_t now_ms) const
{
    if (state_ == PomodoroState::Idle) {
        return phaseDurationMs();
    }
    if (state_ == PomodoroState::Paused) {
        return paused_remaining_ms_;
    }
    if (state_ != PomodoroState::Running ||
        deadlineReached(now_ms, end_ms_)) {
        return 0;
    }
    return end_ms_ - now_ms;
}

uint32_t PomodoroTimer::remainingSeconds(uint32_t now_ms) const
{
    const uint32_t remaining_ms = remainingMilliseconds(now_ms);
    return (remaining_ms + 999) / 1000;
}

uint32_t PomodoroTimer::phaseDurationMs() const
{
    return phase_ == PomodoroPhase::Focus
               ? kFocusDurationMs
               : kBreakDurationMs;
}

void PomodoroTimer::startCurrentPhase(uint32_t now_ms)
{
    end_ms_ = now_ms + phaseDurationMs();
    paused_remaining_ms_ = 0;
    state_ = PomodoroState::Running;
}
