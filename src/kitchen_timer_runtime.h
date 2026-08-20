#pragma once

#include <stdint.h>

#include "end_notification.h"
#include "kitchen_timer.h"

class KitchenTimerRuntime {
public:
    using WakeCallback = void (*)(void *context);
    using AlertOutputCallback = void (*)(NotificationOutputState output,
                                         void *context);

    explicit KitchenTimerRuntime(KitchenTimer &timer);

    void setWakeCallback(WakeCallback callback, void *context = nullptr);
    void setAlertOutputCallback(AlertOutputCallback callback,
                                void *context = nullptr);

    void update(uint32_t now_ms);
    void setNotificationMode(NotificationMode mode);
    NotificationMode notificationMode() const;

    bool nextWakeDelayMilliseconds(uint32_t now_ms,
                                   uint32_t &delay_ms) const;
    bool requiresAwake() const;

private:
    void setAlertOutput(NotificationOutputState output);

    KitchenTimer &timer_;
    WakeCallback wake_callback_ = nullptr;
    void *wake_context_ = nullptr;
    AlertOutputCallback alert_output_callback_ = nullptr;
    void *alert_output_context_ = nullptr;
    EndNotification notification_;
    NotificationMode notification_mode_ =
        NotificationMode::SoundAndVibration;
    KitchenTimerState previous_state_ = KitchenTimerState::Idle;
    NotificationOutputState alert_output_;
};
