#include "generated_clock_main.inc"

#include <math.h>

#include "app_hub_screen.h"
#include "kitchen_timer_app.h"

namespace {

constexpr uint32_t kTimerAlertSampleRate = 44100;
constexpr uint32_t kTimerAlertFrequency = 1000;
constexpr uint32_t kTimerAlertChunkDurationMs = 20;
constexpr size_t kTimerAlertFrameCount =
    kTimerAlertSampleRate * kTimerAlertChunkDurationMs / 1000;
constexpr size_t kTimerAlertSampleCount = kTimerAlertFrameCount * 2;
constexpr float kTimerAlertVolume = 0.35F;
constexpr uint32_t kTimerSpeakerStartupDelayMs = 20;
constexpr uint64_t kMinimumTimerWakeupUs = 1000ULL;
constexpr uint8_t kAlert1000MsEffect = 16;

int16_t timer_alert_samples[kTimerAlertSampleCount];
bool notification_sound_active = false;
bool notification_vibration_active = false;
bool timer_audio_ready = false;
bool timer_audio_write_failure_reported = false;
uint32_t timer_speaker_enabled_ms = 0;
lv_obj_t *timer_countdown_clock_label = nullptr;
KitchenTimerState last_clock_timer_state = KitchenTimerState::Idle;
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

void initializeTimerAlertSamples()
{
    for (size_t frame = 0; frame < kTimerAlertFrameCount; ++frame) {
        const float phase =
            2.0F * PI * kTimerAlertFrequency * frame /
            kTimerAlertSampleRate;
        const int16_t sample = static_cast<int16_t>(
            32767.0F * sinf(phase) * kTimerAlertVolume);
        timer_alert_samples[frame * 2] = sample;
        timer_alert_samples[frame * 2 + 1] = sample;
    }
}

bool initializeTimerAudio()
{
#if ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(5, 0, 0)
    instance.player.end();
    instance.player.setPins(I2S_BCLK, I2S_WCLK, I2S_DOUT);
    timer_audio_ready = instance.player.begin(
        I2S_MODE_STD,
        kTimerAlertSampleRate,
        I2S_DATA_BIT_WIDTH_16BIT,
        I2S_SLOT_MODE_STEREO);
#else
    timer_audio_ready = instance.initAmplifier();
#endif

    Serial.printf("Kitchen timer audio: %s (%lu Hz)\n",
                  timer_audio_ready ? "ready" : "initialization failed",
                  static_cast<unsigned long>(kTimerAlertSampleRate));
    return timer_audio_ready;
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

void wakeForKitchenTimer(void *)
{
    wakeScreen();
    last_activity_ms = millis();
}

void setEndNotificationOutput(NotificationOutputState output, void *)
{
    if (notification_sound_active != output.sound_active) {
        notification_sound_active = output.sound_active;
        if (!notification_sound_active) {
            instance.powerControl(POWER_SPEAK, false);
        } else {
            const uint32_t now_ms = millis();
            if (!timer_audio_ready) {
                initializeTimerAudio();
            }
            if (timer_audio_ready) {
                instance.powerControl(POWER_SPEAK, true);
                timer_speaker_enabled_ms = now_ms;
                timer_audio_write_failure_reported = false;
            }
        }
    }

    if (notification_vibration_active != output.vibration_active) {
        notification_vibration_active = output.vibration_active;
        if (notification_vibration_active) {
            instance.drv.setWaveform(0, kAlert1000MsEffect);
            instance.drv.setWaveform(1, 0);
            instance.drv.run();
        } else {
            instance.drv.stop();
        }
    }
}

void serviceEndNotificationOutput(uint32_t now_ms)
{
    if (!notification_sound_active) {
        return;
    }

    if (timer_audio_ready &&
        now_ms - timer_speaker_enabled_ms >= kTimerSpeakerStartupDelayMs) {
        const size_t bytes_written = instance.player.write(
            reinterpret_cast<uint8_t *>(timer_alert_samples),
            sizeof(timer_alert_samples));
        if (bytes_written == 0 && !timer_audio_write_failure_reported) {
            timer_audio_write_failure_reported = true;
            Serial.println("Kitchen timer audio: PCM write failed");
        }
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
    const KitchenTimerState state = kitchen_timer_app.state();
    const uint32_t seconds = kitchen_timer_app.remainingSeconds(now_ms);
    if (state == last_clock_timer_state &&
        seconds == last_clock_timer_seconds) {
        return;
    }
    last_clock_timer_state = state;
    last_clock_timer_seconds = seconds;

    if (state != KitchenTimerState::Running &&
        state != KitchenTimerState::Paused) {
        lv_obj_add_flag(timer_countdown_clock_label, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_label_set_text_fmt(
        timer_countdown_clock_label,
        state == KitchenTimerState::Paused ? "TIMER PAUSED %02lu:%02lu"
                                           : "TIMER %02lu:%02lu",
        static_cast<unsigned long>(seconds / 60),
        static_cast<unsigned long>(seconds % 60));
    lv_obj_remove_flag(timer_countdown_clock_label, LV_OBJ_FLAG_HIDDEN);
}

uint64_t combinedTimerWakeupUs(bool &kitchen_timer_wakeup)
{
    kitchen_timer_wakeup = false;

    const uint64_t automatic_sync_wakeup_us = nextAutomaticSyncWakeupUs();
    uint32_t kitchen_timer_delay_ms = 0;
    if (!kitchen_timer_app.nextWakeDelayMilliseconds(
            millis(), kitchen_timer_delay_ms)) {
        return automatic_sync_wakeup_us;
    }

    const uint64_t kitchen_timer_wakeup_us =
        kitchen_timer_delay_ms == 0
            ? kMinimumTimerWakeupUs
            : static_cast<uint64_t>(kitchen_timer_delay_ms) * 1000ULL;
    if (automatic_sync_wakeup_us == 0 ||
        kitchen_timer_wakeup_us <= automatic_sync_wakeup_us) {
        kitchen_timer_wakeup = true;
        return kitchen_timer_wakeup_us;
    }
    return automatic_sync_wakeup_us;
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

    bool kitchen_timer_wakeup = false;
    const uint64_t timer_wakeup_us =
        combinedTimerWakeupUs(kitchen_timer_wakeup);
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
        kitchen_timer_app.update(millis());
        if (kitchen_timer_wakeup &&
            kitchen_timer_app.state() == KitchenTimerState::Alerting) {
            Serial.println("Light sleep wake: kitchen timer");
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

    initializeTimerAlertSamples();
    initializeTimerAudio();
    instance.powerControl(POWER_SPEAK, false);

    app_hub_screen.create(showKitchenTimer,
                          nullptr,
                          showClockFromApps,
                          nullptr);
    kitchen_timer_app.create(showAppsFromTimer,
                             nullptr,
                             wakeForKitchenTimer,
                             nullptr,
                             setEndNotificationOutput,
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
    updateTimerCountdownClockLabel(now_ms);
    serviceEndNotificationOutput(now_ms);

    if (!deploy_mode_enabled && screen_on &&
        millis() - last_activity_ms >= currentScreenTimeout()) {
        turnScreenOff();
    }

    if (!deploy_mode_enabled && !screen_on && !isRadioBusy() &&
        !kitchen_timer_app.requiresAwake() &&
        millis() - screen_off_ms >= light_sleep_delay_seconds * 1000) {
        enterIntegratedLightSleep();
    }

    delay(screen_on ? 2 : 20);
}
