#pragma once

#include <stdint.h>

#include "notification_settings_screen.h"
#include "notification_settings_store.h"
#include "notification_sound_settings_screen.h"
#include "pomodoro_timer.h"
#include "pomodoro_timer_runtime.h"
#include "pomodoro_timer_screen.h"

class PomodoroTimerApp {
public:
    using BackCallback = PomodoroTimerScreen::BackCallback;
    using WakeCallback = PomodoroTimerRuntime::WakeCallback;
    using AlertOutputCallback = PomodoroTimerRuntime::AlertOutputCallback;
    using PreviewSoundCallback = void (*)(NotificationSoundPreset preset,
                                          void *context);

    PomodoroTimerApp(uint32_t background_color,
                     uint32_t primary_color,
                     uint32_t accent_color,
                     uint32_t muted_color,
                     uint32_t button_color);

    void create(BackCallback back_callback,
                void *back_context,
                WakeCallback wake_callback,
                void *wake_context,
                AlertOutputCallback alert_output_callback,
                void *alert_output_context,
                PreviewSoundCallback preview_sound_callback,
                void *preview_sound_context);
    void show();
    void update(uint32_t now_ms);

    bool nextWakeDelayMilliseconds(uint32_t now_ms,
                                   uint32_t &delay_ms) const;
    bool requiresAwake() const;
    PomodoroPhase phase() const;
    PomodoroState state() const;
    uint32_t remainingSeconds(uint32_t now_ms) const;
    NotificationSoundPreset notificationSoundPreset() const;

private:
    static void runtimeWakeCallback(void *context);
    static void showSettingsCallback(void *context);
    static void closeSettingsCallback(void *context);
    static void saveSettingsCallback(NotificationMode mode,
                                     NotificationSoundPreset sound_preset,
                                     void *context);
    static void showSoundSettingsCallback(
        NotificationSoundPreset sound_preset, void *context);
    static void selectSoundPresetCallback(
        NotificationSoundPreset sound_preset, void *context);
    static void previewSoundCallback(NotificationSoundPreset sound_preset,
                                     void *context);
    static void closeSoundSettingsCallback(void *context);

    void showTimerScreen(bool move_right);
    void showSettingsScreen();
    void saveNotificationSettings(NotificationMode mode,
                                  NotificationSoundPreset sound_preset);
    void showSoundSettings(NotificationSoundPreset sound_preset);
    void previewSound(NotificationSoundPreset sound_preset);

    PomodoroTimer timer_;
    PomodoroTimerRuntime runtime_;
    PomodoroTimerScreen screen_;
    NotificationSettingsScreen settings_screen_;
    NotificationSoundSettingsScreen sound_settings_screen_;
    NotificationSettingsStore settings_store_;

    WakeCallback wake_callback_ = nullptr;
    void *wake_context_ = nullptr;
    PreviewSoundCallback preview_sound_callback_ = nullptr;
    void *preview_sound_context_ = nullptr;
};
