#pragma once

#include "end_notification.h"

class NotificationSettingsStore {
public:
    NotificationMode loadMode(NotificationTarget target) const;
    bool saveMode(NotificationTarget target, NotificationMode mode) const;

private:
    static const char *keyForTarget(NotificationTarget target);
};
