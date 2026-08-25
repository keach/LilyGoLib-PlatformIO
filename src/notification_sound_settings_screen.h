#pragma once

#include <stdint.h>

#include "lvgl.h"
#include "notification_sound.h"

class NotificationSoundSettingsScreen {
public:
    using SaveCallback = void (*)(NotificationSoundPreset preset,
                                  void *context);
    using PreviewCallback = void (*)(NotificationSoundPreset preset,
                                     void *context);
    using BackCallback = void (*)(void *context);

    NotificationSoundSettingsScreen(uint32_t background_color,
                                    uint32_t primary_color,
                                    uint32_t accent_color,
                                    uint32_t muted_color,
                                    uint32_t button_color);

    void create(SaveCallback save_callback,
                void *save_context,
                PreviewCallback preview_callback,
                void *preview_context,
                BackCallback back_callback,
                void *back_context);
    void show(NotificationSoundPreset preset);
    lv_obj_t *screen() const;

private:
    static void previousCallback(lv_event_t *event);
    static void nextCallback(lv_event_t *event);
    static void previewCallback(lv_event_t *event);
    static void saveCallback(lv_event_t *event);
    static void cancelCallback(lv_event_t *event);

    lv_obj_t *createButton(const char *text, int x, int y,
                           int width, int height, lv_event_cb_t callback);
    void selectPrevious();
    void selectNext();
    void preview();
    void save();
    void goBack();
    void updateLabel();

    uint32_t background_color_;
    uint32_t primary_color_;
    uint32_t accent_color_;
    uint32_t muted_color_;
    uint32_t button_color_;
    lv_obj_t *screen_ = nullptr;
    lv_obj_t *preset_label_ = nullptr;
    NotificationSoundPreset selected_preset_ =
        kDefaultNotificationSoundPreset;
    SaveCallback save_callback_ = nullptr;
    void *save_context_ = nullptr;
    PreviewCallback preview_callback_ = nullptr;
    void *preview_context_ = nullptr;
    BackCallback back_callback_ = nullptr;
    void *back_context_ = nullptr;
};
