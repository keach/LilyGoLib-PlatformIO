#include <LilyGoLib.h>
#include <LV_Helper.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <esp_sntp.h>

// Load LilyGoLib and its member declarations before temporarily renaming the
// Arduino application entry points. This keeps the setup/loop macros scoped to
// the clock application's free functions only.
#define setup clockApplicationSetup
#define loop clockApplicationLoop
#include "main.cpp"
#undef setup
#undef loop

#include <math.h>

#include "app_hub_screen.h"
#include "kitchen_timer_app.h"

namespace {

constexpr uint32_t kTimerAlertSampleRate = 160000;
constexpr uint32_t kTimerAlertFrequency = 1000;
constexpr uint32_t kTimerAlertChunkDurationMs = 20;
constexpr size_t kTimerAlertFrameCount =
    kTimerAlertSampleRate * kTimerAlertChunkDurationMs / 1000;
constexpr size_t kTimerAlertSampleCount = kTimerAlertFrameCount * 2;
constexpr float kTimerAlertVolume = 0.35F;
constexpr uint32_t kTimerVibrationRepeatMs = 100;
constexpr uint64_t kMinimumTimerWakeupUs = 1000ULL;

int16_t timer_alert_samples[kTimerAlertSampleCount];
bool timer_alert_output_active = false;
uint32_t last_timer_vibration_ms = 0;

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

void setKitchenTimerAlertOutput(bool active, void *)
{
    timer_alert_output_active = active;
    if (!active) {
        instance.powerControl(POWER_SPEAK, false);
        return;
    }

    instance.powerControl(POWER_SPEAK, true);
    last_timer_vibration_ms = millis() - kTimerVibrationRepeatMs;
}

void serviceKitchenTimerAlertOutput(uint32_t now_ms)
{
    if (!timer_alert_output_active) {
        return;
    }

    // LilyGoLib configures the T-Watch S3 player for 160 kHz, 16-bit,
    // stereo output. Feed a short interleaved stereo chunk continuously while
    // the one-second alert phase is active instead of allocating a 640 KB
    // one-second buffer.
    instance.player.write(
        reinterpret_cast<uint8_t *>(timer_alert_samples),
        sizeof(timer_alert_samples));

    // Effect 1 is a short strong click. Re-trigger it throughout the active
    // phase so vibration is perceived for the full one-second ON interval.
    if (now_ms - last_timer_vibration_ms >= kTimerVibrationRepeatMs) {
        last_timer_vibration_ms = now_ms;
        instance.drv.setWaveform(0, 1);
        instance.drv.setWaveform(1, 0);
        instance.drv.run();
    }
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

    // A deadline can become due between the final update and sleep setup.
    // Always arm a non-zero wakeup so a 0 ms remainder cannot sleep forever.
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

    // Keep the overlay in place for a touch wake so the same physical touch
    // cannot activate a control after resume.
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
    instance.powerControl(POWER_SPEAK, false);

    app_hub_screen.create(showKitchenTimer,
                          nullptr,
                          showClockFromApps,
                          nullptr);
    kitchen_timer_app.create(showAppsFromTimer,
                             nullptr,
                             wakeForKitchenTimer,
                             nullptr,
                             setKitchenTimerAlertOutput,
                             nullptr);

    createButton(clock_screen,
                 "APPS",
                 112,
                 14,
                 58,
                 30,
                 showAppsFromClock);
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
    serviceKitchenTimerAlertOutput(now_ms);

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
