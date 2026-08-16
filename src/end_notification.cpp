#include "end_notification.h"

const char *notificationModeLabel(NotificationMode mode)
{
    switch (mode) {
    case NotificationMode::SoundAndVibration:
        return "SOUND + VIBRATION";
    case NotificationMode::SoundOnly:
        return "SOUND ONLY";
    case NotificationMode::VibrationOnly:
        return "VIBRATION ONLY";
    }
    return "SOUND + VIBRATION";
}

bool isValidNotificationMode(uint8_t value)
{
    return value <= static_cast<uint8_t>(NotificationMode::VibrationOnly);
}

void EndNotification::start(NotificationTarget target, NotificationMode mode,
                            uint32_t now_ms)
{
    target_ = target;
    mode_ = mode;
    started_ms_ = now_ms;
    active_ = true;
}

void EndNotification::stop()
{
    active_ = false;
}

void EndNotification::setMode(NotificationMode mode)
{
    mode_ = mode;
}

void EndNotification::update(uint32_t now_ms)
{
    if (active_ && now_ms - started_ms_ >= kDurationMs) {
        stop();
    }
}

bool EndNotification::active() const
{
    return active_;
}

NotificationTarget EndNotification::target() const
{
    return target_;
}

NotificationMode EndNotification::mode() const
{
    return mode_;
}

NotificationOutputState EndNotification::output(uint32_t now_ms) const
{
    NotificationOutputState state;
    state.target = target_;
    if (!active_ || (now_ms - started_ms_) / kPhaseMs % 2 != 0) {
        return state;
    }

    state.sound_active = mode_ != NotificationMode::VibrationOnly;
    state.vibration_active = mode_ != NotificationMode::SoundOnly;
    return state;
}
