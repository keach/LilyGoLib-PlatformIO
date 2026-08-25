#pragma once

#include "end_notification.h"
#include "notification_volume.h"

class NotificationSettingsStore {
public:
    NotificationMode loadMode(NotificationTarget target) const;
    bool saveMode(NotificationTarget target, NotificationMode mode) const;
    NotificationSoundPreset loadSoundPreset(NotificationTarget target) const;
    bool saveSoundPreset(NotificationTarget target,
                         NotificationSoundPreset preset) const;
    NotificationVolumeLevel loadMasterVolume() const;
    bool saveMasterVolume(NotificationVolumeLevel level) const;

private:
    static const char *modeKeyForTarget(NotificationTarget target);
    static const char *soundKeyForTarget(NotificationTarget target);
};
