#pragma once

#include <stdint.h>

#include "kitchen_timer.h"
#include "kitchen_timer_runtime.h"
#include "kitchen_timer_screen.h"
#include "notification_settings_screen.h"
#include "notification_settings_store.h"

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
    uint32_t remainingSeconds(uint32_t now_ms) const;
    lv_obj_t *screen() const;

private:
    static void runtimeWakeCallback(void *context);
    static void showSettingsCallback(void *context);
    static void closeSettingsCallback(void *context);
    static void saveSettingsCallback(NotificationMode mode, void *context);

    void showTimerScreen(bool move_right);
    void showSettingsScreen();
    void saveNotificationMode(NotificationMode mode);

    KitchenTimer timer_;
    KitchenTimerRuntime runtime_;
    KitchenTimerScreen screen_;
    NotificationSettingsScreen settings_screen_;
    NotificationSettingsStore settings_store_;

    WakeCallback wake_callback_ = nullptr;
    void *wake_context_ = nullptr;
};
