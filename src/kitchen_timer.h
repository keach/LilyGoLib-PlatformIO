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
    static constexpr uint32_t kMinimumDurationMs = 0;
    static constexpr uint32_t kMaximumDurationMs =
        (99U * 60U + 59U) * 1000U;
    bool setDurationSeconds(uint32_t duration_seconds);
    void adjustSeconds(int32_t delta_seconds);

    bool start(uint32_t now_ms);
    bool pause(uint32_t now_ms);
    bool resume(uint32_t now_ms);
    void cancel();
    void reset();
    void stopAlert();
    void update(uint32_t now_ms);

    KitchenTimerState state() const;
    uint32_t configuredSeconds() const;
    uint32_t remainingMilliseconds(uint32_t now_ms) const;
    uint32_t remainingSeconds(uint32_t now_ms) const;

private:
    static uint32_t clampDurationMs(int64_t duration_ms);

    KitchenTimerState state_ = KitchenTimerState::Idle;
    uint32_t configured_duration_ms_ = 0;
    uint32_t end_ms_ = 0;
    uint32_t paused_remaining_ms_ = 0;
};
