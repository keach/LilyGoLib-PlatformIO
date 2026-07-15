/**
 * T-Watch S3 clock face with RTC adjustment and NTP synchronization.
 */
#include <LilyGoLib.h>
#include <LV_Helper.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_sntp.h>

#include <stdint.h>
#include <time.h>

#include "wifi_credentials_types.h"

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
constexpr uint32_t kClockScreenTimeoutMs = 15 * 1000;
constexpr uint32_t kSettingsScreenTimeoutMs = 60 * 1000;
constexpr uint32_t kLightSleepDelayMs = 5 * 1000;
constexpr uint32_t kBatteryUpdateIntervalMs = 10 * 1000;
constexpr uint32_t kWiFiConnectTimeoutMs = 10 * 1000;
constexpr uint32_t kNtpSyncTimeoutMs = 15 * 1000;
constexpr uint32_t kWiFiRetryIntervalMs = 15 * 60 * 1000;
constexpr uint32_t kSuccessNotificationMs = 3 * 1000;
constexpr uint32_t kWarningNotificationMs = 8 * 1000;
constexpr time_t kNtpSyncIntervalSeconds = 24 * 60 * 60;
constexpr time_t kMinimumValidEpoch = 1577836800;  // 2020-01-01 UTC
constexpr uint8_t kDefaultBrightness = 180;
constexpr uint8_t kMinimumBrightness = 20;
constexpr uint8_t kMaximumBrightness = 255;
constexpr uint8_t kBrightnessStep = 10;
constexpr int kMinimumYear = 2000;
constexpr int kMaximumYear = 2099;

const char *const kTimeZone = "JST-9";
const char *const kNtpServer1 = "pool.ntp.org";
const char *const kNtpServer2 = "time.google.com";
const char *const kNtpServer3 = "ntp.nict.jp";

const char *const kWeekdays[] = {
    "SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY",
    "THURSDAY", "FRIDAY", "SATURDAY",
};

const char *const kMonths[] = {
    "JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE",
    "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER",
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

const char *const kFieldNames[] = {
    "YEAR", "MONTH", "DAY", "HOUR", "MINUTE", "SECOND",
};

lv_obj_t *clock_screen;
lv_obj_t *settings_screen;
lv_obj_t *brightness_screen;
lv_obj_t *hour_label;
lv_obj_t *minute_label;
lv_obj_t *second_label;
lv_obj_t *weekday_label;
lv_obj_t *date_label;
lv_obj_t *battery_label;
lv_obj_t *time_sync_label;
lv_obj_t *settings_status_label;
lv_obj_t *brightness_slider;
lv_obj_t *brightness_value_label;
lv_obj_t *brightness_status_label;
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
uint8_t active_brightness = kDefaultBrightness;
uint8_t pending_brightness = kDefaultBrightness;
TimeSyncState time_sync_state = TimeSyncState::Idle;
size_t wifi_credential_index = 0;
uint32_t time_sync_state_started_ms = 0;
uint32_t wifi_retry_not_before_ms = 0;
uint32_t time_sync_notification_hide_ms = 0;
time_t last_ntp_sync_epoch = 0;
volatile bool ntp_sync_received = false;
bool ntp_config_warning_shown = false;

void syncClockFromRtc();
void updateBatteryStatus(lv_timer_t *);
void requestTimeSyncIfDue();
bool isTimeSyncBusy();

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

void enterLightSleep()
{
    Serial.println("Entering light sleep");
    Serial.flush();

    instance.lightSleep(static_cast<WakeupSource_t>(
        WAKEUP_SRC_POWER_KEY | WAKEUP_SRC_TOUCH_PANEL));

    const bool woke_by_touch =
        esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1 &&
        (esp_sleep_get_ext1_wakeup_status() & (1ULL << TP_INT)) != 0;

    // Keep the overlay in place for a touch wake so the same physical touch
    // cannot activate the SET button or another control after resume.
    wakeScreen(woke_by_touch);
    Serial.printf("Light sleep wake: %s\n",
                  woke_by_touch ? "touch" : "power button");
}

void deviceEventCallback(DeviceEvent_t event, void *params, void *)
{
    if (event != POWER_EVENT) {
        return;
    }

    const PMUEventType_t power_event = instance.getPMUEventType(params);
    if (power_event == PMU_EVENT_KEY_CLICKED) {
        wakeScreen();
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
    return lv_screen_active() == clock_screen
               ? kClockScreenTimeoutMs
               : kSettingsScreenTimeoutMs;
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
    lv_label_set_text(weekday_label, "CLOCK ERROR");
    lv_label_set_text(date_label, message);
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

    lv_label_set_text_fmt(hour_label, "%02d", timeinfo.tm_hour);
    lv_label_set_text_fmt(minute_label, "%02d", timeinfo.tm_min);
    lv_label_set_text_fmt(second_label, "%02d", timeinfo.tm_sec);
    lv_label_set_text(weekday_label, kWeekdays[timeinfo.tm_wday]);
    lv_label_set_text_fmt(date_label, "%s %02d, %04d",
                          kMonths[timeinfo.tm_mon], timeinfo.tm_mday,
                          timeinfo.tm_year + 1900);
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

void loadLastNtpSync()
{
    Preferences preferences;
    if (!preferences.begin("clock_sync", true)) {
        Serial.println("Unable to read NTP sync history");
        return;
    }
    last_ntp_sync_epoch = static_cast<time_t>(
        preferences.getULong64("last_ntp", 0));
    preferences.end();
}

void saveLastNtpSync()
{
    Preferences preferences;
    if (!preferences.begin("clock_sync", false)) {
        Serial.println("Unable to save NTP sync history");
        return;
    }
    preferences.putULong64("last_ntp",
                           static_cast<uint64_t>(last_ntp_sync_epoch));
    preferences.end();
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
    WiFi.disconnect(true, false);
    time_sync_state = TimeSyncState::Idle;
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
            Serial.printf("Connecting to Wi-Fi network %u of %u\n",
                          static_cast<unsigned>(wifi_credential_index + 1),
                          static_cast<unsigned>(kWiFiCredentialCount));
            return true;
        }
        ++wifi_credential_index;
    }
    return false;
}

void failTimeSync(const char *reason)
{
    Serial.printf("Time sync failed: %s\n", reason);
    stopTimeSyncRadio();
    wifi_retry_not_before_ms = millis() + kWiFiRetryIntervalMs;
    setTimeSyncStatus("NTP SYNC FAILED", kLowBatteryColor,
                      kWarningNotificationMs);
}

void requestTimeSyncIfDue()
{
    if (isTimeSyncBusy()) {
        return;
    }
    if (kWiFiCredentialCount == 0) {
        if (!ntp_config_warning_shown) {
            ntp_config_warning_shown = true;
            setTimeSyncStatus("NTP NOT CONFIGURED", kMutedColor,
                              kWarningNotificationMs);
        }
        return;
    }
    if (!isNtpSyncDue()) {
        hideTimeSyncNotification();
        return;
    }
    if (wifi_retry_not_before_ms != 0 &&
        static_cast<int32_t>(millis() - wifi_retry_not_before_ms) < 0) {
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    wifi_credential_index = 0;
    if (!startCurrentWiFiNetwork()) {
        failTimeSync("no usable Wi-Fi credentials");
    }
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
    saveLastNtpSync();
    stopTimeSyncRadio();
    syncClockFromRtc();
    setTimeSyncStatus("TIME SYNCED", kAccentColor,
                      kSuccessNotificationMs);
    Serial.println("NTP time written to RTC");
}

void processTimeSync()
{
    switch (time_sync_state) {
    case TimeSyncState::Idle:
        return;

    case TimeSyncState::Connecting:
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("Wi-Fi connected: %s\n", WiFi.SSID().c_str());
            ntp_sync_received = false;
            time_sync_state = TimeSyncState::WaitingForNtp;
            time_sync_state_started_ms = millis();
            setTimeSyncStatus("SYNCING TIME", kAccentColor);
            configTzTime(kTimeZone, kNtpServer1, kNtpServer2, kNtpServer3);
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
    showClockScreen(nullptr);
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
    showClockScreen(nullptr);
}

void saveSettingsCallback(lv_event_t *)
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
    showClockScreen(nullptr);
}

void settingsScreenEventCallback(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_SCREEN_LOAD_START) {
        last_activity_ms = millis();
        loadSettingValues();
    }
}

void showSettingsScreen(lv_event_t *)
{
    lv_screen_load_anim(settings_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT,
                        180, 0, false);
}

void clockScreenEventCallback(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_SCREEN_LOADED) {
        syncClockFromRtc();
        updateBatteryStatus(nullptr);
        requestTimeSyncIfDue();
    }
}

lv_obj_t *createClockTimeLabel(const char *text, int x_offset, int width)
{
    lv_obj_t *label = lv_label_create(clock_screen);
    lv_label_set_text(label, text);
    lv_obj_set_width(label, width);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_40, 0);
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

    createButton(clock_screen, "BRI", 126, 14, 44, 30,
                 showBrightnessScreen);
    createButton(clock_screen, "SET", 176, 14, 50, 30,
                 showSettingsScreen);

    battery_label = lv_label_create(clock_screen);
    lv_label_set_text(battery_label, "N/A");
    lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(battery_label, lv_color_hex(kMutedColor), 0);
    lv_obj_align(battery_label, LV_ALIGN_TOP_LEFT, 18, 24);

    hour_label = createClockTimeLabel("--", -72, 60);
    createClockTimeLabel(":", -36, 12);
    minute_label = createClockTimeLabel("--", 0, 60);
    createClockTimeLabel(":", 36, 12);
    second_label = createClockTimeLabel("--", 72, 60);

    weekday_label = lv_label_create(clock_screen);
    lv_obj_set_style_text_font(weekday_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(weekday_label, lv_color_hex(kAccentColor), 0);
    lv_obj_align(weekday_label, LV_ALIGN_CENTER, 0, 30);

    date_label = lv_label_create(clock_screen);
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(date_label, lv_color_hex(kMutedColor), 0);
    lv_obj_align(date_label, LV_ALIGN_CENTER, 0, 62);

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
    lv_obj_t *button = lv_button_create(settings_screen);
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

void createSettingsScreen()
{
    settings_screen = lv_obj_create(nullptr);
    styleScreen(settings_screen);
    lv_obj_add_event_cb(settings_screen, markUserActivity,
                        LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(settings_screen, settingsScreenEventCallback,
                        LV_EVENT_SCREEN_LOAD_START, nullptr);

    lv_obj_t *title = lv_label_create(settings_screen);
    lv_label_set_text(title, "SET DATE & TIME");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(kAccentColor), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    lv_obj_t *date_heading = lv_label_create(settings_screen);
    lv_label_set_text(date_heading, "DATE");
    lv_obj_set_style_text_font(date_heading, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(date_heading, lv_color_hex(kMutedColor), 0);
    lv_obj_set_pos(date_heading, 6, 48);

    createFieldButton(SettingField::Year, 48, 36, 76);
    createFieldButton(SettingField::Month, 130, 36, 48);
    createFieldButton(SettingField::Day, 184, 36, 48);

    lv_obj_t *time_heading = lv_label_create(settings_screen);
    lv_label_set_text(time_heading, "TIME");
    lv_obj_set_style_text_font(time_heading, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(time_heading, lv_color_hex(kMutedColor), 0);
    lv_obj_set_pos(time_heading, 6, 98);

    createFieldButton(SettingField::Hour, 48, 86, 48);
    createFieldButton(SettingField::Minute, 110, 86, 48);
    createFieldButton(SettingField::Second, 172, 86, 48);

    settings_status_label = lv_label_create(settings_screen);
    lv_obj_set_style_text_font(settings_status_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(settings_status_label,
                                lv_color_hex(kMutedColor), 0);
    lv_obj_align(settings_status_label, LV_ALIGN_TOP_MID, 0, 132);

    lv_obj_t *decrement = createButton(settings_screen, LV_SYMBOL_MINUS,
                                       35, 151, 75, 40,
                                       decrementButtonCallback);
    lv_obj_t *increment = createButton(settings_screen, LV_SYMBOL_PLUS,
                                       130, 151, 75, 40,
                                       incrementButtonCallback);
    lv_obj_add_event_cb(decrement, decrementButtonCallback,
                        LV_EVENT_LONG_PRESSED_REPEAT, nullptr);
    lv_obj_add_event_cb(increment, incrementButtonCallback,
                        LV_EVENT_LONG_PRESSED_REPEAT, nullptr);

    createButton(settings_screen, "CANCEL", 15, 202, 100, 30,
                 showClockScreen);
    lv_obj_t *save = createButton(settings_screen, "SAVE", 125, 202, 100, 30,
                                  saveSettingsCallback);
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

}  // namespace

void setup()
{
    Serial.begin(115200);
    setenv("TZ", kTimeZone, 1);
    tzset();
    instance.begin();
    beginLvglHelper(instance);
    loadBrightnessSetting();

    createClockScreen();
    createSettingsScreen();
    createBrightnessScreen();
    createWakeOverlay();
    syncClockFromRtc();
    updateBatteryStatus(nullptr);
    lv_timer_create(updateClock, 250, nullptr);
    lv_timer_create(updateBatteryStatus, kBatteryUpdateIntervalMs, nullptr);

    instance.onEvent(deviceEventCallback, POWER_EVENT, nullptr);
    sntp_set_time_sync_notification_cb(ntpTimeAvailableCallback);
    loadLastNtpSync();
    last_activity_ms = millis();
    instance.setBrightness(active_brightness);
    requestTimeSyncIfDue();
}

void loop()
{
    instance.loop();
    lv_timer_handler();
    processTimeSync();
    updateTimeSyncNotification();

    if (screen_on && millis() - last_activity_ms >= currentScreenTimeout()) {
        turnScreenOff();
    }

    if (!screen_on && !isTimeSyncBusy() &&
        millis() - screen_off_ms >= kLightSleepDelayMs) {
        enterLightSleep();
    }

    delay(screen_on ? 2 : 20);
}
