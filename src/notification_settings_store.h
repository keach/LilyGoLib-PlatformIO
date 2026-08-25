#pragma once

#include "end_notification.h"

class NotificationSettingsStore {
public:
    NotificationMode loadMode(NotificationTarget target) const;
    bool saveMode(NotificationTarget target, NotificationMode mode) const;
    NotificationSoundPreset loadSoundPreset(NotificationTarget target) const;
    bool saveSoundPreset(NotificationTarget target,
                         NotificationSoundPreset preset) const;

private:
    static const char *modeKeyForTarget(NotificationTarget target);
    static const char *soundKeyForTarget(NotificationTarget target);
};
