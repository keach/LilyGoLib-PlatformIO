#include "scheduled_alarm_app.h"

#include <Arduino.h>

ScheduledAlarmApp::ScheduledAlarmApp(
    uint32_t background_color, uint32_t primary_color,
    uint32_t accent_color, uint32_t muted_color, uint32_t button_color)
    : runtime_(alarm_),
      screen_(alarm_, background_color, primary_color, accent_color,
              muted_color, button_color),
      settings_screen_(background_color, primary_color, accent_color,
                       muted_color, button_color),
      sound_settings_screen_(background_color, primary_color, accent_color,
                             muted_color, button_color)
{
}

void ScheduledAlarmApp::create(
    ActionCallback back_callback, void *back_context,
    WakeCallback wake_callback, void *wake_context,
    AlertOutputCallback alert_output_callback, void *alert_output_context,
    PreviewSoundCallback preview_sound_callback, void *preview_sound_context)
{
    wake_callback_ = wake_callback;
    wake_context_ = wake_context;
    preview_sound_callback_ = preview_sound_callback;
    preview_sound_context_ = preview_sound_context;
    if (!store_.load(alarm_)) {
        Serial.println("Scheduled alarm: using defaults");
    }
    runtime_.setNotificationMode(
        settings_store_.loadMode(NotificationTarget::ScheduledAlarm));
    runtime_.setNotificationSoundPreset(
        settings_store_.loadSoundPreset(NotificationTarget::ScheduledAlarm));
    screen_.create(configureCallback, this, stopCallback, this,
                   showSettingsCallback, this, back_callback, back_context);
    settings_screen_.create("ALARM SETTINGS", saveSettingsCallback, this,
                            showSoundSettingsCallback, this,
                            closeSettingsCallback, this);
    sound_settings_screen_.create(selectSoundPresetCallback, this,
                                  previewSoundCallback, this,
                                  closeSoundSettingsCallback, this);
    runtime_.setWakeCallback(runtimeWakeCallback, this);
    runtime_.setAlertOutputCallback(alert_output_callback,
                                    alert_output_context);
}

void ScheduledAlarmApp::show()
{
    showAlarmScreen(false);
}

void ScheduledAlarmApp::update(time_t now_epoch, uint32_t now_ms)
{
    const bool was_enabled = alarm_.enabled();
    runtime_.update(now_epoch, now_ms);
    if (was_enabled && !alarm_.enabled()) {
        if (!store_.save(alarm_)) {
            Serial.println("Scheduled alarm: auto-disable save failed");
        }
    }
    if (lv_screen_active() == screen_.screen()) {
        screen_.refresh(use_24_hour_clock_);
    }
}

void ScheduledAlarmApp::handleClockAdjusted(time_t now_epoch)
{
    if (!alarm_.enabled()) {
        return;
    }
    if (!alarm_.reschedule(now_epoch)) {
        Serial.println("Scheduled alarm: reschedule failed");
        return;
    }
    if (!store_.save(alarm_)) {
        Serial.println("Scheduled alarm: reschedule save failed");
    }
    if (lv_screen_active() == screen_.screen()) {
        screen_.refresh(use_24_hour_clock_);
    }
    Serial.println("Scheduled alarm: rescheduled after clock adjustment");
}

void ScheduledAlarmApp::setUse24HourClock(bool use_24_hour_clock)
{
    use_24_hour_clock_ = use_24_hour_clock;
}

bool ScheduledAlarmApp::nextWakeDelaySeconds(
    time_t now_epoch, uint32_t &delay_seconds) const
{
    return runtime_.nextWakeDelaySeconds(now_epoch, delay_seconds);
}

bool ScheduledAlarmApp::requiresAwake() const { return runtime_.requiresAwake(); }
bool ScheduledAlarmApp::enabled() const { return alarm_.enabled(); }
bool ScheduledAlarmApp::alerting() const { return alarm_.alerting(); }

NotificationSoundPreset ScheduledAlarmApp::notificationSoundPreset() const
{
    return runtime_.notificationSoundPreset();
}

void ScheduledAlarmApp::configureCallback(uint8_t hour, uint8_t minute,
                                          bool enabled, void *context)
{
    auto *self = static_cast<ScheduledAlarmApp *>(context);
    if (self != nullptr) self->configure(hour, minute, enabled);
}

void ScheduledAlarmApp::stopCallback(void *context)
{
    auto *self = static_cast<ScheduledAlarmApp *>(context);
    if (self != nullptr) self->stop();
}

void ScheduledAlarmApp::runtimeWakeCallback(void *context)
{
    auto *self = static_cast<ScheduledAlarmApp *>(context);
    if (self == nullptr) return;
    if (self->wake_callback_ != nullptr) {
        self->wake_callback_(self->wake_context_);
    }
    self->show();
}

void ScheduledAlarmApp::showSettingsCallback(void *context)
{
    auto *self = static_cast<ScheduledAlarmApp *>(context);
    if (self != nullptr) {
        self->settings_screen_.show(self->runtime_.notificationMode(),
                                    self->runtime_.notificationSoundPreset());
    }
}

void ScheduledAlarmApp::closeSettingsCallback(void *context)
{
    auto *self = static_cast<ScheduledAlarmApp *>(context);
    if (self != nullptr) self->showAlarmScreen(true);
}

void ScheduledAlarmApp::saveSettingsCallback(
    NotificationMode mode, NotificationSoundPreset preset, void *context)
{
    auto *self = static_cast<ScheduledAlarmApp *>(context);
    if (self != nullptr) self->saveNotificationSettings(mode, preset);
}

void ScheduledAlarmApp::showSoundSettingsCallback(
    NotificationSoundPreset preset, void *context)
{
    auto *self = static_cast<ScheduledAlarmApp *>(context);
    if (self != nullptr) self->sound_settings_screen_.show(preset);
}

void ScheduledAlarmApp::selectSoundPresetCallback(
    NotificationSoundPreset preset, void *context)
{
    auto *self = static_cast<ScheduledAlarmApp *>(context);
    if (self != nullptr) self->settings_screen_.updateSoundPreset(preset);
}

void ScheduledAlarmApp::previewSoundCallback(
    NotificationSoundPreset preset, void *context)
{
    auto *self = static_cast<ScheduledAlarmApp *>(context);
    if (self != nullptr && self->preview_sound_callback_ != nullptr) {
        self->preview_sound_callback_(preset, self->preview_sound_context_);
    }
}

void ScheduledAlarmApp::closeSoundSettingsCallback(void *context)
{
    auto *self = static_cast<ScheduledAlarmApp *>(context);
    if (self != nullptr) self->settings_screen_.showPending();
}

void ScheduledAlarmApp::configure(uint8_t hour, uint8_t minute, bool enabled)
{
    if (!alarm_.configure(time(nullptr), hour, minute, enabled)) {
        Serial.println("Scheduled alarm: configure failed");
        return;
    }
    if (!store_.save(alarm_)) {
        Serial.println("Scheduled alarm: save failed");
    }
    screen_.refresh(use_24_hour_clock_);
}

void ScheduledAlarmApp::stop()
{
    alarm_.stopAlert();
    runtime_.update(time(nullptr), millis());
    screen_.refresh(use_24_hour_clock_);
}

void ScheduledAlarmApp::showAlarmScreen(bool move_right)
{
    screen_.show(use_24_hour_clock_, move_right);
}

void ScheduledAlarmApp::saveNotificationSettings(
    NotificationMode mode, NotificationSoundPreset preset)
{
    runtime_.setNotificationMode(mode);
    runtime_.setNotificationSoundPreset(preset);
    if (!settings_store_.saveMode(NotificationTarget::ScheduledAlarm, mode)) {
        Serial.println("Scheduled alarm notification mode: save failed");
    }
    if (!settings_store_.saveSoundPreset(
            NotificationTarget::ScheduledAlarm, preset)) {
        Serial.println("Scheduled alarm notification sound: save failed");
    }
}
