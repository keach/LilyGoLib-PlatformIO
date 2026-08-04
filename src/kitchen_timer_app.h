#pragma once

#include <stdint.h>

#include "kitchen_timer.h"
#include "kitchen_timer_runtime.h"
#include "kitchen_timer_screen.h"

class KitchenTimerApp {
public:
    using BackCallback = KitchenTimerScreen::BackCallback;
    using WakeCallback = KitchenTimerRuntime::WakeCallback;
    using AlertOutputCallback = KitchenTimerRuntime::AlertOutputCallback;

    KitchenTimerApp(uint32_t background_color,
                    uint32_t primary_color,
                    uint32_t accent_color,
                    uint32_t muted_color,
                    uint32_t button_color);

    void create(BackCallback back_callback,
                void *back_context,
                WakeCallback wake_callback,
                void *wake_context,
                AlertOutputCallback alert_output_callback,
                void *alert_output_context);

    void show();
    void update(uint32_t now_ms);

    bool nextWakeDelayMilliseconds(uint32_t now_ms,
                                   uint32_t &delay_ms) const;
    bool requiresAwake() const;
    KitchenTimerState state() const;
    lv_obj_t *screen() const;

private:
    static void runtimeWakeCallback(void *context);

    KitchenTimer timer_;
    KitchenTimerRuntime runtime_;
    KitchenTimerScreen screen_;

    WakeCallback wake_callback_ = nullptr;
    void *wake_context_ = nullptr;
};
