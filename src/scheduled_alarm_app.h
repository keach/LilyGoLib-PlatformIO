#pragma once

#include <time.h>

#include "notification_settings_screen.h"
#include "notification_settings_store.h"
#include "notification_sound_settings_screen.h"
#include "scheduled_alarm_runtime.h"
#include "scheduled_alarm_screen.h"
#include "scheduled_alarm_store.h"

class ScheduledAlarmApp {
public:
    using ActionCallback = ScheduledAlarmScreen::ActionCallback;
    using WakeCallback = ScheduledAlarmRuntime::WakeCallback;
    using AlertOutputCallback = ScheduledAlarmRuntime::AlertOutputCallback;
    using PreviewSoundCallback = void (*)(NotificationSoundPreset preset,
                                          void *context);

    ScheduledAlarmApp(uint32_t background_color,
                      uint32_t primary_color,
                      uint32_t accent_color,
                      uint32_t muted_color,
                      uint32_t button_color);
    void create(ActionCallback back_callback, void *back_context,
                WakeCallback wake_callback, void *wake_context,
                AlertOutputCallback alert_output_callback,
                void *alert_output_context,
                PreviewSoundCallback preview_sound_callback,
                void *preview_sound_context);
    void show();
    void update(time_t now_epoch, uint32_t now_ms);
    void handleClockAdjusted(time_t now_epoch);
    void setUse24HourClock(bool use_24_hour_clock);
    bool nextWakeDelaySeconds(time_t now_epoch, uint32_t &delay_seconds) const;
    bool requiresAwake() const;
    bool enabled() const;
    bool alerting() const;
    NotificationSoundPreset notificationSoundPreset() const;

private:
    static void configureCallback(uint8_t hour, uint8_t minute,
                                  bool enabled, void *context);
    static void stopCallback(void *context);
    static void runtimeWakeCallback(void *context);
    static void showSettingsCallback(void *context);
    static void closeSettingsCallback(void *context);
    static void saveSettingsCallback(NotificationMode mode,
                                     NotificationSoundPreset preset,
                                     void *context);
    static void showSoundSettingsCallback(NotificationSoundPreset preset,
                                          void *context);
    static void selectSoundPresetCallback(NotificationSoundPreset preset,
                                          void *context);
    static void previewSoundCallback(NotificationSoundPreset preset,
                                     void *context);
    static void closeSoundSettingsCallback(void *context);

    void configure(uint8_t hour, uint8_t minute, bool enabled);
    void stop();
    void showAlarmScreen(bool move_right);
    void saveNotificationSettings(NotificationMode mode,
                                  NotificationSoundPreset preset);

    ScheduledAlarm alarm_;
    ScheduledAlarmRuntime runtime_;
    ScheduledAlarmScreen screen_;
    ScheduledAlarmStore store_;
    NotificationSettingsScreen settings_screen_;
    NotificationSoundSettingsScreen sound_settings_screen_;
    NotificationSettingsStore settings_store_;
    bool use_24_hour_clock_ = true;
    WakeCallback wake_callback_ = nullptr;
    void *wake_context_ = nullptr;
    PreviewSoundCallback preview_sound_callback_ = nullptr;
    void *preview_sound_context_ = nullptr;
};
