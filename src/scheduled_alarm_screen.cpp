#include "scheduled_alarm_screen.h"

#include <stdio.h>

ScheduledAlarmScreen::ScheduledAlarmScreen(
    ScheduledAlarm &alarm,
    uint32_t background_color,
    uint32_t primary_color,
    uint32_t accent_color,
    uint32_t muted_color,
    uint32_t button_color)
    : alarm_(alarm),
      background_color_(background_color),
      primary_color_(primary_color),
      accent_color_(accent_color),
      muted_color_(muted_color),
      button_color_(button_color)
{
}

void ScheduledAlarmScreen::create(
    ConfigureCallback configure_callback, void *configure_context,
    StopCallback stop_callback, void *stop_context,
    ActionCallback settings_callback, void *settings_context,
    ActionCallback back_callback, void *back_context)
{
    configure_callback_ = configure_callback;
    configure_context_ = configure_context;
    stop_callback_ = stop_callback;
    stop_context_ = stop_context;
    settings_callback_ = settings_callback;
    settings_context_ = settings_context;
    back_callback_ = back_callback;
    back_context_ = back_context;

    screen_ = lv_obj_create(nullptr);
    styleScreen();

    lv_obj_t *title = lv_label_create(screen_);
    lv_label_set_text(title, "ALARM");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(accent_color_), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    time_label_ = lv_label_create(screen_);
    lv_label_set_text(time_label_, "07:00");
    lv_obj_set_style_text_font(time_label_, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(time_label_, lv_color_hex(primary_color_), 0);
    lv_obj_align(time_label_, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t *hour_label = lv_label_create(screen_);
    lv_label_set_text(hour_label, "HOUR");
    lv_obj_set_style_text_font(hour_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(hour_label, lv_color_hex(muted_color_), 0);
    lv_obj_set_pos(hour_label, 48, 76);

    lv_obj_t *minute_label = lv_label_create(screen_);
    lv_label_set_text(minute_label, "MINUTE");
    lv_obj_set_style_text_font(minute_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(minute_label, lv_color_hex(muted_color_), 0);
    lv_obj_set_pos(minute_label, 153, 76);

    createButton("-", 20, 91, 42, 31, hourDownCallback);
    createButton("+", 66, 91, 42, 31, hourUpCallback);
    createButton("-", 132, 91, 42, 31, minuteDownCallback);
    createButton("+", 178, 91, 42, 31, minuteUpCallback);

    status_label_ = lv_label_create(screen_);
    lv_label_set_text(status_label_, "DISABLED");
    lv_obj_set_width(status_label_, 220);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(status_label_, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(status_label_, lv_color_hex(muted_color_), 0);
    lv_obj_set_pos(status_label_, 10, 127);

    createButton("SAVE", 10, 146, 108, 34, saveCallback);
    lv_obj_t *toggle_button = createButton(
        "ENABLE", 122, 146, 108, 34, toggleCallback);
    toggle_button_label_ = lv_obj_get_child(toggle_button, 0);

    createButton("SETTINGS", 10, 190, 108, 30, settingsCallback);
    createButton("BACK", 122, 190, 108, 30, backCallback);
}

lv_obj_t *ScheduledAlarmScreen::screen() const
{
    return screen_;
}

void ScheduledAlarmScreen::show(bool use_24_hour_clock, bool move_right)
{
    use_24_hour_clock_ = use_24_hour_clock;
    pending_hour_ = alarm_.hour();
    pending_minute_ = alarm_.minute();
    updateLabels();
    lv_screen_load_anim(screen_,
                        move_right ? LV_SCR_LOAD_ANIM_MOVE_RIGHT
                                   : LV_SCR_LOAD_ANIM_MOVE_LEFT,
                        180, 0, false);
}

void ScheduledAlarmScreen::refresh(bool use_24_hour_clock)
{
    use_24_hour_clock_ = use_24_hour_clock;
    updateLabels();
}

#define ALARM_EVENT_CALLBACK(name, method)                                   \
    void ScheduledAlarmScreen::name(lv_event_t *event)                       \
    {                                                                         \
        auto *self = static_cast<ScheduledAlarmScreen *>(                     \
            lv_obj_get_user_data(lv_event_get_current_target_obj(event)));    \
        if (self != nullptr) {                                                 \
            self->method;                                                      \
        }                                                                      \
    }

ALARM_EVENT_CALLBACK(hourDownCallback, adjustHour(-1))
ALARM_EVENT_CALLBACK(hourUpCallback, adjustHour(1))
ALARM_EVENT_CALLBACK(minuteDownCallback, adjustMinute(-1))
ALARM_EVENT_CALLBACK(minuteUpCallback, adjustMinute(1))
ALARM_EVENT_CALLBACK(saveCallback, save())
ALARM_EVENT_CALLBACK(toggleCallback, toggle())

#undef ALARM_EVENT_CALLBACK

void ScheduledAlarmScreen::settingsCallback(lv_event_t *event)
{
    auto *self = static_cast<ScheduledAlarmScreen *>(
        lv_obj_get_user_data(lv_event_get_current_target_obj(event)));
    if (self != nullptr && self->settings_callback_ != nullptr) {
        self->settings_callback_(self->settings_context_);
    }
}

void ScheduledAlarmScreen::backCallback(lv_event_t *event)
{
    auto *self = static_cast<ScheduledAlarmScreen *>(
        lv_obj_get_user_data(lv_event_get_current_target_obj(event)));
    if (self != nullptr && self->back_callback_ != nullptr) {
        self->back_callback_(self->back_context_);
    }
}

lv_obj_t *ScheduledAlarmScreen::createButton(
    const char *text, int x, int y, int width, int height,
    lv_event_cb_t callback)
{
    lv_obj_t *button = lv_button_create(screen_);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    lv_obj_set_user_data(button, this);
    styleButton(button);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    lv_obj_center(label);
    return button;
}

void ScheduledAlarmScreen::styleScreen()
{
    lv_obj_set_style_bg_color(screen_, lv_color_hex(background_color_), 0);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(screen_, lv_color_hex(primary_color_), 0);
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);
}

void ScheduledAlarmScreen::styleButton(lv_obj_t *button)
{
    lv_obj_set_style_bg_color(button, lv_color_hex(button_color_), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_radius(button, 8, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_pad_all(button, 0, 0);
}

void ScheduledAlarmScreen::adjustHour(int delta)
{
    if (alarm_.alerting()) {
        return;
    }
    pending_hour_ = static_cast<uint8_t>((pending_hour_ + 24 + delta) % 24);
    updateLabels();
}

void ScheduledAlarmScreen::adjustMinute(int delta)
{
    if (alarm_.alerting()) {
        return;
    }
    pending_minute_ = static_cast<uint8_t>(
        (pending_minute_ + 60 + delta) % 60);
    updateLabels();
}

void ScheduledAlarmScreen::save()
{
    if (!alarm_.alerting() && configure_callback_ != nullptr) {
        configure_callback_(pending_hour_, pending_minute_,
                            alarm_.enabled(), configure_context_);
    }
}

void ScheduledAlarmScreen::toggle()
{
    if (alarm_.alerting()) {
        if (stop_callback_ != nullptr) {
            stop_callback_(stop_context_);
        }
    } else if (configure_callback_ != nullptr) {
        configure_callback_(pending_hour_, pending_minute_,
                            !alarm_.enabled(), configure_context_);
    }
}

void ScheduledAlarmScreen::updateLabels()
{
    if (screen_ == nullptr) {
        return;
    }
    uint8_t display_hour = pending_hour_;
    const char *suffix = "";
    if (!use_24_hour_clock_) {
        suffix = pending_hour_ < 12 ? " AM" : " PM";
        display_hour = pending_hour_ % 12;
        if (display_hour == 0) {
            display_hour = 12;
        }
    }
    lv_label_set_text_fmt(time_label_, "%02u:%02u%s",
                          display_hour, pending_minute_, suffix);

    if (alarm_.alerting()) {
        lv_label_set_text(status_label_, "ALARM ACTIVE");
        lv_label_set_text(toggle_button_label_, "STOP ALARM");
        lv_obj_set_style_text_color(status_label_, lv_color_hex(accent_color_), 0);
        return;
    }
    if (!alarm_.enabled()) {
        lv_label_set_text(status_label_, "DISABLED");
        lv_label_set_text(toggle_button_label_, "ENABLE");
        lv_obj_set_style_text_color(status_label_, lv_color_hex(muted_color_), 0);
        return;
    }
    struct tm trigger_time = {};
    const time_t trigger = alarm_.triggerEpoch();
    if (localtime_r(&trigger, &trigger_time) != nullptr) {
        lv_label_set_text_fmt(status_label_, "NEXT %04d.%02d.%02d",
                              trigger_time.tm_year + 1900,
                              trigger_time.tm_mon + 1,
                              trigger_time.tm_mday);
    } else {
        lv_label_set_text(status_label_, "ENABLED");
    }
    lv_label_set_text(toggle_button_label_, "DISABLE");
    lv_obj_set_style_text_color(status_label_, lv_color_hex(muted_color_), 0);
}
