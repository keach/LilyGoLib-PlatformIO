#pragma once

#include "scheduled_alarm.h"

class ScheduledAlarmStore {
public:
    bool load(ScheduledAlarm &alarm) const;
    bool save(const ScheduledAlarm &alarm) const;
};
