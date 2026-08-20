#pragma once

#include <stdint.h>

#include "kitchen_timer.h"
#include "kitchen_timer_runtime.h"
#include "kitchen_timer_screen.h"
#include "notification_settings_screen.h"
#include "notification_settings_store.h"
#include "notification_sound_settings_screen.h"

class KitchenTimerApp {
public:
    using BackCallback = KitchenTimerScreen::BackCallback;
    using WakeCallback = KitchenTimerRuntime::WakeCallback;
    using AlertOutputCallback = KitchenTimerRuntime::AlertOutputCallback;
    using PreviewSoundCallback = void (*)(NotificationSoundPreset preset,
                                          void *context);

    KitchenTimerApp(uint32_t background_color,
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
    KitchenTimerState state() const;
    uint32_t remainingSeconds(uint32_t now_ms) const;
    lv_obj_t *screen() const;

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

    KitchenTimer timer_;
    KitchenTimerRuntime runtime_;
    KitchenTimerScreen screen_;
    NotificationSettingsScreen settings_screen_;
    NotificationSoundSettingsScreen sound_settings_screen_;
    NotificationSettingsStore settings_store_;

    WakeCallback wake_callback_ = nullptr;
    void *wake_context_ = nullptr;
    PreviewSoundCallback preview_sound_callback_ = nullptr;
    void *preview_sound_context_ = nullptr;
};
