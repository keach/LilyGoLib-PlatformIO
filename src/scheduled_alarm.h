#pragma once

#include <stdint.h>
#include <time.h>

time_t nextScheduledAlarmEpoch(time_t now_epoch, uint8_t hour,
                               uint8_t minute);

class ScheduledAlarm {
public:
    bool configure(time_t now_epoch, uint8_t hour, uint8_t minute,
                   bool enabled);
    bool restore(uint8_t hour, uint8_t minute, bool enabled,
                 time_t trigger_epoch);
    bool setEnabled(bool enabled, time_t now_epoch);
    void stopAlert();
    void update(time_t now_epoch);

    uint8_t hour() const;
    uint8_t minute() const;
    bool enabled() const;
    bool alerting() const;
    time_t triggerEpoch() const;
    uint32_t secondsUntilTrigger(time_t now_epoch) const;

private:
    uint8_t hour_ = 7;
    uint8_t minute_ = 0;
    bool enabled_ = false;
    bool alerting_ = false;
    time_t trigger_epoch_ = 0;
};
