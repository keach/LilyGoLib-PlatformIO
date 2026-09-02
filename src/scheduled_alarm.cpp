#include "scheduled_alarm.h"

#include <limits.h>

namespace {

bool validTime(uint8_t hour, uint8_t minute)
{
    return hour < 24 && minute < 60;
}

}  // namespace

time_t nextScheduledAlarmEpoch(time_t now_epoch, uint8_t hour,
                               uint8_t minute)
{
    if (now_epoch <= 0 || !validTime(hour, minute)) {
        return 0;
    }

    struct tm local_time = {};
    if (localtime_r(&now_epoch, &local_time) == nullptr) {
        return 0;
    }
    local_time.tm_hour = hour;
    local_time.tm_min = minute;
    local_time.tm_sec = 0;
    local_time.tm_isdst = -1;
    time_t trigger = mktime(&local_time);
    if (trigger <= now_epoch) {
        local_time.tm_mday += 1;
        local_time.tm_isdst = -1;
        trigger = mktime(&local_time);
    }
    return trigger > now_epoch ? trigger : 0;
}

bool ScheduledAlarm::configure(time_t now_epoch, uint8_t hour,
                               uint8_t minute, bool enabled)
{
    if (!validTime(hour, minute)) {
        return false;
    }
    const time_t trigger = enabled
                               ? nextScheduledAlarmEpoch(now_epoch, hour,
                                                         minute)
                               : 0;
    if (enabled && trigger == 0) {
        return false;
    }
    hour_ = hour;
    minute_ = minute;
    enabled_ = enabled;
    alerting_ = false;
    trigger_epoch_ = trigger;
    return true;
}

bool ScheduledAlarm::restore(uint8_t hour, uint8_t minute, bool enabled,
                             time_t trigger_epoch)
{
    if (!validTime(hour, minute) || (enabled && trigger_epoch <= 0)) {
        return false;
    }
    hour_ = hour;
    minute_ = minute;
    enabled_ = enabled;
    alerting_ = false;
    trigger_epoch_ = enabled ? trigger_epoch : 0;
    return true;
}

bool ScheduledAlarm::setEnabled(bool enabled, time_t now_epoch)
{
    if (!enabled) {
        enabled_ = false;
        alerting_ = false;
        trigger_epoch_ = 0;
        return true;
    }
    const time_t trigger = nextScheduledAlarmEpoch(now_epoch, hour_, minute_);
    if (trigger == 0) {
        return false;
    }
    enabled_ = true;
    alerting_ = false;
    trigger_epoch_ = trigger;
    return true;
}

bool ScheduledAlarm::reschedule(time_t now_epoch)
{
    if (!enabled_) {
        return true;
    }
    const time_t trigger = nextScheduledAlarmEpoch(now_epoch, hour_, minute_);
    if (trigger == 0) {
        return false;
    }
    trigger_epoch_ = trigger;
    return true;
}

void ScheduledAlarm::stopAlert()
{
    alerting_ = false;
}

void ScheduledAlarm::update(time_t now_epoch)
{
    if (!enabled_ || trigger_epoch_ <= 0 || now_epoch < trigger_epoch_) {
        return;
    }
    enabled_ = false;
    alerting_ = true;
    trigger_epoch_ = 0;
}

uint8_t ScheduledAlarm::hour() const
{
    return hour_;
}

uint8_t ScheduledAlarm::minute() const
{
    return minute_;
}

bool ScheduledAlarm::enabled() const
{
    return enabled_;
}

bool ScheduledAlarm::alerting() const
{
    return alerting_;
}

time_t ScheduledAlarm::triggerEpoch() const
{
    return trigger_epoch_;
}

uint32_t ScheduledAlarm::secondsUntilTrigger(time_t now_epoch) const
{
    if (!enabled_ || trigger_epoch_ <= now_epoch) {
        return 0;
    }
    const uint64_t seconds = static_cast<uint64_t>(trigger_epoch_ - now_epoch);
    return seconds > UINT32_MAX ? UINT32_MAX
                                : static_cast<uint32_t>(seconds);
}
