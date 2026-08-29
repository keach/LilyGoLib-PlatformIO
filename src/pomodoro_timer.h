#pragma once

#include <stdint.h>

enum class PomodoroPhase : uint8_t {
    Focus,
    Break,
};

enum class PomodoroState : uint8_t {
    Idle,
    Running,
    Paused,
    Alerting,
};

class PomodoroTimer {
public:
    static constexpr uint32_t kFocusDurationMs = 25U * 60U * 1000U;
    static constexpr uint32_t kBreakDurationMs = 5U * 60U * 1000U;

    bool start(uint32_t now_ms);
    bool pause(uint32_t now_ms);
    bool resume(uint32_t now_ms);
    bool startNextPhase(uint32_t now_ms);
    void reset();
    void update(uint32_t now_ms);

    PomodoroPhase phase() const;
    PomodoroState state() const;
    uint32_t completedFocusSessions() const;
    uint32_t remainingMilliseconds(uint32_t now_ms) const;
    uint32_t remainingSeconds(uint32_t now_ms) const;

private:
    uint32_t phaseDurationMs() const;
    void startCurrentPhase(uint32_t now_ms);

    PomodoroPhase phase_ = PomodoroPhase::Focus;
    PomodoroState state_ = PomodoroState::Idle;
    uint32_t completed_focus_sessions_ = 0;
    uint32_t end_ms_ = 0;
    uint32_t paused_remaining_ms_ = 0;
};
