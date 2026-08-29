#pragma once

#include <stdint.h>

#include "lvgl.h"
#include "pomodoro_timer.h"

class PomodoroTimerScreen {
public:
    using BackCallback = void (*)(void *context);
    using SettingsCallback = void (*)(void *context);

    PomodoroTimerScreen(PomodoroTimer &timer,
                        uint32_t background_color,
                        uint32_t primary_color,
                        uint32_t accent_color,
                        uint32_t muted_color,
                        uint32_t button_color);

    void create(BackCallback back_callback, void *back_context,
                SettingsCallback settings_callback,
                void *settings_context);
    lv_obj_t *screen() const;
    void refresh(uint32_t now_ms);

private:
    static void primaryActionCallback(lv_event_t *event);
    static void endCallback(lv_event_t *event);
    static void settingsCallback(lv_event_t *event);
    static void backCallback(lv_event_t *event);
    static void screenLoadCallback(lv_event_t *event);

    lv_obj_t *createButton(const char *text, int x, int y,
                           int width, int height, lv_event_cb_t callback);
    void styleScreen();
    void styleButton(lv_obj_t *button);
    void primaryAction();
    void endSession();
    void showSettings();
    void goBack();
    void updateLabels(uint32_t now_ms);

    PomodoroTimer &timer_;
    uint32_t background_color_;
    uint32_t primary_color_;
    uint32_t accent_color_;
    uint32_t muted_color_;
    uint32_t button_color_;

    lv_obj_t *screen_ = nullptr;
    lv_obj_t *phase_label_ = nullptr;
    lv_obj_t *time_label_ = nullptr;
    lv_obj_t *state_label_ = nullptr;
    lv_obj_t *sessions_label_ = nullptr;
    lv_obj_t *primary_button_label_ = nullptr;
    BackCallback back_callback_ = nullptr;
    void *back_context_ = nullptr;
    SettingsCallback settings_callback_ = nullptr;
    void *settings_context_ = nullptr;
};
