#include "kitchen_timer_app.h"

#include <Arduino.h>

KitchenTimerApp::KitchenTimerApp(uint32_t background_color,
                                 uint32_t primary_color,
                                 uint32_t accent_color,
                                 uint32_t muted_color,
                                 uint32_t button_color)
    : runtime_(timer_),
      screen_(timer_,
              background_color,
              primary_color,
              accent_color,
              muted_color,
              button_color),
      settings_screen_(background_color,
                       primary_color,
                       accent_color,
                       muted_color,
                       button_color),
      sound_settings_screen_(background_color,
                             primary_color,
                             accent_color,
                             muted_color,
                             button_color)
{
}

void KitchenTimerApp::create(BackCallback back_callback,
                             void *back_context,
                             WakeCallback wake_callback,
                             void *wake_context,
                             AlertOutputCallback alert_output_callback,
                             void *alert_output_context,
                             PreviewSoundCallback preview_sound_callback,
                             void *preview_sound_context)
{
    wake_callback_ = wake_callback;
    wake_context_ = wake_context;
    preview_sound_callback_ = preview_sound_callback;
    preview_sound_context_ = preview_sound_context;

    runtime_.setNotificationMode(
        settings_store_.loadMode(NotificationTarget::KitchenTimer));
    runtime_.setNotificationSoundPreset(
        settings_store_.loadSoundPreset(NotificationTarget::KitchenTimer));
    screen_.create(back_callback, back_context,
                   showSettingsCallback, this);
    settings_screen_.create("TIMER SETTINGS",
                            saveSettingsCallback, this,
                            showSoundSettingsCallback, this,
                            closeSettingsCallback, this);
    sound_settings_screen_.create(
        selectSoundPresetCallback, this,
        previewSoundCallback, this,
        closeSoundSettingsCallback, this);
    runtime_.setWakeCallback(runtimeWakeCallback, this);
    runtime_.setAlertOutputCallback(alert_output_callback,
                                    alert_output_context);
}

void KitchenTimerApp::show()
{
    showTimerScreen(false);
}

void KitchenTimerApp::showTimerScreen(bool move_right)
{
    screen_.refresh(millis());
    lv_screen_load_anim(screen_.screen(),
                        move_right ? LV_SCR_LOAD_ANIM_MOVE_RIGHT
                                   : LV_SCR_LOAD_ANIM_MOVE_LEFT,
                        180,
                        0,
                        false);
}

void KitchenTimerApp::update(uint32_t now_ms)
{
    runtime_.update(now_ms);
    if (lv_screen_active() == screen_.screen()) {
        screen_.refresh(now_ms);
    }
}

bool KitchenTimerApp::nextWakeDelayMilliseconds(uint32_t now_ms,
                                                uint32_t &delay_ms) const
{
    return runtime_.nextWakeDelayMilliseconds(now_ms, delay_ms);
}

bool KitchenTimerApp::requiresAwake() const
{
    return runtime_.requiresAwake();
}

KitchenTimerState KitchenTimerApp::state() const
{
    return timer_.state();
}

uint32_t KitchenTimerApp::remainingSeconds(uint32_t now_ms) const
{
    return timer_.remainingSeconds(now_ms);
}

lv_obj_t *KitchenTimerApp::screen() const
{
    return screen_.screen();
}

void KitchenTimerApp::runtimeWakeCallback(void *context)
{
    auto *self = static_cast<KitchenTimerApp *>(context);
    if (self == nullptr) {
        return;
    }

    if (self->wake_callback_ != nullptr) {
        self->wake_callback_(self->wake_context_);
    }

    self->show();
}

void KitchenTimerApp::showSettingsCallback(void *context)
{
    auto *self = static_cast<KitchenTimerApp *>(context);
    if (self != nullptr) {
        self->showSettingsScreen();
    }
}

void KitchenTimerApp::closeSettingsCallback(void *context)
{
    auto *self = static_cast<KitchenTimerApp *>(context);
    if (self != nullptr) {
        self->showTimerScreen(true);
    }
}

void KitchenTimerApp::saveSettingsCallback(
    NotificationMode mode,
    NotificationSoundPreset sound_preset,
    void *context)
{
    auto *self = static_cast<KitchenTimerApp *>(context);
    if (self != nullptr) {
        self->saveNotificationSettings(mode, sound_preset);
    }
}

void KitchenTimerApp::showSettingsScreen()
{
    settings_screen_.show(runtime_.notificationMode(),
                          runtime_.notificationSoundPreset());
}

void KitchenTimerApp::saveNotificationSettings(
    NotificationMode mode, NotificationSoundPreset sound_preset)
{
    runtime_.setNotificationMode(mode);
    runtime_.setNotificationSoundPreset(sound_preset);
    if (!settings_store_.saveMode(NotificationTarget::KitchenTimer, mode)) {
        Serial.println("Kitchen timer notification mode: save failed");
    }
    if (!settings_store_.saveSoundPreset(NotificationTarget::KitchenTimer,
                                         sound_preset)) {
        Serial.println("Kitchen timer notification sound: save failed");
    }
}

void KitchenTimerApp::showSoundSettingsCallback(
    NotificationSoundPreset sound_preset, void *context)
{
    auto *self = static_cast<KitchenTimerApp *>(context);
    if (self != nullptr) {
        self->showSoundSettings(sound_preset);
    }
}

void KitchenTimerApp::selectSoundPresetCallback(
    NotificationSoundPreset sound_preset, void *context)
{
    auto *self = static_cast<KitchenTimerApp *>(context);
    if (self != nullptr) {
        self->settings_screen_.updateSoundPreset(sound_preset);
    }
}

void KitchenTimerApp::previewSoundCallback(
    NotificationSoundPreset sound_preset, void *context)
{
    auto *self = static_cast<KitchenTimerApp *>(context);
    if (self != nullptr) {
        self->previewSound(sound_preset);
    }
}

void KitchenTimerApp::closeSoundSettingsCallback(void *context)
{
    auto *self = static_cast<KitchenTimerApp *>(context);
    if (self != nullptr) {
        self->settings_screen_.showPending();
    }
}

void KitchenTimerApp::showSoundSettings(
    NotificationSoundPreset sound_preset)
{
    sound_settings_screen_.show(sound_preset);
}

void KitchenTimerApp::previewSound(NotificationSoundPreset sound_preset)
{
    if (preview_sound_callback_ != nullptr) {
        preview_sound_callback_(sound_preset, preview_sound_context_);
    }
}
