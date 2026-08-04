#pragma once

#include <stdint.h>

#include "kitchen_timer.h"

class KitchenTimerRuntime {
public:
    using WakeCallback = void (*)(void *context);
    using AlertOutputCallback = void (*)(bool active, void *context);

    explicit KitchenTimerRuntime(KitchenTimer &timer);

    void setWakeCallback(WakeCallback callback, void *context = nullptr);
    void setAlertOutputCallback(AlertOutputCallback callback,
                                void *context = nullptr);

    void update(uint32_t now_ms);

    bool nextWakeDelayMilliseconds(uint32_t now_ms,
                                   uint32_t &delay_ms) const;
    bool requiresAwake() const;

private:
    void setAlertOutput(bool active);

    KitchenTimer &timer_;
    WakeCallback wake_callback_ = nullptr;
    void *wake_context_ = nullptr;
    AlertOutputCallback alert_output_callback_ = nullptr;
    void *alert_output_context_ = nullptr;
    KitchenTimerState previous_state_ = KitchenTimerState::Idle;
    bool alert_output_active_ = false;
};
