#pragma once

#include <stdint.h>

#include "lvgl.h"

class AppHubScreen {
public:
    using ActionCallback = void (*)(void *context);

    AppHubScreen(uint32_t background_color,
                 uint32_t primary_color,
                 uint32_t accent_color,
                 uint32_t muted_color,
                 uint32_t button_color);

    void create(ActionCallback kitchen_timer_callback,
                void *kitchen_timer_context,
                ActionCallback alarm_volume_callback,
                void *alarm_volume_context,
                ActionCallback back_callback,
                void *back_context);

    lv_obj_t *screen() const;
    void show(bool returning_from_child = false);

private:
    struct ActionBinding {
        AppHubScreen *screen;
        ActionCallback callback;
        void *context;
    };

    static void actionCallback(lv_event_t *event);

    lv_obj_t *createButton(const char *text,
                           int x,
                           int y,
                           int width,
                           int height,
                           ActionBinding *binding);
    void styleScreen();
    void styleButton(lv_obj_t *button);

    uint32_t background_color_;
    uint32_t primary_color_;
    uint32_t accent_color_;
    uint32_t muted_color_;
    uint32_t button_color_;

    lv_obj_t *screen_ = nullptr;
    ActionBinding kitchen_timer_binding_ = {};
    ActionBinding alarm_volume_binding_ = {};
    ActionBinding back_binding_ = {};
};
