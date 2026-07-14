/**
 * Minimal T-Watch S3 clock face.
 */
#include <LilyGoLib.h>
#include <LV_Helper.h>

#include <time.h>

namespace {

constexpr uint32_t kBackgroundColor = 0x101820;
constexpr uint32_t kPrimaryColor = 0xF2F5F7;
constexpr uint32_t kAccentColor = 0x55C2FF;
constexpr uint32_t kMutedColor = 0x94A3AD;

const char *const kWeekdays[] = {
    "SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY",
    "THURSDAY", "FRIDAY", "SATURDAY",
};

const char *const kMonths[] = {
    "JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE",
    "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER",
};

lv_obj_t *time_label;
lv_obj_t *weekday_label;
lv_obj_t *date_label;
int last_second = -1;

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

void showClockError(const char *message)
{
    lv_label_set_text(time_label, "--:--:--");
    lv_label_set_text(weekday_label, "CLOCK ERROR");
    lv_label_set_text(date_label, message);
}

void updateClock(lv_timer_t *)
{
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

void createClockScreen()
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(kBackgroundColor), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(kPrimaryColor), 0);

    lv_obj_t *title_label = lv_label_create(screen);
    lv_label_set_text(title_label, "T-WATCH S3");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(kAccentColor), 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 24);

    time_label = lv_label_create(screen);
    lv_label_set_text(time_label, "--:--:--");
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(time_label, lv_color_hex(kPrimaryColor), 0);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, -20);

    weekday_label = lv_label_create(screen);
    lv_obj_set_style_text_font(weekday_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(weekday_label, lv_color_hex(kAccentColor), 0);
    lv_obj_align(weekday_label, LV_ALIGN_CENTER, 0, 30);

    date_label = lv_label_create(screen);
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(date_label, lv_color_hex(kMutedColor), 0);
    lv_obj_align(date_label, LV_ALIGN_CENTER, 0, 62);
}

}  // namespace

void setup()
{
    Serial.begin(115200);
    instance.begin();
    beginLvglHelper(instance);

    createClockScreen();
    updateClock(nullptr);
    lv_timer_create(updateClock, 250, nullptr);

    instance.setBrightness(DEVICE_MAX_BRIGHTNESS_LEVEL);
}

void loop()
{
    lv_timer_handler();
    delay(2);
}
