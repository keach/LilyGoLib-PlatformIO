#pragma once

#include <stdint.h>

#include "end_notification.h"
#include "lvgl.h"

class NotificationSettingsScreen {
public:
    using SaveCallback = void (*)(NotificationMode mode, void *context);
    using BackCallback = void (*)(void *context);

    NotificationSettingsScreen(uint32_t background_color,
                               uint32_t primary_color,
                               uint32_t accent_color,
                               uint32_t muted_color,
                               uint32_t button_color);

    void create(const char *title,
                SaveCallback save_callback,
                void *save_context,
                BackCallback back_callback,
                void *back_context);
    void show(NotificationMode mode);
    lv_obj_t *screen() const;

private:
    static void modeCallback(lv_event_t *event);
    static void saveCallback(lv_event_t *event);
    static void cancelCallback(lv_event_t *event);

    lv_obj_t *createButton(const char *text, int x, int y,
                           int width, int height, lv_event_cb_t callback,
                           void *user_data = nullptr);
    void selectMode(NotificationMode mode);
    void updateSelection();
    void save();
    void goBack();

    uint32_t background_color_;
    uint32_t primary_color_;
    uint32_t accent_color_;
    uint32_t muted_color_;
    uint32_t button_color_;
    lv_obj_t *screen_ = nullptr;
    lv_obj_t *mode_buttons_[3] = {};
    NotificationMode selected_mode_ = NotificationMode::SoundAndVibration;
    SaveCallback save_callback_ = nullptr;
    void *save_context_ = nullptr;
    BackCallback back_callback_ = nullptr;
    void *back_context_ = nullptr;
};
