#pragma once

#include <stdint.h>

enum class KitchenTimerState : uint8_t {
    Idle,
    Running,
    Paused,
    Alerting,
};

class KitchenTimer {
public:
    static constexpr uint32_t kMinimumDurationMs = 1000;
    static constexpr uint32_t kMaximumDurationMs =
        (99U * 60U + 59U) * 1000U;
    static constexpr uint32_t kAlertDurationMs = 30U * 1000U;
    static constexpr uint32_t kAlertPhaseMs = 1000U;

    bool setDurationSeconds(uint32_t duration_seconds);
    void adjustSeconds(int32_t delta_seconds);

    bool start(uint32_t now_ms);
    bool pause(uint32_t now_ms);
    bool resume(uint32_t now_ms);
    void cancel();
    void stopAlert();
    void update(uint32_t now_ms);

    KitchenTimerState state() const;
    uint32_t configuredSeconds() const;
    uint32_t remainingSeconds(uint32_t now_ms) const;
    bool alertOutputActive(uint32_t now_ms) const;

private:
    static uint32_t clampDurationMs(int64_t duration_ms);
    uint32_t remainingMilliseconds(uint32_t now_ms) const;

    KitchenTimerState state_ = KitchenTimerState::Idle;
    uint32_t configured_duration_ms_ = 60U * 1000U;
    uint32_t end_ms_ = 0;
    uint32_t paused_remaining_ms_ = 0;
    uint32_t alert_started_ms_ = 0;
};
