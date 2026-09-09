#include "generated_clock_main.inc"

#include <math.h>

#include "app_hub_screen.h"
#include "kitchen_timer_app.h"
#include "notification_settings_store.h"
#include "notification_volume_screen.h"
#include "pomodoro_timer_app.h"
#include "scheduled_alarm_app.h"

namespace {

constexpr uint32_t kNotificationSampleRate = 44100;
constexpr uint32_t kNotificationChunkDurationMs = 20;
constexpr size_t kNotificationFrameCount =
    kNotificationSampleRate * kNotificationChunkDurationMs / 1000;
constexpr size_t kNotificationSampleCount = kNotificationFrameCount * 2;
constexpr uint32_t kNotificationSpeakerStartupDelayMs = 20;
constexpr uint32_t kNotificationPreviewDurationMs =
    kNotificationSoundPatternDurationMs;
constexpr uint64_t kMinimumTimerWakeupUs = 1000ULL;
constexpr uint8_t kAlert1000MsEffect = 16;

int16_t notification_audio_samples[kNotificationSampleCount];
bool end_notification_sound_active = false;
bool notification_vibration_active = false;
bool notification_preview_active = false;
bool notification_audio_ready = false;
bool notification_audio_write_failure_reported = false;
bool notification_speaker_powered = false;
uint32_t notification_speaker_enabled_ms = 0;
uint32_t end_notification_sound_started_ms = 0;
uint32_t notification_preview_started_ms = 0;
NotificationSoundPreset end_notification_sound_preset =
    kDefaultNotificationSoundPreset;
NotificationSoundPreset notification_preview_preset =
    kDefaultNotificationSoundPreset;
NotificationVolumeLevel notification_master_volume_level =
    kDefaultNotificationVolumeLevel;
NotificationVolumeLevel notification_preview_volume_level =
    kDefaultNotificationVolumeLevel;
NotificationOutputState end_notification_outputs[] = {
    {NotificationTarget::KitchenTimer, kDefaultNotificationSoundPreset,
     false, false},
    {NotificationTarget::PomodoroTimer, kDefaultNotificationSoundPreset,
     false, false},
    {NotificationTarget::ScheduledAlarm, kDefaultNotificationSoundPreset,
     false, false},
};
float notification_audio_phase = 0.0F;
lv_obj_t *timer_countdown_clock_label = nullptr;
enum class ClockCountdownSource : uint8_t {
    None,
    KitchenTimer,
    PomodoroTimer,
};
ClockCountdownSource last_clock_countdown_source =
    ClockCountdownSource::None;
uint8_t last_clock_countdown_state = UINT8_MAX;
PomodoroPhase last_clock_pomodoro_phase = PomodoroPhase::Focus;
uint32_t last_clock_timer_seconds = UINT32_MAX;

AppHubScreen app_hub_screen(kBackgroundColor,
                            kPrimaryColor,
                            kAccentColor,
                            kMutedColor,
                            kButtonColor);
KitchenTimerApp kitchen_timer_app(kBackgroundColor,
                                  kPrimaryColor,
                                  kAccentColor,
                                  kMutedColor,
                                  kButtonColor);
PomodoroTimerApp pomodoro_timer_app(kBackgroundColor,
                                    kPrimaryColor,
                                    kAccentColor,
                                    kMutedColor,
                                    kButtonColor);
ScheduledAlarmApp scheduled_alarm_app(kBackgroundColor,
                                      kPrimaryColor,
                                      kAccentColor,
                                      kMutedColor,
                                      kButtonColor);
NotificationVolumeScreen notification_volume_screen(kBackgroundColor,
                                                     kPrimaryColor,
                                                     kAccentColor,
                                                     kMutedColor,
                                                     kButtonColor);
NotificationSettingsStore notification_settings_store;

void fillNotificationAudioSamples(uint16_t frequency_hz,
                                  NotificationVolumeLevel volume_level)
{
    const float volume_gain = notificationVolumeGain(volume_level);
    const float phase_step = frequency_hz == 0
                                 ? 0.0F
                                 : 2.0F * PI * frequency_hz /
                                       kNotificationSampleRate;
    for (size_t frame = 0; frame < kNotificationFrameCount; ++frame) {
        const int16_t sample = frequency_hz == 0
                                   ? 0
                                   : static_cast<int16_t>(
                                         32767.0F *
                                         sinf(notification_audio_phase) *
                                         volume_gain);
        notification_audio_samples[frame * 2] = sample;
        notification_audio_samples[frame * 2 + 1] = sample;
        notification_audio_phase += phase_step;
        if (notification_audio_phase >= 2.0F * PI) {
            notification_audio_phase -= 2.0F * PI;
        }
    }
}

bool initializeNotificationAudio()
{
#if ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(5, 0, 0)
    instance.player.end();
    instance.player.setPins(I2S_BCLK, I2S_WCLK, I2S_DOUT);
    notification_audio_ready = instance.player.begin(
        I2S_MODE_STD,
        kNotificationSampleRate,
        I2S_DATA_BIT_WIDTH_16BIT,
        I2S_SLOT_MODE_STEREO);
#else
    notification_audio_ready = instance.initAmplifier();
#endif

    Serial.printf("Notification audio: %s (%lu Hz)\n",
                  notification_audio_ready ? "ready"
                                           : "initialization failed",
                  static_cast<unsigned long>(kNotificationSampleRate));
    return notification_audio_ready;
}

bool notificationAudioRequested()
{
    return end_notification_sound_active || notification_preview_active;
}

void updateNotificationSpeakerPower(uint32_t now_ms)
{
    if (!notificationAudioRequested()) {
        if (notification_speaker_powered) {
            instance.powerControl(POWER_SPEAK, false);
            notification_speaker_powered = false;
        }
        return;
    }

    if (!notification_audio_ready && !initializeNotificationAudio()) {
        return;
    }
    if (!notification_speaker_powered) {
        instance.powerControl(POWER_SPEAK, true);
        notification_speaker_powered = true;
        notification_speaker_enabled_ms = now_ms;
        notification_audio_write_failure_reported = false;
        notification_audio_phase = 0.0F;
    }
}

void showClockFromApps(void *)
{
    last_activity_ms = millis();
    lv_screen_load_anim(clock_screen,
                        LV_SCR_LOAD_ANIM_MOVE_RIGHT,
                        180,
                        0,
                        false);
}

void showAppsFromClock(lv_event_t *)
{
    last_activity_ms = millis();
    app_hub_screen.show(false);
}

void showAppsFromTimer(void *)
{
    last_activity_ms = millis();
    app_hub_screen.show(true);
}

void showKitchenTimer(void *)
{
    last_activity_ms = millis();
    kitchen_timer_app.show();
}

void showPomodoroTimer(void *)
{
    last_activity_ms = millis();
    pomodoro_timer_app.show();
}

void showScheduledAlarm(void *)
{
    last_activity_ms = millis();
    scheduled_alarm_app.setUse24HourClock(use_24_hour_clock);
    scheduled_alarm_app.show();
}

void showAlarmVolume(void *)
{
    last_activity_ms = millis();
    notification_volume_screen.show(
        notification_master_volume_level,
        kitchen_timer_app.notificationSoundPreset());
}

void showAppsFromAlarmVolume(void *)
{
    last_activity_ms = millis();
    app_hub_screen.show(true);
}

void wakeForKitchenTimer(void *)
{
    wakeScreen();
    last_activity_ms = millis();
}

void wakeForPomodoroTimer(void *)
{
    wakeScreen();
    last_activity_ms = millis();
}

void wakeForScheduledAlarm(void *)
{
    wakeScreen();
    last_activity_ms = millis();
}

void rescheduleAlarmAfterClockAdjustment(time_t now_epoch, void *)
{
    scheduled_alarm_app.handleClockAdjusted(now_epoch);
}

size_t notificationOutputIndex(NotificationTarget target)
{
    switch (target) {
    case NotificationTarget::KitchenTimer:
        return 0;
    case NotificationTarget::PomodoroTimer:
        return 1;
    case NotificationTarget::ScheduledAlarm:
        return 2;
    }
    return 0;
}

void setEndNotificationOutput(NotificationOutputState output, void *)
{
    end_notification_outputs[notificationOutputIndex(output.target)] = output;

    bool sound_active = false;
    bool vibration_active = false;
    NotificationSoundPreset sound_preset = kDefaultNotificationSoundPreset;
    for (const NotificationOutputState &candidate :
         end_notification_outputs) {
        if (!sound_active && candidate.sound_active) {
            sound_preset = candidate.sound_preset;
        }
        sound_active = sound_active || candidate.sound_active;
        vibration_active = vibration_active || candidate.vibration_active;
    }

    const uint32_t now_ms = millis();
    const NotificationSoundPreset resolved_preset =
        resolveNotificationSoundPreset(static_cast<uint8_t>(sound_preset));
    const bool preset_changed =
        end_notification_sound_preset != resolved_preset;
    end_notification_sound_preset = resolved_preset;
    if (end_notification_sound_active != sound_active ||
        (sound_active && preset_changed)) {
        end_notification_sound_active = sound_active;
        if (end_notification_sound_active) {
            end_notification_sound_started_ms = now_ms;
            notification_audio_phase = 0.0F;
        }
        updateNotificationSpeakerPower(now_ms);
    }

    if (notification_vibration_active != vibration_active) {
        notification_vibration_active = vibration_active;
        if (notification_vibration_active) {
            instance.drv.setWaveform(0, kAlert1000MsEffect);
            instance.drv.setWaveform(1, 0);
            instance.drv.run();
        } else {
            instance.drv.stop();
        }
    }
}

void startNotificationSoundPreview(NotificationSoundPreset preset,
                                   NotificationVolumeLevel volume_level)
{
    notification_preview_preset = resolveNotificationSoundPreset(
        static_cast<uint8_t>(preset));
    notification_preview_volume_level = resolveNotificationVolumeLevel(
        static_cast<uint8_t>(volume_level));
    notification_preview_started_ms = millis();
    notification_preview_active = true;
    notification_audio_phase = 0.0F;
    updateNotificationSpeakerPower(notification_preview_started_ms);
}

void previewNotificationSound(NotificationSoundPreset preset, void *)
{
    startNotificationSoundPreview(preset,
                                  notification_master_volume_level);
}

void previewNotificationSoundAtVolume(
    NotificationSoundPreset preset,
    NotificationVolumeLevel volume_level,
    void *)
{
    startNotificationSoundPreview(preset, volume_level);
}

void saveNotificationMasterVolume(NotificationVolumeLevel level, void *)
{
    notification_master_volume_level = resolveNotificationVolumeLevel(
        static_cast<uint8_t>(level));
    if (!notification_settings_store.saveMasterVolume(
            notification_master_volume_level)) {
        Serial.println("Notification master volume: save failed");
    }
}

void serviceEndNotificationOutput(uint32_t now_ms)
{
    if (notification_preview_active &&
        now_ms - notification_preview_started_ms >=
            kNotificationPreviewDurationMs) {
        notification_preview_active = false;
        updateNotificationSpeakerPower(now_ms);
    }

    if (!notificationAudioRequested() || !notification_audio_ready ||
        !notification_speaker_powered ||
        now_ms - notification_speaker_enabled_ms <
            kNotificationSpeakerStartupDelayMs) {
        return;
    }

    const NotificationSoundPreset preset = end_notification_sound_active
                                               ? end_notification_sound_preset
                                               : notification_preview_preset;
    const uint32_t started_ms = end_notification_sound_active
                                    ? end_notification_sound_started_ms
                                    : notification_preview_started_ms;
    const NotificationVolumeLevel volume_level =
        end_notification_sound_active
            ? notification_master_volume_level
            : notification_preview_volume_level;
    const uint16_t frequency_hz = notificationSoundFrequencyAt(
        preset, now_ms - started_ms);
    fillNotificationAudioSamples(frequency_hz, volume_level);
    const size_t bytes_written = instance.player.write(
        reinterpret_cast<uint8_t *>(notification_audio_samples),
        sizeof(notification_audio_samples));
    if (bytes_written == 0 &&
        !notification_audio_write_failure_reported) {
        notification_audio_write_failure_reported = true;
        Serial.println("Notification audio: PCM write failed");
    }
}

void createTimerCountdownClockLabel()
{
    timer_countdown_clock_label = lv_label_create(clock_screen);
    lv_label_set_text(timer_countdown_clock_label, "");
    lv_obj_set_style_text_font(timer_countdown_clock_label,
                               &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(timer_countdown_clock_label,
                                lv_color_hex(kAccentColor), 0);
    lv_obj_align(timer_countdown_clock_label, LV_ALIGN_BOTTOM_MID, 0, -42);
    lv_obj_add_flag(timer_countdown_clock_label, LV_OBJ_FLAG_HIDDEN);
}

void updateTimerCountdownClockLabel(uint32_t now_ms)
{
    const KitchenTimerState kitchen_state = kitchen_timer_app.state();
    const PomodoroState pomodoro_state = pomodoro_timer_app.state();
    ClockCountdownSource source = ClockCountdownSource::None;
    uint8_t state = 0;
    PomodoroPhase pomodoro_phase = pomodoro_timer_app.phase();
    uint32_t seconds = 0;

    if (kitchen_state == KitchenTimerState::Running ||
        kitchen_state == KitchenTimerState::Paused) {
        source = ClockCountdownSource::KitchenTimer;
        state = static_cast<uint8_t>(kitchen_state);
        seconds = kitchen_timer_app.remainingSeconds(now_ms);
    } else if (pomodoro_state == PomodoroState::Running ||
               pomodoro_state == PomodoroState::Paused) {
        source = ClockCountdownSource::PomodoroTimer;
        state = static_cast<uint8_t>(pomodoro_state);
        seconds = pomodoro_timer_app.remainingSeconds(now_ms);
    }

    if (source == last_clock_countdown_source &&
        state == last_clock_countdown_state &&
        pomodoro_phase == last_clock_pomodoro_phase &&
        seconds == last_clock_timer_seconds) {
        return;
    }
    last_clock_countdown_source = source;
    last_clock_countdown_state = state;
    last_clock_pomodoro_phase = pomodoro_phase;
    last_clock_timer_seconds = seconds;

    if (source == ClockCountdownSource::None) {
        lv_obj_add_flag(timer_countdown_clock_label, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (source == ClockCountdownSource::KitchenTimer) {
        lv_label_set_text_fmt(
            timer_countdown_clock_label,
            kitchen_state == KitchenTimerState::Paused
                ? "TIMER PAUSED %02lu:%02lu"
                : "TIMER %02lu:%02lu",
            static_cast<unsigned long>(seconds / 60),
            static_cast<unsigned long>(seconds % 60));
    } else {
        const bool paused = pomodoro_state == PomodoroState::Paused;
        const char *phase = pomodoro_phase == PomodoroPhase::Focus
                                ? "FOCUS"
                                : "BREAK";
        lv_label_set_text_fmt(
            timer_countdown_clock_label,
            paused ? "%s PAUSED %02lu:%02lu" : "%s %02lu:%02lu",
            phase,
            static_cast<unsigned long>(seconds / 60),
            static_cast<unsigned long>(seconds % 60));
    }
    lv_obj_remove_flag(timer_countdown_clock_label, LV_OBJ_FLAG_HIDDEN);
}

uint64_t combinedTimerWakeupUs()
{
    uint64_t earliest_wakeup_us = nextAutomaticSyncWakeupUs();
    const uint32_t now_ms = millis();
    uint32_t delay_ms = 0;
    if (kitchen_timer_app.nextWakeDelayMilliseconds(now_ms, delay_ms)) {
        const uint64_t candidate_us =
            delay_ms == 0 ? kMinimumTimerWakeupUs
                          : static_cast<uint64_t>(delay_ms) * 1000ULL;
        if (earliest_wakeup_us == 0 || candidate_us < earliest_wakeup_us) {
            earliest_wakeup_us = candidate_us;
        }
    }
    if (pomodoro_timer_app.nextWakeDelayMilliseconds(now_ms, delay_ms)) {
        const uint64_t candidate_us =
            delay_ms == 0 ? kMinimumTimerWakeupUs
                          : static_cast<uint64_t>(delay_ms) * 1000ULL;
        if (earliest_wakeup_us == 0 || candidate_us < earliest_wakeup_us) {
            earliest_wakeup_us = candidate_us;
        }
    }
    uint32_t delay_seconds = 0;
    if (scheduled_alarm_app.nextWakeDelaySeconds(time(nullptr),
                                                 delay_seconds)) {
        const uint64_t candidate_us =
            delay_seconds == 0
                ? kMinimumTimerWakeupUs
                : static_cast<uint64_t>(delay_seconds) * 1000000ULL;
        if (earliest_wakeup_us == 0 || candidate_us < earliest_wakeup_us) {
            earliest_wakeup_us = candidate_us;
        }
    }
    return earliest_wakeup_us;
}

void enterIntegratedLightSleep()
{
    Serial.println("Entering light sleep");
    Serial.flush();

    if (!isTimeSyncBusy() && WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect(true, false);
        wifi_connection_result = WiFiConnectionResult::Unknown;
        refreshWiFiScreen();
    }

    const uint64_t timer_wakeup_us = combinedTimerWakeupUs();
    if (timer_wakeup_us != 0) {
        esp_sleep_enable_timer_wakeup(timer_wakeup_us);
    }

    const bool tilt_wake_prepared = prepareTiltWakeForLightSleep();
    const uint32_t wakeup_sources =
        WAKEUP_SRC_POWER_KEY | WAKEUP_SRC_TOUCH_PANEL |
        (tilt_wake_prepared ? WAKEUP_SRC_SENSOR : 0);
    instance.lightSleep(static_cast<WakeupSource_t>(wakeup_sources));

    const esp_sleep_wakeup_cause_t wakeup_cause =
        esp_sleep_get_wakeup_cause();
    const uint64_t ext1_wakeup_status =
        wakeup_cause == ESP_SLEEP_WAKEUP_EXT1
            ? esp_sleep_get_ext1_wakeup_status()
            : 0;
    const bool woke_by_touch =
        (ext1_wakeup_status & (1ULL << TP_INT)) != 0;
    const bool woke_by_tilt =
        tilt_wake_prepared &&
        (ext1_wakeup_status & (1ULL << SENSOR_INT)) != 0;

    restoreTiltInterruptAfterLightSleep(tilt_wake_prepared);

    if (timer_wakeup_us != 0) {
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    }

    if (wakeup_cause == ESP_SLEEP_WAKEUP_TIMER) {
        screen_off_ms = millis();
        // The ESP32 system clock and the external RTC can drift apart during
        // a long Light Sleep. Refresh the system clock before checking an
        // absolute wall-clock alarm so it cannot fire ahead of the time shown
        // by the watch. updateClock() remains suppressed while screen_on is
        // false, so this does not turn on or redraw the display.
        syncClockFromRtc();
        const uint32_t now_ms = millis();
        kitchen_timer_app.update(now_ms);
        pomodoro_timer_app.update(now_ms);
        scheduled_alarm_app.update(time(nullptr), now_ms);
        if (kitchen_timer_app.state() == KitchenTimerState::Alerting) {
            Serial.println("Light sleep wake: kitchen timer");
            return;
        }
        if (pomodoro_timer_app.state() == PomodoroState::Alerting) {
            Serial.println("Light sleep wake: pomodoro timer");
            return;
        }
        if (scheduled_alarm_app.alerting()) {
            Serial.println("Light sleep wake: scheduled alarm");
            return;
        }
        requestTimeSyncIfDue();
        Serial.println("Light sleep wake: automatic time sync");
        return;
    }

    wakeScreen(woke_by_touch);
    const char *wakeup_reason = woke_by_tilt
                                    ? "tilt"
                                    : (woke_by_touch ? "touch"
                                                     : "power button");
    Serial.printf("Light sleep wake: %s\n", wakeup_reason);
}

}  // namespace

void setup()
{
    clockApplicationSetup();

    notification_master_volume_level =
        notification_settings_store.loadMasterVolume();
    initializeNotificationAudio();
    instance.powerControl(POWER_SPEAK, false);

    app_hub_screen.create(showKitchenTimer,
                          nullptr,
                          showPomodoroTimer,
                          nullptr,
                          showScheduledAlarm,
                          nullptr,
                          showAlarmVolume,
                          nullptr,
                          showClockFromApps,
                          nullptr);
    kitchen_timer_app.create(showAppsFromTimer,
                             nullptr,
                             wakeForKitchenTimer,
                             nullptr,
                             setEndNotificationOutput,
                             nullptr,
                             previewNotificationSound,
                             nullptr);
    pomodoro_timer_app.create(showAppsFromTimer,
                              nullptr,
                              wakeForPomodoroTimer,
                              nullptr,
                              setEndNotificationOutput,
                              nullptr,
                              previewNotificationSound,
                              nullptr);
    scheduled_alarm_app.create(showAppsFromTimer,
                               nullptr,
                               wakeForScheduledAlarm,
                               nullptr,
                               setEndNotificationOutput,
                               nullptr,
                               previewNotificationSound,
                               nullptr);
    scheduled_alarm_app.setUse24HourClock(use_24_hour_clock);
    setClockAdjustedCallback(rescheduleAlarmAfterClockAdjustment, nullptr);
    notification_volume_screen.create(
        saveNotificationMasterVolume,
        nullptr,
        previewNotificationSoundAtVolume,
        nullptr,
        showAppsFromAlarmVolume,
        nullptr);

    createButton(clock_screen,
                 "APPS",
                 112,
                 14,
                 58,
                 30,
                 showAppsFromClock);
    createTimerCountdownClockLabel();
    lv_refr_now(nullptr);
}

void loop()
{
    instance.loop();
    lv_timer_handler();

    if (power_off_in_progress) {
        delay(1000);
        return;
    }

    processWiFiConnection();
    processTimeSync();
    updateTimeSyncNotification();
    const uint32_t now_ms = millis();
    kitchen_timer_app.update(now_ms);
    pomodoro_timer_app.update(now_ms);
    scheduled_alarm_app.setUse24HourClock(use_24_hour_clock);
    scheduled_alarm_app.update(time(nullptr), now_ms);
    updateTimerCountdownClockLabel(now_ms);
    serviceEndNotificationOutput(now_ms);

    if (!deploy_mode_enabled && screen_on &&
        millis() - last_activity_ms >= currentScreenTimeout()) {
        turnScreenOff();
    }

    if (!deploy_mode_enabled && !screen_on && !isRadioBusy() &&
        !kitchen_timer_app.requiresAwake() &&
        !pomodoro_timer_app.requiresAwake() &&
        !scheduled_alarm_app.requiresAwake() &&
        millis() - screen_off_ms >= light_sleep_delay_seconds * 1000) {
        enterIntegratedLightSleep();
    }

    delay(screen_on ? 2 : 20);
}
