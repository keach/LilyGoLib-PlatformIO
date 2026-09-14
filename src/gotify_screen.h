#pragma once

#include <stdint.h>
#include <time.h>

#include "gotify_logic.h"
#include "lvgl.h"

class GotifyScreen {
public:
    using ActionCallback = void (*)(void *context);

    GotifyScreen(uint32_t background_color, uint32_t primary_color,
                 uint32_t accent_color, uint32_t muted_color,
                 uint32_t button_color);
    void create(ActionCallback check_callback, void *check_context,
                ActionCallback settings_callback, void *settings_context,
                ActionCallback back_callback, void *back_context,
                ActionCallback dismiss_callback, void *dismiss_context);
    void show();
    void update(bool configured, bool checking, time_t last_check_epoch,
                const char *result, uint64_t last_message_id);
    void showMessage(const GotifyMessage &message);
    void hideMessage();
    bool messageVisible() const;
    lv_obj_t *screen() const;

private:
    struct Binding { ActionCallback callback; void *context; };
    static void actionCallback(lv_event_t *event);
    lv_obj_t *createButton(lv_obj_t *parent, const char *text, int x, int y,
                           int width, int height, Binding *binding);

    uint32_t background_color_;
    uint32_t primary_color_;
    uint32_t accent_color_;
    uint32_t muted_color_;
    uint32_t button_color_;
    lv_obj_t *screen_ = nullptr;
    lv_obj_t *configuration_label_ = nullptr;
    lv_obj_t *last_check_label_ = nullptr;
    lv_obj_t *result_label_ = nullptr;
    lv_obj_t *message_id_label_ = nullptr;
    lv_obj_t *popup_ = nullptr;
    lv_obj_t *popup_title_ = nullptr;
    lv_obj_t *popup_body_ = nullptr;
    lv_obj_t *popup_date_ = nullptr;
    Binding check_binding_ = {};
    Binding settings_binding_ = {};
    Binding back_binding_ = {};
    Binding dismiss_binding_ = {};
};
