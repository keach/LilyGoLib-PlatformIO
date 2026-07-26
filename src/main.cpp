/**
 * T-Watch S3 clock face with RTC adjustment and NTP synchronization.
 */
#include <LilyGoLib.h>
#include <LV_Helper.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <esp_sntp.h>

#include <stdint.h>
#include <time.h>

#include "wifi_credentials_types.h"

LV_FONT_DECLARE(lv_font_watch_digits_36);

#if __has_include("wifi_credentials.h")
#include "wifi_credentials.h"
#else
inline constexpr const WiFiCredential *kWiFiCredentials = nullptr;
inline constexpr size_t kWiFiCredentialCount = 0;
#endif

namespace {

constexpr uint32_t kBackgroundColor = 0x101820;
constexpr uint32_t kPrimaryColor = 0xF2F5F7;
constexpr uint32_t kAccentColor = 0x55C2FF;
constexpr uint32_t kMutedColor = 0x94A3AD;
constexpr uint32_t kButtonColor = 0x24323D;
constexpr uint32_t kLowBatteryColor = 0xFF6B6B;
constexpr uint32_t kDefaultClockScreenTimeoutSeconds = 15;
constexpr uint32_t kDefaultSettingsScreenTimeoutSeconds = 60;
constexpr uint32_t kDefaultLightSleepDelaySeconds = 5;
constexpr uint32_t kBatteryUpdateIntervalMs = 10 * 1000;
constexpr uint32_t kWiFiConnectTimeoutMs = 10 * 1000;
constexpr uint32_t kNtpSyncTimeoutMs = 15 * 1000;
constexpr uint32_t kWiFiRetryIntervalMs = 15 * 60 * 1000;
constexpr uint32_t kSuccessNotificationMs = 3 * 1000;
constexpr uint32_t kWarningNotificationMs = 8 * 1000;
constexpr uint32_t kStartupScreenMinimumMs = 3000;
constexpr uint32_t kShutdownScreenDurationMs = 3000;
constexpr time_t kNtpSyncIntervalSeconds = 24 * 60 * 60;
constexpr time_t kMinimumValidEpoch = 1577836800;  // 2020-01-01 UTC
constexpr uint8_t kDefaultBrightness = 180;
constexpr uint8_t kMinimumBrightness = 20;
constexpr uint8_t kMaximumBrightness = 255;
constexpr uint8_t kBrightnessStep = 10;
constexpr uint8_t kBma423LevelTriggeredInterrupt = 0;
constexpr uint8_t kBma423ActiveLowInterrupt = 0;
constexpr int kMinimumYear = 2000;
constexpr int kMaximumYear = 2099;

constexpr uint32_t kClockTimeoutOptions[] = {10, 15, 30, 60, 120};
constexpr uint32_t kSettingsTimeoutOptions[] = {30, 60, 120, 300};
constexpr uint32_t kLightSleepDelayOptions[] = {5, 10, 30, 60};

const char *const kTimeZone = "JST-9";
const char *const kNtpServer1 = "pool.ntp.org";
const char *const kNtpServer2 = "time.google.com";
const char *const kNtpServer3 = "ntp.nict.jp";

const char *const kWeekdays[] = {
    "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT",
};

enum class SettingField : uint8_t {
    Year,
    Month,
    Day,
    Hour,
    Minute,
    Second,
    Count,
};

enum class TimeSyncState : uint8_t {
    Idle,
    Connecting,
    WaitingForNtp,
};

enum class TimeSyncResult : uint8_t {
    Never,
    Success,
    Failed,
};

enum class WiFiConnectionResult : uint8_t {
    Unknown,
    Connected,
    Failed,
};

const char *const kFieldNames[] = {
    "YEAR", "MONTH", "DAY", "HOUR", "MINUTE", "SECOND",
};

lv_obj_t *clock_screen;
lv_obj_t *settings_hub_screen;
lv_obj_t *power_display_screen;
lv_obj_t *timeout_settings_screen;
lv_obj_t *reset_settings_screen;
lv_obj_t *wifi_ntp_screen;
lv_obj_t *date_time_screen;
lv_obj_t *brightness_screen;
lv_obj_t *time_sync_screen;
lv_obj_t *wifi_screen;
lv_obj_t *startup_screen;
lv_obj_t *power_off_screen;
lv_obj_t *hour_label;
lv_obj_t *minute_label;
lv_obj_t *second_label;
lv_obj_t *meridiem_label;
lv_obj_t *date_line_label;
lv_obj_t *battery_label;
lv_obj_t *deploy_mode_clock_label;
lv_obj_t *deploy_mode_button_label;
lv_obj_t *deploy_mode_status_label;
lv_obj_t *clock_format_button_label;
lv_obj_t *clock_timeout_button_label;
lv_obj_t *settings_timeout_button_label;
lv_obj_t *light_sleep_delay_button_label;
lv_obj_t *timeout_settings_status_label;
lv_obj_t *reset_settings_status_label;
lv_obj_t *time_sync_label;
lv_obj_t *settings_status_label;
lv_obj_t *brightness_slider;
lv_obj_t *brightness_value_label;
lv_obj_t *brightness_status_label;
lv_obj_t *auto_sync_button_label;
lv_obj_t *sync_now_button;
lv_obj_t *sync_screen_status_label;
lv_obj_t *last_sync_label;
lv_obj_t *wifi_status_label;
lv_obj_t *wifi_network_label;
lv_obj_t *wifi_connect_button;
lv_obj_t *wifi_connect_button_label;
lv_obj_t *wifi_disconnect_button;
lv_obj_t *wake_overlay;
lv_obj_t *field_buttons[static_cast<uint8_t>(SettingField::Count)];
lv_obj_t *field_labels[static_cast<uint8_t>(SettingField::Count)];

SettingField selected_field = SettingField::Year;
int setting_year = 2026;
int setting_month = 1;
int setting_day = 1;
int setting_hour = 0;
int setting_minute = 0;
int setting_second = 0;
int last_second = -1;
uint32_t last_activity_ms = 0;
uint32_t screen_off_ms = 0;
bool screen_on = true;
bool deploy_mode_enabled = false;
bool use_24_hour_clock = true;
uint32_t clock_screen_timeout_seconds =
    kDefaultClockScreenTimeoutSeconds;
uint32_t settings_screen_timeout_seconds =
    kDefaultSettingsScreenTimeoutSeconds;
uint32_t light_sleep_delay_seconds =
    kDefaultLightSleepDelaySeconds;
uint32_t pending_clock_screen_timeout_seconds =
    kDefaultClockScreenTimeoutSeconds;
uint32_t pending_settings_screen_timeout_seconds =
    kDefaultSettingsScreenTimeoutSeconds;
uint32_t pending_light_sleep_delay_seconds =
    kDefaultLightSleepDelaySeconds;
uint8_t active_brightness = kDefaultBrightness;
uint8_t pending_brightness = kDefaultBrightness;
TimeSyncState time_sync_state = TimeSyncState::Idle;
TimeSyncResult last_time_sync_result = TimeSyncResult::Never;
WiFiConnectionResult wifi_connection_result =
    WiFiConnectionResult::Unknown;
size_t wifi_credential_index = 0;
size_t wifi_connection_credential_index = 0;
uint32_t time_sync_state_started_ms = 0;
uint32_t wifi_connection_started_ms = 0;
uint32_t wifi_retry_not_before_ms = 0;
uint32_t time_sync_notification_hide_ms = 0;
time_t last_ntp_sync_epoch = 0;
volatile bool ntp_sync_received = false;
bool ntp_config_warning_shown = false;
bool automatic_time_sync_enabled = true;
bool wifi_connection_in_progress = false;
bool time_sync_owns_wifi_connection = false;
bool power_off_in_progress = false;
bool tilt_wake_available = false;

void syncClockFromRtc();
void updateBatteryStatus(lv_timer_t *);
void requestTimeSyncIfDue();
bool isTimeSyncBusy();
bool isWiFiConnectionBusy();
bool isRadioBusy();
void refreshTimeSyncScreen();
void refreshWiFiScreen();
void refreshPowerDisplayScreen();
void refreshTimeoutSettingsScreen();
void updateClock(lv_timer_t *);
uint64_t nextAutomaticSyncWakeupUs();

bool isValidDateTime(const struct tm &timeinfo)
{
    return timeinfo.tm_year >= 100 &&
           timeinfo.tm_mon >= 0 && timeinfo.tm_mon < 12 &&
           timeinfo.tm_mday >= 1 && timeinfo.tm_mday <= 31 &&
           timeinfo.tm_wday >= 0 && timeinfo.tm_wday < 7 &&
           timeinfo.tm_hour >= 0 && timeinfo.tm_hour < 24 &&
           timeinfo.tm_min >= 0 && timeinfo.tm_min < 60 &&
           timeinfo.tm_sec >= 0 && timeinfo.tm_sec < 60;
}

bool isLeapYear(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int daysInMonth(int year, int month)
{
    static const uint8_t days[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
    };
    return month == 2 && isLeapYear(year) ? 29 : days[month - 1];
}

int wrapValue(int value, int minimum, int maximum)
{
    if (value < minimum) {
        return maximum;
    }
    if (value > maximum) {
        return minimum;
    }
    return value;
}

void markUserActivity(lv_event_t *)
{
    last_activity_ms = millis();
}

uint8_t currentDisplayBrightness()
{
    return lv_screen_active() == brightness_screen
               ? pending_brightness
               : active_brightness;
}

void wakeScreen(bool keep_wake_overlay = false)
{
    last_activity_ms = millis();
    if (screen_on) {
        return;
    }

    screen_on = true;
    if (!keep_wake_overlay) {
        lv_obj_add_flag(wake_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    instance.setBrightness(currentDisplayBrightness());
    syncClockFromRtc();
    updateBatteryStatus(nullptr);
    requestTimeSyncIfDue();
    Serial.println("Display awake");
}

void turnScreenOff()
{
    if (!screen_on) {
        return;
    }

    // The overlay consumes the first wake-up touch so it cannot accidentally
    // activate a control beneath it.
    lv_obj_remove_flag(wake_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(wake_overlay);
    screen_on = false;
    screen_off_ms = millis();
    instance.setBrightness(0);
    Serial.println("Display off");
}

void wakeOverlayCallback(lv_event_t *)
{
    if (screen_on) {
        // A touch that woke the CPU from light sleep is consumed here before
        // normal controls are made available again.
        last_activity_ms = millis();
        lv_obj_add_flag(wake_overlay, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    wakeScreen();
}

void configureTiltWake()
{
    if ((instance.getDeviceProbe() & HW_BMA423_ONLINE) == 0) {
        Serial.println("Wrist wake unavailable: BMA423 not detected");
        return;
    }

    // LilyGoLib enables every BMA423 feature interrupt during startup. Keep
    // only tilt mapped so unrelated motion events cannot wake the display.
    instance.sensor.disablePedometerIRQ();
    instance.sensor.disableWakeupIRQ();
    instance.sensor.disableAnyNoMotionIRQ();
    instance.sensor.disableActivityIRQ();
    instance.sensor.enableAccelerometer();

    const bool feature_enabled =
        instance.sensor.enableFeature(SensorBMA423::FEATURE_TILT, true);
    const bool interrupt_enabled = instance.sensor.enableTiltIRQ();
    instance.sensor.readIrqStatus();
    tilt_wake_available = feature_enabled && interrupt_enabled;
    Serial.printf("Wrist wake: %s\n",
                  tilt_wake_available ? "enabled" : "initialization failed");
}

bool prepareTiltWakeForLightSleep()
{
    if (!tilt_wake_available) {
        return false;
    }

    // Power and touch wake pins are active-low. Temporarily make the BMA423
    // interrupt active-low as well so all three can share EXT1 ANY_LOW.
    instance.sensor.readIrqStatus();
    pinMode(SENSOR_INT, INPUT_PULLUP);
    const bool configured = instance.sensor.configInterrupt(
        kBma423LevelTriggeredInterrupt,
        kBma423ActiveLowInterrupt);
    if (!configured) {
        pinMode(SENSOR_INT, INPUT_PULLDOWN);
        Serial.println("Wrist wake: failed to prepare sensor interrupt");
    }
    return configured;
}

void restoreTiltInterruptAfterLightSleep(bool tilt_wake_prepared)
{
    if (!tilt_wake_prepared) {
        return;
    }

    instance.sensor.readIrqStatus();
    instance.sensor.configInterrupt();
    pinMode(SENSOR_INT, INPUT_PULLDOWN);
}

void enterLightSleep()
{
    Serial.println("Entering light sleep");
    Serial.flush();

    if (!isTimeSyncBusy() && WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect(true, false);
        wifi_connection_result = WiFiConnectionResult::Unknown;
        refreshWiFiScreen();
    }

    const uint64_t timer_wakeup_us = nextAutomaticSyncWakeupUs();
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
        requestTimeSyncIfDue();
        Serial.println("Light sleep wake: automatic time sync");
        return;
    }

    // Keep the overlay in place for a touch wake so the same physical touch
    // cannot activate the SET button or another control after resume.
    wakeScreen(woke_by_touch);
    const char *wakeup_reason = woke_by_tilt
                                    ? "tilt"
                                    : (woke_by_touch ? "touch"
                                                     : "power button");
    Serial.printf("Light sleep wake: %s\n", wakeup_reason);
}

void powerOff()
{
    if (power_off_in_progress) {
        return;
    }

    power_off_in_progress = true;
    Serial.println("Power key long press: shutting down");

    WiFi.disconnect(true, false);
    lv_obj_add_flag(wake_overlay, LV_OBJ_FLAG_HIDDEN);
    screen_on = true;
    lv_screen_load(power_off_screen);
    instance.setBrightness(active_brightness);
    lv_refr_now(nullptr);

    // Keep the confirmation visible briefly before the AXP2101 cuts power.
    delay(kShutdownScreenDurationMs);
    Serial.flush();
    instance.pmu.shutdown();
}

void deviceEventCallback(DeviceEvent_t event, void *params, void *)
{
    if (event == SENSOR_EVENT) {
        if (instance.getSensorEventType(params) == SENSOR_TILT_DETECTED) {
            Serial.println("Sensor event: tilt detected");
            if (!screen_on && !power_off_in_progress) {
                wakeScreen();
            }
        }
        return;
    }

    if (event != POWER_EVENT) {
        return;
    }

    const PMUEventType_t power_event = instance.getPMUEventType(params);
    if (power_event == PMU_EVENT_KEY_CLICKED) {
        wakeScreen();
        return;
    }

    if (power_event == PMU_EVENT_KEY_LONG_PRESSED) {
        powerOff();
        return;
    }

    if (power_event == PMU_EVENT_USBC_INSERT ||
        power_event == PMU_EVENT_USBC_REMOVE ||
        power_event == PMU_EVENT_CHARGE_STARTED ||
        power_event == PMU_EVENT_CHARGE_FINISH ||
        power_event == PMU_EVENT_BATTERY_INSERT ||
        power_event == PMU_EVENT_BATTERY_REMOVE) {
        updateBatteryStatus(nullptr);
    }
}

uint32_t currentScreenTimeout()
{
    const uint32_t timeout_seconds =
        lv_screen_active() == clock_screen
            ? clock_screen_timeout_seconds
            : settings_screen_timeout_seconds;
    return timeout_seconds * 1000;
}

void styleScreen(lv_obj_t *screen)
{
    lv_obj_set_style_bg_color(screen, lv_color_hex(kBackgroundColor), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(kPrimaryColor), 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
}

void styleButton(lv_obj_t *button)
{
    lv_obj_set_style_bg_color(button, lv_color_hex(kButtonColor), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_radius(button, 8, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_pad_all(button, 0, 0);
}

lv_obj_t *createButton(lv_obj_t *parent, const char *text,
                       int x, int y, int width, int height,
                       lv_event_cb_t callback)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    styleButton(button);
    lv_obj_add_event_cb(button, markUserActivity, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_center(label);
    return button;
}

void showClockError(const char *message)
{
    lv_label_set_text(hour_label, "--");
    lv_label_set_text(minute_label, "--");
    lv_label_set_text(second_label, "--");
    lv_label_set_text(date_line_label, message);
    lv_obj_add_flag(meridiem_label, LV_OBJ_FLAG_HIDDEN);
}

void updateClock(lv_timer_t *)
{
    if (!screen_on || lv_screen_active() != clock_screen) {
        return;
    }
    if ((instance.getDeviceProbe() & HW_RTC_ONLINE) == 0) {
        showClockError("RTC NOT AVAILABLE");
        return;
    }

    struct tm timeinfo = {};
    instance.rtc.getDateTime(&timeinfo);

    if (!isValidDateTime(timeinfo)) {
        showClockError("SET DATE AND TIME");
        return;
    }

    if (timeinfo.tm_sec == last_second) {
        return;
    }
    last_second = timeinfo.tm_sec;

    int display_hour = timeinfo.tm_hour;
    if (use_24_hour_clock) {
        lv_obj_add_flag(meridiem_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(meridiem_label,
                          timeinfo.tm_hour < 12 ? "AM" : "PM");
        lv_obj_remove_flag(meridiem_label, LV_OBJ_FLAG_HIDDEN);
        display_hour %= 12;
        if (display_hour == 0) {
            display_hour = 12;
        }
    }

    lv_label_set_text_fmt(hour_label, "%02d", display_hour);
    lv_label_set_text_fmt(minute_label, "%02d", timeinfo.tm_min);
    lv_label_set_text_fmt(second_label, "%02d", timeinfo.tm_sec);
    lv_label_set_text_fmt(date_line_label, "%04d.%02d.%02d. %s",
                          timeinfo.tm_year + 1900, timeinfo.tm_mon + 1,
                          timeinfo.tm_mday, kWeekdays[timeinfo.tm_wday]);
}

const char *batterySymbol(int percent)
{
    if (percent >= 90) {
        return LV_SYMBOL_BATTERY_FULL;
    }
    if (percent >= 65) {
        return LV_SYMBOL_BATTERY_3;
    }
    if (percent >= 40) {
        return LV_SYMBOL_BATTERY_2;
    }
    if (percent >= 15) {
        return LV_SYMBOL_BATTERY_1;
    }
    return LV_SYMBOL_BATTERY_EMPTY;
}

void updateBatteryStatus(lv_timer_t *)
{
    if (!screen_on || lv_screen_active() != clock_screen) {
        return;
    }

    if ((instance.getDeviceProbe() & HW_PMU_ONLINE) == 0) {
        lv_label_set_text(battery_label, "N/A");
        lv_obj_set_style_text_color(battery_label, lv_color_hex(kMutedColor), 0);
        return;
    }

    const int percent = instance.pmu.getBatteryPercent();
    const bool battery_connected = instance.pmu.isBatteryConnect();
    const bool usb_connected = instance.pmu.isVbusIn();
    const bool charging = instance.pmu.isCharging();

    if (!battery_connected || percent < 0 || percent > 100) {
        lv_label_set_text(battery_label,
                          usb_connected ? LV_SYMBOL_USB : "N/A");
        lv_obj_set_style_text_color(battery_label, lv_color_hex(kMutedColor), 0);
        return;
    }

    if (charging) {
        lv_label_set_text_fmt(battery_label, LV_SYMBOL_CHARGE " %d%%", percent);
        lv_obj_set_style_text_color(battery_label, lv_color_hex(kAccentColor), 0);
    } else if (usb_connected && percent >= 100) {
        lv_label_set_text_fmt(battery_label,
                              LV_SYMBOL_BATTERY_FULL " %d%%", percent);
        lv_obj_set_style_text_color(battery_label, lv_color_hex(kAccentColor), 0);
    } else if (usb_connected) {
        lv_label_set_text_fmt(battery_label, LV_SYMBOL_USB " %d%%", percent);
        lv_obj_set_style_text_color(battery_label, lv_color_hex(kMutedColor), 0);
    } else {
        lv_label_set_text_fmt(battery_label, "%s %d%%",
                              batterySymbol(percent), percent);
        const uint32_t color = percent < 20 ? kLowBatteryColor : kMutedColor;
        lv_obj_set_style_text_color(battery_label, lv_color_hex(color), 0);
    }
}

void hideTimeSyncNotification()
{
    if (time_sync_label == nullptr) {
        return;
    }
    time_sync_notification_hide_ms = 0;
    lv_obj_add_flag(time_sync_label, LV_OBJ_FLAG_HIDDEN);
}

void setTimeSyncStatus(const char *status, uint32_t color = kMutedColor,
                       uint32_t duration_ms = 0)
{
    if (time_sync_label == nullptr) {
        return;
    }
    lv_label_set_text(time_sync_label, status);
    lv_obj_set_style_text_color(time_sync_label, lv_color_hex(color), 0);
    lv_obj_remove_flag(time_sync_label, LV_OBJ_FLAG_HIDDEN);
    time_sync_notification_hide_ms =
        duration_ms == 0 ? 0 : millis() + duration_ms;
}

void updateTimeSyncNotification()
{
    if (time_sync_notification_hide_ms != 0 &&
        static_cast<int32_t>(millis() - time_sync_notification_hide_ms) >= 0) {
        hideTimeSyncNotification();
    }
}

bool isTimeSyncBusy()
{
    return time_sync_state != TimeSyncState::Idle;
}

bool isWiFiConnectionBusy()
{
    return wifi_connection_in_progress;
}

bool isRadioBusy()
{
    return isTimeSyncBusy() || isWiFiConnectionBusy();
}

bool isNtpSyncDue()
{
    const time_t now = time(nullptr);
    if (now < kMinimumValidEpoch || last_ntp_sync_epoch < kMinimumValidEpoch) {
        return true;
    }
    if (now < last_ntp_sync_epoch) {
        return true;
    }
    return now - last_ntp_sync_epoch >= kNtpSyncIntervalSeconds;
}

uint64_t nextAutomaticSyncWakeupUs()
{
    if (!automatic_time_sync_enabled || kWiFiCredentialCount == 0 ||
        isRadioBusy()) {
        return 0;
    }

    if (isNtpSyncDue()) {
        if (wifi_retry_not_before_ms != 0) {
            const int32_t remaining_ms = static_cast<int32_t>(
                wifi_retry_not_before_ms - millis());
            if (remaining_ms > 0) {
                return static_cast<uint64_t>(remaining_ms) * 1000ULL;
            }
        }
        // The main loop normally starts a due synchronization before sleep.
        // Keep a short timer as a fallback if the state changes at the edge.
        return 1000ULL * 1000ULL;
    }

    const time_t now = time(nullptr);
    const time_t next_sync = last_ntp_sync_epoch + kNtpSyncIntervalSeconds;
    if (now < kMinimumValidEpoch || next_sync <= now) {
        return 1000ULL * 1000ULL;
    }
    return static_cast<uint64_t>(next_sync - now) * 1000ULL * 1000ULL;
}

void refreshTimeSyncScreen()
{
    if (time_sync_screen == nullptr || auto_sync_button_label == nullptr ||
        sync_screen_status_label == nullptr || last_sync_label == nullptr ||
        sync_now_button == nullptr) {
        return;
    }

    lv_label_set_text(auto_sync_button_label,
                      automatic_time_sync_enabled
                          ? "AUTO SYNC: ON"
                          : "AUTO SYNC: OFF");

    const char *status = "NEVER SYNCED";
    uint32_t status_color = kMutedColor;
    if (time_sync_state == TimeSyncState::Connecting) {
        status = "CONNECTING";
        status_color = kAccentColor;
    } else if (time_sync_state == TimeSyncState::WaitingForNtp) {
        status = "SYNCING";
        status_color = kAccentColor;
    } else if (last_time_sync_result == TimeSyncResult::Success) {
        status = "SYNCED";
        status_color = kAccentColor;
    } else if (last_time_sync_result == TimeSyncResult::Failed) {
        status = "FAILED";
        status_color = kLowBatteryColor;
    }
    lv_label_set_text_fmt(sync_screen_status_label, "STATUS: %s", status);
    lv_obj_set_style_text_color(sync_screen_status_label,
                                lv_color_hex(status_color), 0);

    if (last_ntp_sync_epoch < kMinimumValidEpoch) {
        lv_label_set_text(last_sync_label, "LAST SYNC: NEVER");
    } else {
        struct tm local_time = {};
        localtime_r(&last_ntp_sync_epoch, &local_time);
        lv_label_set_text_fmt(last_sync_label,
                              "LAST: %04d-%02d-%02d %02d:%02d",
                              local_time.tm_year + 1900,
                              local_time.tm_mon + 1,
                              local_time.tm_mday,
                              local_time.tm_hour,
                              local_time.tm_min);
    }

    if (isRadioBusy()) {
        lv_obj_add_state(sync_now_button, LV_STATE_DISABLED);
    } else {
        lv_obj_remove_state(sync_now_button, LV_STATE_DISABLED);
    }
}

void refreshWiFiScreen()
{
    if (wifi_screen == nullptr || wifi_status_label == nullptr ||
        wifi_network_label == nullptr || wifi_connect_button == nullptr ||
        wifi_connect_button_label == nullptr ||
        wifi_disconnect_button == nullptr) {
        return;
    }

    const bool connected = WiFi.status() == WL_CONNECTED;
    const bool connecting = isWiFiConnectionBusy() ||
                            time_sync_state == TimeSyncState::Connecting;
    const char *status = "OFF";
    uint32_t status_color = kMutedColor;
    if (connected) {
        status = "CONNECTED";
        status_color = kAccentColor;
    } else if (connecting) {
        status = "CONNECTING";
        status_color = kAccentColor;
    } else if (wifi_connection_result == WiFiConnectionResult::Failed) {
        status = "FAILED";
        status_color = kLowBatteryColor;
    }
    lv_label_set_text_fmt(wifi_status_label, "STATUS: %s", status);
    lv_obj_set_style_text_color(wifi_status_label,
                                lv_color_hex(status_color), 0);

    if (connected) {
        lv_label_set_text_fmt(wifi_network_label, "NETWORK: %s",
                              WiFi.SSID().c_str());
    } else if (isWiFiConnectionBusy() &&
               wifi_connection_credential_index < kWiFiCredentialCount) {
        const WiFiCredential &credential =
            kWiFiCredentials[wifi_connection_credential_index];
        lv_label_set_text_fmt(wifi_network_label, "NETWORK: %s",
                              credential.ssid == nullptr
                                  ? "UNKNOWN"
                                  : credential.ssid);
    } else {
        lv_label_set_text(wifi_network_label, "NETWORK: --");
    }

    lv_label_set_text(wifi_connect_button_label,
                      connected ? "RECONNECT" : "CONNECT");
    if (isRadioBusy()) {
        lv_obj_add_state(wifi_connect_button, LV_STATE_DISABLED);
    } else {
        lv_obj_remove_state(wifi_connect_button, LV_STATE_DISABLED);
    }

    if (isTimeSyncBusy() || (!connected && !isWiFiConnectionBusy())) {
        lv_obj_add_state(wifi_disconnect_button, LV_STATE_DISABLED);
    } else {
        lv_obj_remove_state(wifi_disconnect_button, LV_STATE_DISABLED);
    }
}

template <size_t N>
bool isTimeoutOption(uint32_t value, const uint32_t (&options)[N])
{
    for (const uint32_t option : options) {
        if (value == option) {
            return true;
        }
    }
    return false;
}

template <size_t N>
uint32_t nextTimeoutOption(uint32_t value,
                           const uint32_t (&options)[N])
{
    for (size_t index = 0; index < N; ++index) {
        if (value == options[index]) {
            return options[(index + 1) % N];
        }
    }
    return options[0];
}

void loadPowerDisplaySettings()
{
    Preferences preferences;
    if (!preferences.begin("power_display", true)) {
        Serial.println("Unable to read power and display settings");
        return;
    }

    const uint32_t stored_clock_timeout = preferences.getUInt(
        "clock_sec", kDefaultClockScreenTimeoutSeconds);
    const uint32_t stored_settings_timeout = preferences.getUInt(
        "settings_sec", kDefaultSettingsScreenTimeoutSeconds);
    const uint32_t stored_sleep_delay = preferences.getUInt(
        "sleep_sec", kDefaultLightSleepDelaySeconds);
    use_24_hour_clock = preferences.getBool("format_24h", true);
    preferences.end();

    clock_screen_timeout_seconds =
        isTimeoutOption(stored_clock_timeout, kClockTimeoutOptions)
            ? stored_clock_timeout
            : kDefaultClockScreenTimeoutSeconds;
    settings_screen_timeout_seconds =
        isTimeoutOption(stored_settings_timeout, kSettingsTimeoutOptions)
            ? stored_settings_timeout
            : kDefaultSettingsScreenTimeoutSeconds;
    light_sleep_delay_seconds =
        isTimeoutOption(stored_sleep_delay, kLightSleepDelayOptions)
            ? stored_sleep_delay
            : kDefaultLightSleepDelaySeconds;
}

bool savePowerDisplaySettings()
{
    Preferences preferences;
    if (!preferences.begin("power_display", false)) {
        Serial.println("Unable to save power and display settings");
        return false;
    }

    const bool clock_saved =
        preferences.putUInt("clock_sec", clock_screen_timeout_seconds) > 0;
    const bool settings_saved =
        preferences.putUInt("settings_sec",
                            settings_screen_timeout_seconds) > 0;
    const bool sleep_saved =
        preferences.putUInt("sleep_sec", light_sleep_delay_seconds) > 0;
    const bool format_saved =
        preferences.putBool("format_24h", use_24_hour_clock) > 0;
    preferences.end();
    return clock_saved && settings_saved && sleep_saved && format_saved;
}

void loadBrightnessSetting()
{
    Preferences preferences;
    if (!preferences.begin("clock_config", true)) {
        Serial.println("Unable to read brightness setting");
        return;
    }
    const uint8_t stored_brightness =
        preferences.getUChar("brightness", kDefaultBrightness);
    preferences.end();

    active_brightness =
        stored_brightness >= kMinimumBrightness
            ? stored_brightness
            : kDefaultBrightness;
    pending_brightness = active_brightness;
}

bool saveBrightnessSetting()
{
    Preferences preferences;
    if (!preferences.begin("clock_config", false)) {
        Serial.println("Unable to save brightness setting");
        return false;
    }
    const size_t bytes_written =
        preferences.putUChar("brightness", active_brightness);
    preferences.end();
    return bytes_written == sizeof(active_brightness);
}

void loadTimeSyncSettings()
{
    Preferences preferences;
    if (!preferences.begin("clock_sync", true)) {
        Serial.println("Unable to read time sync settings");
        return;
    }
    last_ntp_sync_epoch = static_cast<time_t>(
        preferences.getULong64("last_ntp", 0));
    automatic_time_sync_enabled = preferences.getBool("auto_sync", true);
    const uint8_t default_result =
        last_ntp_sync_epoch >= kMinimumValidEpoch
            ? static_cast<uint8_t>(TimeSyncResult::Success)
            : static_cast<uint8_t>(TimeSyncResult::Never);
    const uint8_t stored_result =
        preferences.getUChar("last_result", default_result);
    last_time_sync_result =
        stored_result <= static_cast<uint8_t>(TimeSyncResult::Failed)
            ? static_cast<TimeSyncResult>(stored_result)
            : TimeSyncResult::Never;
    preferences.end();
}

bool saveTimeSyncSettings()
{
    Preferences preferences;
    if (!preferences.begin("clock_sync", false)) {
        Serial.println("Unable to save time sync settings");
        return false;
    }
    const bool last_sync_saved =
        preferences.putULong64(
            "last_ntp", static_cast<uint64_t>(last_ntp_sync_epoch)) > 0;
    const bool auto_sync_saved =
        preferences.putBool("auto_sync",
                            automatic_time_sync_enabled) > 0;
    const bool result_saved =
        preferences.putUChar(
            "last_result",
            static_cast<uint8_t>(last_time_sync_result)) > 0;
    preferences.end();
    return last_sync_saved && auto_sync_saved && result_saved;
}

void ntpTimeAvailableCallback(struct timeval *)
{
    // This callback runs in the TCP/IP task. RTC, LVGL, and Preferences work
    // remains in loop() so those components are only touched from one task.
    ntp_sync_received = true;
}

void stopTimeSyncRadio()
{
    if (time_sync_state == TimeSyncState::WaitingForNtp) {
        esp_sntp_stop();
    }
    if (time_sync_owns_wifi_connection) {
        WiFi.disconnect(true, false);
    }
    time_sync_state = TimeSyncState::Idle;
    time_sync_owns_wifi_connection = false;
    refreshWiFiScreen();
}

void beginNtpSyncOnConnectedWiFi()
{
    Serial.printf("Wi-Fi connected: %s\n", WiFi.SSID().c_str());
    wifi_connection_result = WiFiConnectionResult::Connected;
    ntp_sync_received = false;
    time_sync_state = TimeSyncState::WaitingForNtp;
    time_sync_state_started_ms = millis();
    setTimeSyncStatus("SYNCING TIME", kAccentColor);
    refreshTimeSyncScreen();
    refreshWiFiScreen();
    configTzTime(kTimeZone, kNtpServer1, kNtpServer2, kNtpServer3);
}

bool startCurrentWiFiNetwork()
{
    while (wifi_credential_index < kWiFiCredentialCount) {
        const WiFiCredential &credential =
            kWiFiCredentials[wifi_credential_index];
        if (credential.ssid != nullptr && credential.ssid[0] != '\0') {
            WiFi.disconnect(false, false);
            WiFi.begin(credential.ssid,
                       credential.password == nullptr ? "" : credential.password);
            time_sync_state = TimeSyncState::Connecting;
            time_sync_state_started_ms = millis();
            char notification[32];
            snprintf(notification, sizeof(notification),
                     "CONNECTING WIFI %u/%u",
                     static_cast<unsigned>(wifi_credential_index + 1),
                     static_cast<unsigned>(kWiFiCredentialCount));
            setTimeSyncStatus(notification, kAccentColor);
            refreshTimeSyncScreen();
            refreshWiFiScreen();
            Serial.printf("Connecting to Wi-Fi network %u of %u\n",
                          static_cast<unsigned>(wifi_credential_index + 1),
                          static_cast<unsigned>(kWiFiCredentialCount));
            return true;
        }
        ++wifi_credential_index;
    }
    return false;
}

bool startCurrentWiFiConnection()
{
    while (wifi_connection_credential_index < kWiFiCredentialCount) {
        const WiFiCredential &credential =
            kWiFiCredentials[wifi_connection_credential_index];
        if (credential.ssid != nullptr && credential.ssid[0] != '\0') {
            WiFi.disconnect(false, false);
            WiFi.begin(credential.ssid,
                       credential.password == nullptr ? "" :
                                                         credential.password);
            wifi_connection_in_progress = true;
            wifi_connection_started_ms = millis();
            refreshWiFiScreen();
            Serial.printf("Manual Wi-Fi connection %u of %u\n",
                          static_cast<unsigned>(
                              wifi_connection_credential_index + 1),
                          static_cast<unsigned>(kWiFiCredentialCount));
            return true;
        }
        ++wifi_connection_credential_index;
    }
    return false;
}

void failWiFiConnection()
{
    WiFi.disconnect(true, false);
    wifi_connection_in_progress = false;
    wifi_connection_result = WiFiConnectionResult::Failed;
    wifi_retry_not_before_ms = millis() + kWiFiRetryIntervalMs;
    refreshWiFiScreen();
    refreshTimeSyncScreen();
    Serial.println("Manual Wi-Fi connection failed");
}

void requestWiFiReconnect()
{
    if (isRadioBusy()) {
        return;
    }
    if (kWiFiCredentialCount == 0) {
        failWiFiConnection();
        return;
    }

    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    wifi_connection_result = WiFiConnectionResult::Unknown;
    wifi_connection_credential_index = 0;
    if (!startCurrentWiFiConnection()) {
        failWiFiConnection();
    }
}

void processWiFiConnection()
{
    if (!isWiFiConnectionBusy()) {
        return;
    }
    if (WiFi.status() == WL_CONNECTED) {
        wifi_connection_in_progress = false;
        wifi_connection_result = WiFiConnectionResult::Connected;
        wifi_retry_not_before_ms = 0;
        refreshWiFiScreen();
        refreshTimeSyncScreen();
        Serial.printf("Manual Wi-Fi connected: %s\n", WiFi.SSID().c_str());
        return;
    }
    if (millis() - wifi_connection_started_ms >= kWiFiConnectTimeoutMs) {
        ++wifi_connection_credential_index;
        if (!startCurrentWiFiConnection()) {
            failWiFiConnection();
        }
    }
}

void failTimeSync(const char *reason)
{
    Serial.printf("Time sync failed: %s\n", reason);
    stopTimeSyncRadio();
    wifi_retry_not_before_ms = millis() + kWiFiRetryIntervalMs;
    last_time_sync_result = TimeSyncResult::Failed;
    saveTimeSyncSettings();
    refreshTimeSyncScreen();
    setTimeSyncStatus("NTP SYNC FAILED", kLowBatteryColor,
                      kWarningNotificationMs);
}

void requestTimeSync(bool force)
{
    if (isRadioBusy()) {
        return;
    }
    if (!force && !automatic_time_sync_enabled) {
        return;
    }
    if (kWiFiCredentialCount == 0) {
        if (!ntp_config_warning_shown) {
            ntp_config_warning_shown = true;
            setTimeSyncStatus("NTP NOT CONFIGURED", kMutedColor,
                              kWarningNotificationMs);
        }
        if (force) {
            last_time_sync_result = TimeSyncResult::Failed;
            saveTimeSyncSettings();
            refreshTimeSyncScreen();
        }
        return;
    }
    if (!force && !isNtpSyncDue()) {
        return;
    }
    if (!force && wifi_retry_not_before_ms != 0 &&
        static_cast<int32_t>(millis() - wifi_retry_not_before_ms) < 0) {
        return;
    }

    if (force) {
        wifi_retry_not_before_ms = 0;
    }
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    if (WiFi.status() == WL_CONNECTED) {
        time_sync_owns_wifi_connection = false;
        beginNtpSyncOnConnectedWiFi();
        return;
    }
    time_sync_owns_wifi_connection = true;
    wifi_credential_index = 0;
    if (!startCurrentWiFiNetwork()) {
        failTimeSync("no usable Wi-Fi credentials");
    }
}

void requestTimeSyncIfDue()
{
    requestTimeSync(false);
}

void completeNtpSync()
{
    const time_t now = time(nullptr);
    if (now < kMinimumValidEpoch) {
        failTimeSync("NTP returned an invalid time");
        return;
    }
    if ((instance.getDeviceProbe() & HW_RTC_ONLINE) == 0 ||
        instance.rtc.hwClockWrite() != 0) {
        failTimeSync("RTC update failed");
        return;
    }

    last_ntp_sync_epoch = now;
    last_time_sync_result = TimeSyncResult::Success;
    wifi_retry_not_before_ms = 0;
    saveTimeSyncSettings();
    stopTimeSyncRadio();
    syncClockFromRtc();
    refreshTimeSyncScreen();
    setTimeSyncStatus("TIME SYNCED", kAccentColor,
                      kSuccessNotificationMs);
    Serial.println("NTP time written to RTC");
}

void processTimeSync()
{
    switch (time_sync_state) {
    case TimeSyncState::Idle:
        requestTimeSyncIfDue();
        return;

    case TimeSyncState::Connecting:
        if (WiFi.status() == WL_CONNECTED) {
            beginNtpSyncOnConnectedWiFi();
            return;
        }
        if (millis() - time_sync_state_started_ms >= kWiFiConnectTimeoutMs) {
            ++wifi_credential_index;
            if (!startCurrentWiFiNetwork()) {
                failTimeSync("all Wi-Fi networks unavailable");
            }
        }
        return;

    case TimeSyncState::WaitingForNtp:
        if (ntp_sync_received) {
            ntp_sync_received = false;
            completeNtpSync();
            return;
        }
        if (WiFi.status() != WL_CONNECTED) {
            failTimeSync("Wi-Fi disconnected during NTP sync");
            return;
        }
        if (millis() - time_sync_state_started_ms >= kNtpSyncTimeoutMs) {
            failTimeSync("NTP timeout");
        }
        return;
    }
}

void syncClockFromRtc()
{
    if ((instance.getDeviceProbe() & HW_RTC_ONLINE) == 0) {
        showClockError("RTC NOT AVAILABLE");
        return;
    }

    instance.rtc.hwClockRead();
    last_second = -1;
    updateClock(nullptr);
}

void updateSettingLabels()
{
    lv_label_set_text_fmt(field_labels[0], "%04d", setting_year);
    lv_label_set_text_fmt(field_labels[1], "%02d", setting_month);
    lv_label_set_text_fmt(field_labels[2], "%02d", setting_day);
    lv_label_set_text_fmt(field_labels[3], "%02d", setting_hour);
    lv_label_set_text_fmt(field_labels[4], "%02d", setting_minute);
    lv_label_set_text_fmt(field_labels[5], "%02d", setting_second);

    const uint8_t selected = static_cast<uint8_t>(selected_field);
    for (uint8_t index = 0;
         index < static_cast<uint8_t>(SettingField::Count); ++index) {
        const uint32_t color = index == selected ? kAccentColor : kButtonColor;
        lv_obj_set_style_bg_color(field_buttons[index], lv_color_hex(color), 0);
        lv_obj_set_style_text_color(
            field_labels[index],
            lv_color_hex(index == selected ? kBackgroundColor : kPrimaryColor), 0);
    }
    lv_label_set_text_fmt(settings_status_label, "SELECTED: %s",
                          kFieldNames[selected]);
}

void loadSettingValues()
{
    struct tm timeinfo = {};
    if ((instance.getDeviceProbe() & HW_RTC_ONLINE) != 0) {
        instance.rtc.getDateTime(&timeinfo);
    }

    if (isValidDateTime(timeinfo)) {
        setting_year = timeinfo.tm_year + 1900;
        setting_month = timeinfo.tm_mon + 1;
        setting_day = timeinfo.tm_mday;
        setting_hour = timeinfo.tm_hour;
        setting_minute = timeinfo.tm_min;
        setting_second = timeinfo.tm_sec;
    } else {
        setting_year = 2026;
        setting_month = 1;
        setting_day = 1;
        setting_hour = 0;
        setting_minute = 0;
        setting_second = 0;
    }

    selected_field = SettingField::Year;
    updateSettingLabels();
}

void adjustSelectedField(int amount)
{
    switch (selected_field) {
    case SettingField::Year:
        setting_year = wrapValue(setting_year + amount,
                                 kMinimumYear, kMaximumYear);
        break;
    case SettingField::Month:
        setting_month = wrapValue(setting_month + amount, 1, 12);
        break;
    case SettingField::Day:
        setting_day = wrapValue(setting_day + amount, 1,
                                daysInMonth(setting_year, setting_month));
        break;
    case SettingField::Hour:
        setting_hour = wrapValue(setting_hour + amount, 0, 23);
        break;
    case SettingField::Minute:
        setting_minute = wrapValue(setting_minute + amount, 0, 59);
        break;
    case SettingField::Second:
        setting_second = wrapValue(setting_second + amount, 0, 59);
        break;
    case SettingField::Count:
        return;
    }

    const int maximum_day = daysInMonth(setting_year, setting_month);
    if (setting_day > maximum_day) {
        setting_day = maximum_day;
    }
    updateSettingLabels();
}

void fieldButtonCallback(lv_event_t *event)
{
    selected_field = static_cast<SettingField>(
        reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
    updateSettingLabels();
}

void decrementButtonCallback(lv_event_t *)
{
    adjustSelectedField(-1);
}

void incrementButtonCallback(lv_event_t *)
{
    adjustSelectedField(1);
}

void showClockScreen(lv_event_t *)
{
    lv_screen_load_anim(clock_screen, LV_SCR_LOAD_ANIM_MOVE_RIGHT,
                        180, 0, false);
}

void showSettingsHubScreen(lv_event_t *)
{
    lv_screen_load_anim(settings_hub_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT,
                        180, 0, false);
}

void showPowerDisplayScreen(lv_event_t *)
{
    lv_screen_load_anim(power_display_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT,
                        180, 0, false);
}

void showTimeoutSettingsScreen(lv_event_t *)
{
    lv_screen_load_anim(timeout_settings_screen,
                        LV_SCR_LOAD_ANIM_MOVE_LEFT, 180, 0, false);
}

void showResetSettingsScreen(lv_event_t *)
{
    lv_screen_load_anim(reset_settings_screen,
                        LV_SCR_LOAD_ANIM_MOVE_LEFT, 180, 0, false);
}

void showWiFiNtpScreen(lv_event_t *)
{
    lv_screen_load_anim(wifi_ntp_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT,
                        180, 0, false);
}

void returnToSettingsHubScreen(lv_event_t *)
{
    lv_screen_load_anim(settings_hub_screen, LV_SCR_LOAD_ANIM_MOVE_RIGHT,
                        180, 0, false);
}

void returnToPowerDisplayScreen(lv_event_t *)
{
    lv_screen_load_anim(power_display_screen,
                        LV_SCR_LOAD_ANIM_MOVE_RIGHT, 180, 0, false);
}

void returnToWiFiNtpScreen(lv_event_t *)
{
    lv_screen_load_anim(wifi_ntp_screen, LV_SCR_LOAD_ANIM_MOVE_RIGHT,
                        180, 0, false);
}

void refreshPowerDisplayScreen()
{
    lv_label_set_text(deploy_mode_button_label,
                      deploy_mode_enabled
                          ? "DEPLOY MODE: ON"
                          : "DEPLOY MODE: OFF");
    lv_label_set_text(deploy_mode_status_label,
                      deploy_mode_enabled
                          ? "DEPLOY MODE KEEPS DEVICE AWAKE"
                          : "POWER SAVING ACTIVE");
    lv_obj_set_style_text_color(deploy_mode_status_label,
                                lv_color_hex(kMutedColor), 0);
    lv_label_set_text(clock_format_button_label,
                      use_24_hour_clock
                          ? "CLOCK FORMAT: 24H"
                          : "CLOCK FORMAT: 12H");

    if (deploy_mode_enabled) {
        lv_obj_remove_flag(deploy_mode_clock_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(deploy_mode_clock_label, LV_OBJ_FLAG_HIDDEN);
    }
}

void refreshTimeoutSettingsScreen()
{
    lv_label_set_text_fmt(clock_timeout_button_label,
                          "CLOCK SCREEN: %lu SEC",
                          static_cast<unsigned long>(
                              pending_clock_screen_timeout_seconds));
    lv_label_set_text_fmt(settings_timeout_button_label,
                          "SETTINGS: %lu SEC",
                          static_cast<unsigned long>(
                              pending_settings_screen_timeout_seconds));
    lv_label_set_text_fmt(light_sleep_delay_button_label,
                          "LIGHT SLEEP: %lu SEC",
                          static_cast<unsigned long>(
                              pending_light_sleep_delay_seconds));
}

void toggleDeployModeCallback(lv_event_t *)
{
    deploy_mode_enabled = !deploy_mode_enabled;
    last_activity_ms = millis();
    refreshPowerDisplayScreen();
    Serial.printf("Deploy mode: %s\n",
                  deploy_mode_enabled ? "enabled" : "disabled");
}

void toggleClockFormatCallback(lv_event_t *)
{
    const bool previous_format = use_24_hour_clock;
    use_24_hour_clock = !use_24_hour_clock;
    if (!savePowerDisplaySettings()) {
        use_24_hour_clock = previous_format;
        refreshPowerDisplayScreen();
        lv_label_set_text(deploy_mode_status_label, "SAVE FAILED");
        lv_obj_set_style_text_color(deploy_mode_status_label,
                                    lv_color_hex(kLowBatteryColor), 0);
        return;
    }

    last_second = -1;
    last_activity_ms = millis();
    refreshPowerDisplayScreen();
    updateClock(nullptr);
    Serial.printf("Clock format: %s\n",
                  use_24_hour_clock ? "24-hour" : "12-hour");
}

void powerDisplayScreenEventCallback(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_SCREEN_LOAD_START) {
        last_activity_ms = millis();
        lv_obj_set_style_text_color(deploy_mode_status_label,
                                    lv_color_hex(kMutedColor), 0);
        refreshPowerDisplayScreen();
    }
}

void timeoutSettingsScreenEventCallback(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_SCREEN_LOAD_START) {
        return;
    }

    last_activity_ms = millis();
    pending_clock_screen_timeout_seconds = clock_screen_timeout_seconds;
    pending_settings_screen_timeout_seconds =
        settings_screen_timeout_seconds;
    pending_light_sleep_delay_seconds = light_sleep_delay_seconds;
    lv_label_set_text(timeout_settings_status_label, "SELECT A PRESET");
    lv_obj_set_style_text_color(timeout_settings_status_label,
                                lv_color_hex(kMutedColor), 0);
    refreshTimeoutSettingsScreen();
}

void cycleClockTimeoutCallback(lv_event_t *)
{
    pending_clock_screen_timeout_seconds = nextTimeoutOption(
        pending_clock_screen_timeout_seconds, kClockTimeoutOptions);
    refreshTimeoutSettingsScreen();
}

void cycleSettingsTimeoutCallback(lv_event_t *)
{
    pending_settings_screen_timeout_seconds = nextTimeoutOption(
        pending_settings_screen_timeout_seconds, kSettingsTimeoutOptions);
    refreshTimeoutSettingsScreen();
}

void cycleLightSleepDelayCallback(lv_event_t *)
{
    pending_light_sleep_delay_seconds = nextTimeoutOption(
        pending_light_sleep_delay_seconds, kLightSleepDelayOptions);
    refreshTimeoutSettingsScreen();
}

void cancelTimeoutSettingsCallback(lv_event_t *)
{
    pending_clock_screen_timeout_seconds = clock_screen_timeout_seconds;
    pending_settings_screen_timeout_seconds =
        settings_screen_timeout_seconds;
    pending_light_sleep_delay_seconds = light_sleep_delay_seconds;
    returnToPowerDisplayScreen(nullptr);
}

void saveTimeoutSettingsCallback(lv_event_t *)
{
    const uint32_t previous_clock_timeout =
        clock_screen_timeout_seconds;
    const uint32_t previous_settings_timeout =
        settings_screen_timeout_seconds;
    const uint32_t previous_sleep_delay = light_sleep_delay_seconds;

    clock_screen_timeout_seconds =
        pending_clock_screen_timeout_seconds;
    settings_screen_timeout_seconds =
        pending_settings_screen_timeout_seconds;
    light_sleep_delay_seconds = pending_light_sleep_delay_seconds;
    if (!savePowerDisplaySettings()) {
        clock_screen_timeout_seconds = previous_clock_timeout;
        settings_screen_timeout_seconds = previous_settings_timeout;
        light_sleep_delay_seconds = previous_sleep_delay;
        lv_label_set_text(timeout_settings_status_label, "SAVE FAILED");
        lv_obj_set_style_text_color(timeout_settings_status_label,
                                    lv_color_hex(kLowBatteryColor), 0);
        return;
    }

    last_activity_ms = millis();
    Serial.printf("Timeouts saved: clock=%lus settings=%lus sleep=%lus\n",
                  static_cast<unsigned long>(clock_screen_timeout_seconds),
                  static_cast<unsigned long>(settings_screen_timeout_seconds),
                  static_cast<unsigned long>(light_sleep_delay_seconds));
    returnToPowerDisplayScreen(nullptr);
}

void resetSettingsScreenEventCallback(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_SCREEN_LOAD_START) {
        last_activity_ms = millis();
        lv_label_set_text(reset_settings_status_label,
                          "THIS CANNOT BE UNDONE");
        lv_obj_set_style_text_color(reset_settings_status_label,
                                    lv_color_hex(kLowBatteryColor), 0);
    }
}

void resetAllSettingsCallback(lv_event_t *)
{
    active_brightness = kDefaultBrightness;
    pending_brightness = kDefaultBrightness;
    clock_screen_timeout_seconds = kDefaultClockScreenTimeoutSeconds;
    settings_screen_timeout_seconds = kDefaultSettingsScreenTimeoutSeconds;
    light_sleep_delay_seconds = kDefaultLightSleepDelaySeconds;
    pending_clock_screen_timeout_seconds =
        kDefaultClockScreenTimeoutSeconds;
    pending_settings_screen_timeout_seconds =
        kDefaultSettingsScreenTimeoutSeconds;
    pending_light_sleep_delay_seconds =
        kDefaultLightSleepDelaySeconds;
    use_24_hour_clock = true;
    automatic_time_sync_enabled = true;
    deploy_mode_enabled = false;

    const bool brightness_saved = saveBrightnessSetting();
    const bool power_display_saved = savePowerDisplaySettings();
    const bool time_sync_saved = saveTimeSyncSettings();
    const bool saved = brightness_saved &&
                       power_display_saved &&
                       time_sync_saved;

    instance.setBrightness(active_brightness);
    last_second = -1;
    last_activity_ms = millis();
    refreshPowerDisplayScreen();
    refreshTimeSyncScreen();
    updateClock(nullptr);
    requestTimeSyncIfDue();

    if (!saved) {
        lv_label_set_text(reset_settings_status_label, "RESET SAVE FAILED");
        return;
    }

    Serial.println("Saved settings restored to defaults");
    returnToSettingsHubScreen(nullptr);
}

void updateBrightnessControls()
{
    lv_slider_set_value(brightness_slider, pending_brightness, LV_ANIM_OFF);
    const unsigned percent =
        (static_cast<unsigned>(pending_brightness) * 100 + 127) / 255;
    lv_label_set_text_fmt(brightness_value_label, "%u%%", percent);
    instance.setBrightness(pending_brightness);
}

void brightnessSliderCallback(lv_event_t *)
{
    last_activity_ms = millis();
    pending_brightness = static_cast<uint8_t>(
        lv_slider_get_value(brightness_slider));
    updateBrightnessControls();
}

void adjustBrightness(int amount)
{
    int adjusted = static_cast<int>(pending_brightness) + amount;
    if (adjusted < kMinimumBrightness) {
        adjusted = kMinimumBrightness;
    }
    if (adjusted > kMaximumBrightness) {
        adjusted = kMaximumBrightness;
    }
    pending_brightness = static_cast<uint8_t>(adjusted);
    last_activity_ms = millis();
    updateBrightnessControls();
}

void decreaseBrightnessCallback(lv_event_t *)
{
    adjustBrightness(-kBrightnessStep);
}

void increaseBrightnessCallback(lv_event_t *)
{
    adjustBrightness(kBrightnessStep);
}

void brightnessScreenEventCallback(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_SCREEN_LOAD_START) {
        return;
    }
    last_activity_ms = millis();
    pending_brightness = active_brightness;
    lv_label_set_text(brightness_status_label, "PREVIEW - SAVE TO KEEP");
    lv_obj_set_style_text_color(brightness_status_label,
                                lv_color_hex(kMutedColor), 0);
    updateBrightnessControls();
}

void showBrightnessScreen(lv_event_t *)
{
    lv_screen_load_anim(brightness_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT,
                        180, 0, false);
}

void cancelBrightnessCallback(lv_event_t *)
{
    pending_brightness = active_brightness;
    instance.setBrightness(active_brightness);
    returnToSettingsHubScreen(nullptr);
}

void saveBrightnessCallback(lv_event_t *)
{
    const uint8_t previous_brightness = active_brightness;
    active_brightness = pending_brightness;
    if (!saveBrightnessSetting()) {
        active_brightness = previous_brightness;
        lv_label_set_text(brightness_status_label, "SAVE FAILED");
        lv_obj_set_style_text_color(brightness_status_label,
                                    lv_color_hex(kLowBatteryColor), 0);
        return;
    }

    Serial.printf("Brightness saved: %u\n", active_brightness);
    returnToSettingsHubScreen(nullptr);
}

void saveDateTimeCallback(lv_event_t *)
{
    if ((instance.getDeviceProbe() & HW_RTC_ONLINE) == 0) {
        lv_label_set_text(settings_status_label, "RTC NOT AVAILABLE");
        return;
    }

    instance.rtc.setDateTime(setting_year, setting_month, setting_day,
                             setting_hour, setting_minute, setting_second);
    instance.rtc.hwClockRead();
    Serial.printf("RTC set to %04d-%02d-%02d %02d:%02d:%02d\n",
                  setting_year, setting_month, setting_day,
                  setting_hour, setting_minute, setting_second);
    returnToSettingsHubScreen(nullptr);
}

void dateTimeScreenEventCallback(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_SCREEN_LOAD_START) {
        last_activity_ms = millis();
        loadSettingValues();
    }
}

void showDateTimeScreen(lv_event_t *)
{
    lv_screen_load_anim(date_time_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT,
                        180, 0, false);
}

void timeSyncScreenEventCallback(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_SCREEN_LOAD_START) {
        last_activity_ms = millis();
        refreshTimeSyncScreen();
    }
}

void toggleAutomaticTimeSyncCallback(lv_event_t *)
{
    automatic_time_sync_enabled = !automatic_time_sync_enabled;
    saveTimeSyncSettings();
    if (automatic_time_sync_enabled) {
        requestTimeSyncIfDue();
    }
    refreshTimeSyncScreen();
    Serial.printf("Automatic time sync: %s\n",
                  automatic_time_sync_enabled ? "enabled" : "disabled");
}

void syncNowCallback(lv_event_t *)
{
    requestTimeSync(true);
    refreshTimeSyncScreen();
}

void showTimeSyncScreen(lv_event_t *)
{
    lv_screen_load_anim(time_sync_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT,
                        180, 0, false);
}

void wifiScreenEventCallback(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_SCREEN_LOAD_START) {
        last_activity_ms = millis();
        refreshWiFiScreen();
    }
}

void reconnectWiFiCallback(lv_event_t *)
{
    requestWiFiReconnect();
    refreshWiFiScreen();
}

void disconnectWiFiCallback(lv_event_t *)
{
    if (isTimeSyncBusy()) {
        return;
    }
    wifi_connection_in_progress = false;
    wifi_connection_result = WiFiConnectionResult::Unknown;
    WiFi.disconnect(true, false);
    refreshWiFiScreen();
    refreshTimeSyncScreen();
    Serial.println("Wi-Fi disconnected by user");
}

void showWiFiScreen(lv_event_t *)
{
    lv_screen_load_anim(wifi_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT,
                        180, 0, false);
}

void updateWiFiScreenStatus(lv_timer_t *)
{
    if (lv_screen_active() == wifi_screen) {
        refreshWiFiScreen();
    }
}

void settingsHubScreenEventCallback(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_SCREEN_LOAD_START) {
        last_activity_ms = millis();
    }
}

void clockScreenEventCallback(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_SCREEN_LOADED) {
        syncClockFromRtc();
        updateBatteryStatus(nullptr);
        requestTimeSyncIfDue();
    }
}

lv_obj_t *createClockTimeLabel(const char *text, int x_offset, int width,
                               const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(clock_screen);
    lv_label_set_text(label, text);
    lv_obj_set_width(label, width);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(kPrimaryColor), 0);
    lv_obj_align(label, LV_ALIGN_CENTER, x_offset, -20);
    return label;
}

void createClockScreen()
{
    clock_screen = lv_screen_active();
    styleScreen(clock_screen);
    lv_obj_add_event_cb(clock_screen, markUserActivity,
                        LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(clock_screen, clockScreenEventCallback,
                        LV_EVENT_SCREEN_LOADED, nullptr);

    createButton(clock_screen, "SET", 176, 14, 50, 30,
                 showSettingsHubScreen);

    battery_label = lv_label_create(clock_screen);
    lv_label_set_text(battery_label, "N/A");
    lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(battery_label, lv_color_hex(kMutedColor), 0);
    lv_obj_align(battery_label, LV_ALIGN_TOP_LEFT, 18, 24);

    deploy_mode_clock_label = lv_label_create(clock_screen);
    lv_label_set_text(deploy_mode_clock_label, "DEPLOY");
    lv_obj_set_style_text_font(deploy_mode_clock_label,
                               &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(deploy_mode_clock_label,
                                lv_color_hex(kAccentColor), 0);
    lv_obj_align(deploy_mode_clock_label, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_add_flag(deploy_mode_clock_label, LV_OBJ_FLAG_HIDDEN);

    hour_label = createClockTimeLabel(
        "--", -72, 60, &lv_font_watch_digits_36);
    createClockTimeLabel(":", -36, 12, &lv_font_montserrat_40);
    minute_label = createClockTimeLabel(
        "--", 0, 60, &lv_font_watch_digits_36);
    createClockTimeLabel(":", 36, 12, &lv_font_montserrat_40);
    second_label = createClockTimeLabel(
        "--", 72, 60, &lv_font_watch_digits_36);

    meridiem_label = lv_label_create(clock_screen);
    lv_label_set_text(meridiem_label, "AM");
    lv_obj_set_style_text_font(meridiem_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(meridiem_label, lv_color_hex(kMutedColor), 0);
    lv_obj_align(meridiem_label, LV_ALIGN_CENTER, 100, 8);
    lv_obj_add_flag(meridiem_label, LV_OBJ_FLAG_HIDDEN);

    date_line_label = lv_label_create(clock_screen);
    lv_obj_set_style_text_font(date_line_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(date_line_label, lv_color_hex(kAccentColor), 0);
    lv_obj_align(date_line_label, LV_ALIGN_CENTER, 0, 34);

    time_sync_label = lv_label_create(clock_screen);
    lv_label_set_text(time_sync_label, "");
    lv_obj_set_style_text_font(time_sync_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(time_sync_label, lv_color_hex(kMutedColor), 0);
    lv_obj_align(time_sync_label, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_add_flag(time_sync_label, LV_OBJ_FLAG_HIDDEN);
}

void createFieldButton(SettingField field, int x, int y, int width)
{
    const uint8_t index = static_cast<uint8_t>(field);
    lv_obj_t *button = lv_button_create(date_time_screen);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, 36);
    styleButton(button);
    lv_obj_add_event_cb(button, markUserActivity, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(button, fieldButtonCallback, LV_EVENT_CLICKED,
                        reinterpret_cast<void *>(static_cast<uintptr_t>(index)));

    field_buttons[index] = button;
    field_labels[index] = lv_label_create(button);
    lv_obj_set_style_text_font(field_labels[index], &lv_font_montserrat_18, 0);
    lv_obj_center(field_labels[index]);
}

void createSettingsHubScreen()
{
    settings_hub_screen = lv_obj_create(nullptr);
    styleScreen(settings_hub_screen);
    lv_obj_add_event_cb(settings_hub_screen, markUserActivity,
                        LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(settings_hub_screen, settingsHubScreenEventCallback,
                        LV_EVENT_SCREEN_LOAD_START, nullptr);

    lv_obj_t *title = lv_label_create(settings_hub_screen);
    lv_label_set_text(title, "SETTINGS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(kAccentColor), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    lv_obj_t *subtitle = lv_label_create(settings_hub_screen);
    lv_label_set_text(subtitle, "SELECT AN OPTION");
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(kMutedColor), 0);
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 30);

    createButton(settings_hub_screen, "DATE & TIME",
                 20, 44, 200, 34, showDateTimeScreen);
    createButton(settings_hub_screen, "POWER & DISPLAY",
                 20, 82, 200, 34, showPowerDisplayScreen);
    createButton(settings_hub_screen, "BRIGHTNESS",
                 20, 120, 200, 34, showBrightnessScreen);
    createButton(settings_hub_screen, "WI-FI & NTP",
                 20, 158, 200, 34, showWiFiNtpScreen);
    createButton(settings_hub_screen, "BACK",
                 20, 202, 200, 28, showClockScreen);
}

void createPowerDisplayScreen()
{
    power_display_screen = lv_obj_create(nullptr);
    styleScreen(power_display_screen);
    lv_obj_add_event_cb(power_display_screen, markUserActivity,
                        LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(power_display_screen,
                        powerDisplayScreenEventCallback,
                        LV_EVENT_SCREEN_LOAD_START, nullptr);

    lv_obj_t *title = lv_label_create(power_display_screen);
    lv_label_set_text(title, "POWER & DISPLAY");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(kAccentColor), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    deploy_mode_status_label = lv_label_create(power_display_screen);
    lv_label_set_text(deploy_mode_status_label, "POWER SAVING ACTIVE");
    lv_obj_set_width(deploy_mode_status_label, 220);
    lv_obj_set_style_text_align(deploy_mode_status_label,
                                LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(deploy_mode_status_label,
                               &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(deploy_mode_status_label,
                                lv_color_hex(kMutedColor), 0);
    lv_obj_align(deploy_mode_status_label, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t *deploy_mode_button = createButton(
        power_display_screen, "DEPLOY MODE: OFF",
        20, 44, 200, 34, toggleDeployModeCallback);
    deploy_mode_button_label = lv_obj_get_child(deploy_mode_button, 0);

    createButton(power_display_screen, "TIMEOUTS",
                 20, 82, 200, 34, showTimeoutSettingsScreen);

    lv_obj_t *clock_format_button = createButton(
        power_display_screen, "CLOCK FORMAT: 24H",
        20, 120, 200, 34, toggleClockFormatCallback);
    clock_format_button_label = lv_obj_get_child(clock_format_button, 0);

    lv_obj_t *reset_button = createButton(
        power_display_screen, "RESET SETTINGS",
        20, 158, 200, 34, showResetSettingsScreen);
    lv_obj_set_style_text_color(lv_obj_get_child(reset_button, 0),
                                lv_color_hex(kLowBatteryColor), 0);

    createButton(power_display_screen, "BACK", 20, 202, 200, 30,
                 returnToSettingsHubScreen);
    refreshPowerDisplayScreen();
}

void createTimeoutSettingsScreen()
{
    timeout_settings_screen = lv_obj_create(nullptr);
    styleScreen(timeout_settings_screen);
    lv_obj_add_event_cb(timeout_settings_screen, markUserActivity,
                        LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(timeout_settings_screen,
                        timeoutSettingsScreenEventCallback,
                        LV_EVENT_SCREEN_LOAD_START, nullptr);

    lv_obj_t *title = lv_label_create(timeout_settings_screen);
    lv_label_set_text(title, "DISPLAY TIMEOUTS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(kAccentColor), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *clock_timeout_button = createButton(
        timeout_settings_screen, "CLOCK SCREEN: 15 SEC",
        20, 46, 200, 34, cycleClockTimeoutCallback);
    clock_timeout_button_label = lv_obj_get_child(
        clock_timeout_button, 0);

    lv_obj_t *settings_timeout_button = createButton(
        timeout_settings_screen, "SETTINGS: 60 SEC",
        20, 86, 200, 34, cycleSettingsTimeoutCallback);
    settings_timeout_button_label = lv_obj_get_child(
        settings_timeout_button, 0);

    lv_obj_t *sleep_delay_button = createButton(
        timeout_settings_screen, "LIGHT SLEEP: 5 SEC",
        20, 126, 200, 34, cycleLightSleepDelayCallback);
    light_sleep_delay_button_label = lv_obj_get_child(
        sleep_delay_button, 0);

    timeout_settings_status_label =
        lv_label_create(timeout_settings_screen);
    lv_label_set_text(timeout_settings_status_label, "SELECT A PRESET");
    lv_obj_set_style_text_font(timeout_settings_status_label,
                               &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(timeout_settings_status_label,
                                lv_color_hex(kMutedColor), 0);
    lv_obj_align(timeout_settings_status_label,
                 LV_ALIGN_TOP_MID, 0, 174);

    createButton(timeout_settings_screen, "CANCEL",
                 15, 202, 100, 30, cancelTimeoutSettingsCallback);
    lv_obj_t *save = createButton(
        timeout_settings_screen, "SAVE",
        125, 202, 100, 30, saveTimeoutSettingsCallback);
    lv_obj_set_style_bg_color(save, lv_color_hex(kAccentColor), 0);
    lv_obj_set_style_text_color(lv_obj_get_child(save, 0),
                                lv_color_hex(kBackgroundColor), 0);
    refreshTimeoutSettingsScreen();
}

void createResetSettingsScreen()
{
    reset_settings_screen = lv_obj_create(nullptr);
    styleScreen(reset_settings_screen);
    lv_obj_add_event_cb(reset_settings_screen, markUserActivity,
                        LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(reset_settings_screen,
                        resetSettingsScreenEventCallback,
                        LV_EVENT_SCREEN_LOAD_START, nullptr);

    lv_obj_t *title = lv_label_create(reset_settings_screen);
    lv_label_set_text(title, "RESET SETTINGS?");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(kLowBatteryColor), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 28);

    lv_obj_t *description = lv_label_create(reset_settings_screen);
    lv_label_set_text(
        description,
        "BRIGHTNESS, DISPLAY TIMEOUTS,\nCLOCK FORMAT, AND AUTO SYNC\n"
        "WILL RETURN TO DEFAULTS.");
    lv_obj_set_width(description, 220);
    lv_obj_set_style_text_align(description, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(description, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(description, lv_color_hex(kPrimaryColor), 0);
    lv_obj_align(description, LV_ALIGN_TOP_MID, 0, 76);

    reset_settings_status_label = lv_label_create(reset_settings_screen);
    lv_label_set_text(reset_settings_status_label,
                      "THIS CANNOT BE UNDONE");
    lv_obj_set_style_text_font(reset_settings_status_label,
                               &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(reset_settings_status_label,
                                lv_color_hex(kLowBatteryColor), 0);
    lv_obj_align(reset_settings_status_label,
                 LV_ALIGN_TOP_MID, 0, 160);

    createButton(reset_settings_screen, "CANCEL",
                 15, 202, 100, 30, returnToPowerDisplayScreen);
    lv_obj_t *reset = createButton(
        reset_settings_screen, "RESET",
        125, 202, 100, 30, resetAllSettingsCallback);
    lv_obj_set_style_bg_color(reset, lv_color_hex(kLowBatteryColor), 0);
    lv_obj_set_style_text_color(lv_obj_get_child(reset, 0),
                                lv_color_hex(kBackgroundColor), 0);
}

void createWiFiNtpScreen()
{
    wifi_ntp_screen = lv_obj_create(nullptr);
    styleScreen(wifi_ntp_screen);
    lv_obj_add_event_cb(wifi_ntp_screen, markUserActivity,
                        LV_EVENT_PRESSED, nullptr);

    lv_obj_t *title = lv_label_create(wifi_ntp_screen);
    lv_label_set_text(title, "WI-FI & NTP");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(kAccentColor), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 14);

    lv_obj_t *description = lv_label_create(wifi_ntp_screen);
    lv_label_set_text(description, "CONNECTION AND TIME SYNC");
    lv_obj_set_style_text_font(description, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(description, lv_color_hex(kMutedColor), 0);
    lv_obj_align(description, LV_ALIGN_TOP_MID, 0, 46);

    createButton(wifi_ntp_screen, "WI-FI",
                 20, 78, 200, 44, showWiFiScreen);
    createButton(wifi_ntp_screen, "TIME SYNC",
                 20, 134, 200, 44, showTimeSyncScreen);
    createButton(wifi_ntp_screen, "BACK",
                 20, 202, 200, 30, returnToSettingsHubScreen);
}

void createDateTimeScreen()
{
    date_time_screen = lv_obj_create(nullptr);
    styleScreen(date_time_screen);
    lv_obj_add_event_cb(date_time_screen, markUserActivity,
                        LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(date_time_screen, dateTimeScreenEventCallback,
                        LV_EVENT_SCREEN_LOAD_START, nullptr);

    lv_obj_t *title = lv_label_create(date_time_screen);
    lv_label_set_text(title, "SET DATE & TIME");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(kAccentColor), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    lv_obj_t *date_heading = lv_label_create(date_time_screen);
    lv_label_set_text(date_heading, "DATE");
    lv_obj_set_style_text_font(date_heading, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(date_heading, lv_color_hex(kMutedColor), 0);
    lv_obj_set_pos(date_heading, 6, 48);

    createFieldButton(SettingField::Year, 48, 36, 76);
    createFieldButton(SettingField::Month, 130, 36, 48);
    createFieldButton(SettingField::Day, 184, 36, 48);

    lv_obj_t *time_heading = lv_label_create(date_time_screen);
    lv_label_set_text(time_heading, "TIME");
    lv_obj_set_style_text_font(time_heading, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(time_heading, lv_color_hex(kMutedColor), 0);
    lv_obj_set_pos(time_heading, 6, 98);

    createFieldButton(SettingField::Hour, 48, 86, 48);
    createFieldButton(SettingField::Minute, 110, 86, 48);
    createFieldButton(SettingField::Second, 172, 86, 48);

    settings_status_label = lv_label_create(date_time_screen);
    lv_obj_set_style_text_font(settings_status_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(settings_status_label,
                                lv_color_hex(kMutedColor), 0);
    lv_obj_align(settings_status_label, LV_ALIGN_TOP_MID, 0, 132);

    lv_obj_t *decrement = createButton(date_time_screen, LV_SYMBOL_MINUS,
                                       35, 151, 75, 40,
                                       decrementButtonCallback);
    lv_obj_t *increment = createButton(date_time_screen, LV_SYMBOL_PLUS,
                                       130, 151, 75, 40,
                                       incrementButtonCallback);
    lv_obj_add_event_cb(decrement, decrementButtonCallback,
                        LV_EVENT_LONG_PRESSED_REPEAT, nullptr);
    lv_obj_add_event_cb(increment, incrementButtonCallback,
                        LV_EVENT_LONG_PRESSED_REPEAT, nullptr);

    createButton(date_time_screen, "CANCEL", 15, 202, 100, 30,
                 returnToSettingsHubScreen);
    lv_obj_t *save = createButton(date_time_screen, "SAVE", 125, 202, 100, 30,
                                  saveDateTimeCallback);
    lv_obj_set_style_bg_color(save, lv_color_hex(kAccentColor), 0);
    lv_obj_set_style_text_color(lv_obj_get_child(save, 0),
                                lv_color_hex(kBackgroundColor), 0);
}

void createBrightnessScreen()
{
    brightness_screen = lv_obj_create(nullptr);
    styleScreen(brightness_screen);
    lv_obj_add_event_cb(brightness_screen, markUserActivity,
                        LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(brightness_screen, brightnessScreenEventCallback,
                        LV_EVENT_SCREEN_LOAD_START, nullptr);

    lv_obj_t *title = lv_label_create(brightness_screen);
    lv_label_set_text(title, "DISPLAY BRIGHTNESS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(kAccentColor), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

    brightness_value_label = lv_label_create(brightness_screen);
    lv_label_set_text(brightness_value_label, "71%");
    lv_obj_set_style_text_font(brightness_value_label,
                               &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(brightness_value_label,
                                lv_color_hex(kPrimaryColor), 0);
    lv_obj_align(brightness_value_label, LV_ALIGN_TOP_MID, 0, 50);

    brightness_slider = lv_slider_create(brightness_screen);
    lv_obj_set_pos(brightness_slider, 24, 98);
    lv_obj_set_size(brightness_slider, 192, 18);
    lv_slider_set_range(brightness_slider,
                        kMinimumBrightness, kMaximumBrightness);
    lv_obj_set_style_bg_color(brightness_slider,
                              lv_color_hex(kButtonColor), LV_PART_MAIN);
    lv_obj_set_style_bg_color(brightness_slider,
                              lv_color_hex(kAccentColor), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(brightness_slider,
                              lv_color_hex(kPrimaryColor), LV_PART_KNOB);
    lv_obj_add_event_cb(brightness_slider, brightnessSliderCallback,
                        LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t *decrease = createButton(brightness_screen, LV_SYMBOL_MINUS,
                                      35, 130, 75, 42,
                                      decreaseBrightnessCallback);
    lv_obj_t *increase = createButton(brightness_screen, LV_SYMBOL_PLUS,
                                      130, 130, 75, 42,
                                      increaseBrightnessCallback);
    lv_obj_add_event_cb(decrease, decreaseBrightnessCallback,
                        LV_EVENT_LONG_PRESSED_REPEAT, nullptr);
    lv_obj_add_event_cb(increase, increaseBrightnessCallback,
                        LV_EVENT_LONG_PRESSED_REPEAT, nullptr);

    brightness_status_label = lv_label_create(brightness_screen);
    lv_label_set_text(brightness_status_label, "PREVIEW - SAVE TO KEEP");
    lv_obj_set_style_text_font(brightness_status_label,
                               &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(brightness_status_label,
                                lv_color_hex(kMutedColor), 0);
    lv_obj_align(brightness_status_label, LV_ALIGN_TOP_MID, 0, 180);

    createButton(brightness_screen, "CANCEL", 15, 202, 100, 30,
                 cancelBrightnessCallback);
    lv_obj_t *save = createButton(brightness_screen, "SAVE",
                                  125, 202, 100, 30,
                                  saveBrightnessCallback);
    lv_obj_set_style_bg_color(save, lv_color_hex(kAccentColor), 0);
    lv_obj_set_style_text_color(lv_obj_get_child(save, 0),
                                lv_color_hex(kBackgroundColor), 0);
}

void createTimeSyncScreen()
{
    time_sync_screen = lv_obj_create(nullptr);
    styleScreen(time_sync_screen);
    lv_obj_add_event_cb(time_sync_screen, markUserActivity,
                        LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(time_sync_screen, timeSyncScreenEventCallback,
                        LV_EVENT_SCREEN_LOAD_START, nullptr);

    lv_obj_t *title = lv_label_create(time_sync_screen);
    lv_label_set_text(title, "TIME SYNC");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(kAccentColor), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t *auto_sync_button = createButton(
        time_sync_screen, "AUTO SYNC: ON",
        20, 48, 200, 38, toggleAutomaticTimeSyncCallback);
    auto_sync_button_label = lv_obj_get_child(auto_sync_button, 0);

    sync_screen_status_label = lv_label_create(time_sync_screen);
    lv_label_set_text(sync_screen_status_label, "STATUS: NEVER SYNCED");
    lv_obj_set_style_text_font(sync_screen_status_label,
                               &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sync_screen_status_label,
                                lv_color_hex(kMutedColor), 0);
    lv_obj_align(sync_screen_status_label, LV_ALIGN_TOP_MID, 0, 98);

    last_sync_label = lv_label_create(time_sync_screen);
    lv_label_set_text(last_sync_label, "LAST SYNC: NEVER");
    lv_obj_set_style_text_font(last_sync_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(last_sync_label,
                                lv_color_hex(kMutedColor), 0);
    lv_obj_align(last_sync_label, LV_ALIGN_TOP_MID, 0, 124);

    sync_now_button = createButton(time_sync_screen, "SYNC NOW",
                                   20, 150, 200, 40, syncNowCallback);
    lv_obj_set_style_bg_color(sync_now_button, lv_color_hex(kAccentColor), 0);
    lv_obj_set_style_text_color(lv_obj_get_child(sync_now_button, 0),
                                lv_color_hex(kBackgroundColor), 0);

    createButton(time_sync_screen, "BACK", 20, 202, 200, 30,
                 returnToWiFiNtpScreen);
    refreshTimeSyncScreen();
}

void createWiFiScreen()
{
    wifi_screen = lv_obj_create(nullptr);
    styleScreen(wifi_screen);
    lv_obj_add_event_cb(wifi_screen, markUserActivity,
                        LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(wifi_screen, wifiScreenEventCallback,
                        LV_EVENT_SCREEN_LOAD_START, nullptr);

    lv_obj_t *title = lv_label_create(wifi_screen);
    lv_label_set_text(title, "WI-FI");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(kAccentColor), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 14);

    wifi_status_label = lv_label_create(wifi_screen);
    lv_label_set_text(wifi_status_label, "STATUS: OFF");
    lv_obj_set_style_text_font(wifi_status_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(wifi_status_label,
                                lv_color_hex(kMutedColor), 0);
    lv_obj_align(wifi_status_label, LV_ALIGN_TOP_MID, 0, 54);

    wifi_network_label = lv_label_create(wifi_screen);
    lv_label_set_text(wifi_network_label, "NETWORK: --");
    lv_obj_set_width(wifi_network_label, 210);
    lv_label_set_long_mode(wifi_network_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(wifi_network_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(wifi_network_label,
                               &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(wifi_network_label,
                                lv_color_hex(kMutedColor), 0);
    lv_obj_align(wifi_network_label, LV_ALIGN_TOP_MID, 0, 84);

    lv_obj_t *power_note = lv_label_create(wifi_screen);
    lv_label_set_text(power_note, "WI-FI STOPS IN SLEEP");
    lv_obj_set_style_text_font(power_note, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(power_note, lv_color_hex(kMutedColor), 0);
    lv_obj_align(power_note, LV_ALIGN_TOP_MID, 0, 112);

    wifi_connect_button = createButton(wifi_screen, "CONNECT",
                                       20, 142, 96, 42,
                                       reconnectWiFiCallback);
    wifi_connect_button_label = lv_obj_get_child(wifi_connect_button, 0);
    lv_obj_set_style_bg_color(wifi_connect_button,
                              lv_color_hex(kAccentColor), 0);
    lv_obj_set_style_text_color(wifi_connect_button_label,
                                lv_color_hex(kBackgroundColor), 0);

    wifi_disconnect_button = createButton(wifi_screen, "DISCONNECT",
                                          124, 142, 96, 42,
                                          disconnectWiFiCallback);

    createButton(wifi_screen, "BACK", 20, 202, 200, 30,
                 returnToWiFiNtpScreen);
    refreshWiFiScreen();
}

void createWakeOverlay()
{
    wake_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_pos(wake_overlay, 0, 0);
    lv_obj_set_size(wake_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(wake_overlay, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wake_overlay, 0, 0);
    lv_obj_set_style_pad_all(wake_overlay, 0, 0);
    lv_obj_clear_flag(wake_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(wake_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(wake_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(wake_overlay, wakeOverlayCallback,
                        LV_EVENT_PRESSED, nullptr);
}

void createPowerOffScreen()
{
    power_off_screen = lv_obj_create(nullptr);
    styleScreen(power_off_screen);

    lv_obj_t *title = lv_label_create(power_off_screen);
    lv_label_set_text(title, "GRACEFUL SHUTDOWN");
    lv_obj_set_width(title, 204);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(kPrimaryColor), 0);
    lv_obj_set_pos(title, 18, 24);

    lv_obj_t *status = lv_label_create(power_off_screen);
    lv_label_set_text(status, "CLOSING CONNECTIONS SAFELY...");
    lv_obj_set_width(status, 204);
    lv_label_set_long_mode(status, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(status, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(status, lv_color_hex(kAccentColor), 0);
    lv_obj_set_pos(status, 18, 52);

    lv_obj_t *hint = lv_label_create(power_off_screen);
    lv_label_set_text(hint, "HOLD CROWN TO TURN ON");
    lv_obj_set_width(hint, 204);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(kMutedColor), 0);
    lv_obj_set_pos(hint, 18, 78);
}

void createStartupScreen()
{
    startup_screen = lv_obj_create(nullptr);
    styleScreen(startup_screen);

    lv_obj_t *title = lv_label_create(startup_screen);
    lv_label_set_text(title, "T-WATCH S3 CUSTOM");
    lv_obj_set_width(title, 204);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(kPrimaryColor), 0);
    lv_obj_set_pos(title, 18, 24);

    lv_obj_t *status = lv_label_create(startup_screen);
    lv_label_set_text(status, "INITIALIZING...");
    lv_obj_set_width(status, 204);
    lv_obj_set_style_text_font(status, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(status, lv_color_hex(kAccentColor), 0);
    lv_obj_set_pos(status, 18, 52);
}

}  // namespace

void setup()
{
    Serial.begin(115200);
    setenv("TZ", kTimeZone, 1);
    tzset();
    instance.begin();
    instance.pmu.setPowerKeyPressOffTime(XPOWERS_POWEROFF_4S);
    instance.pmu.setPowerKeyPressOnTime(XPOWERS_POWERON_2S);
    instance.pmu.setLongPressPowerOFF();
    instance.pmu.disableLongPressShutdown();
    configureTiltWake();
    beginLvglHelper(instance);
    loadBrightnessSetting();
    loadTimeSyncSettings();
    loadPowerDisplaySettings();

    createClockScreen();
    createStartupScreen();
    lv_screen_load(startup_screen);
    instance.setBrightness(active_brightness);
    lv_refr_now(nullptr);
    const uint32_t startup_screen_started_ms = millis();

    createSettingsHubScreen();
    createPowerDisplayScreen();
    createTimeoutSettingsScreen();
    createResetSettingsScreen();
    createWiFiNtpScreen();
    createDateTimeScreen();
    createBrightnessScreen();
    createTimeSyncScreen();
    createWiFiScreen();
    createWakeOverlay();
    createPowerOffScreen();
    syncClockFromRtc();
    updateBatteryStatus(nullptr);
    lv_timer_create(updateClock, 250, nullptr);
    lv_timer_create(updateBatteryStatus, kBatteryUpdateIntervalMs, nullptr);
    lv_timer_create(updateWiFiScreenStatus, 1000, nullptr);

    instance.onEvent(deviceEventCallback, POWER_EVENT, nullptr);
    instance.onEvent(deviceEventCallback, SENSOR_EVENT, nullptr);
    sntp_set_time_sync_notification_cb(ntpTimeAvailableCallback);

    while (millis() - startup_screen_started_ms <
           kStartupScreenMinimumMs) {
        lv_timer_handler();
        delay(5);
    }

    lv_screen_load(clock_screen);
    lv_refr_now(nullptr);
    last_activity_ms = millis();
    requestTimeSyncIfDue();
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

    if (!deploy_mode_enabled && screen_on &&
        millis() - last_activity_ms >= currentScreenTimeout()) {
        turnScreenOff();
    }

    if (!deploy_mode_enabled && !screen_on && !isRadioBusy() &&
        millis() - screen_off_ms >= light_sleep_delay_seconds * 1000) {
        enterLightSleep();
    }

    delay(screen_on ? 2 : 20);
}
