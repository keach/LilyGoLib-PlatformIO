#pragma once

#include <stdint.h>
#include <time.h>

#include "lvgl.h"
#include "scheduled_alarm.h"

class ScheduledAlarmScreen {
public:
    using ConfigureCallback = void (*)(uint8_t hour, uint8_t minute,
                                       bool enabled, void *context);
    using StopCallback = void (*)(void *context);
    using ActionCallback = void (*)(void *context);

    ScheduledAlarmScreen(ScheduledAlarm &alarm,
                         uint32_t background_color,
                         uint32_t primary_color,
                         uint32_t accent_color,
                         uint32_t muted_color,
                         uint32_t button_color);

    void create(ConfigureCallback configure_callback,
                void *configure_context,
                StopCallback stop_callback,
                void *stop_context,
                ActionCallback settings_callback,
                void *settings_context,
                ActionCallback back_callback,
                void *back_context);
    lv_obj_t *screen() const;
    void show(bool use_24_hour_clock, bool move_right = false);
    void refresh(bool use_24_hour_clock);

private:
    static void hourDownCallback(lv_event_t *event);
    static void hourUpCallback(lv_event_t *event);
    static void minuteDownCallback(lv_event_t *event);
    static void minuteUpCallback(lv_event_t *event);
    static void saveCallback(lv_event_t *event);
    static void toggleCallback(lv_event_t *event);
    static void settingsCallback(lv_event_t *event);
    static void backCallback(lv_event_t *event);

    lv_obj_t *createButton(const char *text, int x, int y,
                           int width, int height, lv_event_cb_t callback);
    void styleScreen();
    void styleButton(lv_obj_t *button);
    void adjustHour(int delta);
    void adjustMinute(int delta);
    void save();
    void toggle();
    void updateLabels();

    ScheduledAlarm &alarm_;
    uint32_t background_color_;
    uint32_t primary_color_;
    uint32_t accent_color_;
    uint32_t muted_color_;
    uint32_t button_color_;
    bool use_24_hour_clock_ = true;
    uint8_t pending_hour_ = 7;
    uint8_t pending_minute_ = 0;

    lv_obj_t *screen_ = nullptr;
    lv_obj_t *time_label_ = nullptr;
    lv_obj_t *status_label_ = nullptr;
    lv_obj_t *toggle_button_label_ = nullptr;
    ConfigureCallback configure_callback_ = nullptr;
    void *configure_context_ = nullptr;
    StopCallback stop_callback_ = nullptr;
    void *stop_context_ = nullptr;
    ActionCallback settings_callback_ = nullptr;
    void *settings_context_ = nullptr;
    ActionCallback back_callback_ = nullptr;
    void *back_context_ = nullptr;
};
