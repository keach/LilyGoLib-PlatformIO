/**
 * Minimal T-Watch S3 clock face with manual RTC adjustment.
 */
#include <LilyGoLib.h>
#include <LV_Helper.h>

#include <stdint.h>
#include <time.h>

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
constexpr uint8_t kActiveBrightness = 180;
constexpr int kMinimumYear = 2000;
constexpr int kMaximumYear = 2099;

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

const char *const kFieldNames[] = {
    "YEAR", "MONTH", "DAY", "HOUR", "MINUTE", "SECOND",
};

lv_obj_t *clock_screen;
lv_obj_t *settings_screen;
lv_obj_t *time_label;
lv_obj_t *weekday_label;
lv_obj_t *date_label;
lv_obj_t *battery_label;
lv_obj_t *settings_status_label;
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

void syncClockFromRtc();
void updateBatteryStatus(lv_timer_t *);

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
    instance.setBrightness(kActiveBrightness);
    syncClockFromRtc();
    updateBatteryStatus(nullptr);
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
    return lv_screen_active() == settings_screen
               ? kSettingsScreenTimeoutMs
               : kClockScreenTimeoutMs;
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
    lv_label_set_text(time_label, "--:--:--");
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

    lv_label_set_text_fmt(time_label, "%02d:%02d:%02d",
                          timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
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
        lv_label_set_text(battery_label, "BATTERY N/A");
        lv_obj_set_style_text_color(battery_label, lv_color_hex(kMutedColor), 0);
        return;
    }

    const int percent = instance.pmu.getBatteryPercent();
    const bool battery_connected = instance.pmu.isBatteryConnect();
    const bool usb_connected = instance.pmu.isVbusIn();
    const bool charging = instance.pmu.isCharging();

    if (!battery_connected || percent < 0 || percent > 100) {
        lv_label_set_text(battery_label,
                          usb_connected ? LV_SYMBOL_USB " USB POWER"
                                        : "BATTERY N/A");
        lv_obj_set_style_text_color(battery_label, lv_color_hex(kMutedColor), 0);
        return;
    }

    if (charging) {
        lv_label_set_text_fmt(battery_label, LV_SYMBOL_CHARGE " %d%% CHARGING",
                              percent);
        lv_obj_set_style_text_color(battery_label, lv_color_hex(kAccentColor), 0);
    } else if (usb_connected && percent >= 100) {
        lv_label_set_text_fmt(battery_label,
                              LV_SYMBOL_BATTERY_FULL " %d%% FULL", percent);
        lv_obj_set_style_text_color(battery_label, lv_color_hex(kAccentColor), 0);
    } else if (usb_connected) {
        lv_label_set_text_fmt(battery_label, LV_SYMBOL_USB " %d%% POWERED",
                              percent);
        lv_obj_set_style_text_color(battery_label, lv_color_hex(kMutedColor), 0);
    } else {
        lv_label_set_text_fmt(battery_label, "%s %d%%",
                              batterySymbol(percent), percent);
        const uint32_t color = percent < 20 ? kLowBatteryColor : kMutedColor;
        lv_obj_set_style_text_color(battery_label, lv_color_hex(color), 0);
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
    }
}

void createClockScreen()
{
    clock_screen = lv_screen_active();
    styleScreen(clock_screen);
    lv_obj_add_event_cb(clock_screen, markUserActivity,
                        LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(clock_screen, clockScreenEventCallback,
                        LV_EVENT_SCREEN_LOADED, nullptr);

    lv_obj_t *title_label = lv_label_create(clock_screen);
    lv_label_set_text(title_label, "T-WATCH S3");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(kAccentColor), 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 18, 24);

    createButton(clock_screen, "SET", 176, 14, 50, 30,
                 showSettingsScreen);

    time_label = lv_label_create(clock_screen);
    lv_label_set_text(time_label, "--:--:--");
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(time_label, lv_color_hex(kPrimaryColor), 0);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, -20);

    weekday_label = lv_label_create(clock_screen);
    lv_obj_set_style_text_font(weekday_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(weekday_label, lv_color_hex(kAccentColor), 0);
    lv_obj_align(weekday_label, LV_ALIGN_CENTER, 0, 30);

    date_label = lv_label_create(clock_screen);
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(date_label, lv_color_hex(kMutedColor), 0);
    lv_obj_align(date_label, LV_ALIGN_CENTER, 0, 62);

    battery_label = lv_label_create(clock_screen);
    lv_label_set_text(battery_label, "BATTERY N/A");
    lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(battery_label, lv_color_hex(kMutedColor), 0);
    lv_obj_align(battery_label, LV_ALIGN_BOTTOM_MID, 0, -14);
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
    instance.begin();
    beginLvglHelper(instance);

    createClockScreen();
    createSettingsScreen();
    createWakeOverlay();
    syncClockFromRtc();
    updateBatteryStatus(nullptr);
    lv_timer_create(updateClock, 250, nullptr);
    lv_timer_create(updateBatteryStatus, kBatteryUpdateIntervalMs, nullptr);

    instance.onEvent(deviceEventCallback, POWER_EVENT, nullptr);
    last_activity_ms = millis();
    instance.setBrightness(kActiveBrightness);
}

void loop()
{
    instance.loop();
    lv_timer_handler();

    if (screen_on && millis() - last_activity_ms >= currentScreenTimeout()) {
        turnScreenOff();
    }

    if (!screen_on && millis() - screen_off_ms >= kLightSleepDelayMs) {
        enterLightSleep();
    }

    delay(screen_on ? 2 : 20);
}
