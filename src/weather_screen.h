#pragma once

#include <stdint.h>

#include "lvgl.h"
#include "weather_logic.h"

enum class WeatherScreenState : uint8_t {
    Ready,
    Stale,
    Updating,
    Unavailable,
    Error,
};

class WeatherScreen {
public:
    using ActionCallback = void (*)(void *context);

    WeatherScreen(uint32_t background_color,
                  uint32_t primary_color,
                  uint32_t accent_color,
                  uint32_t muted_color,
                  uint32_t button_color);
    void create(ActionCallback refresh_callback, void *refresh_context,
                ActionCallback back_callback, void *back_context);
    void show();
    void update(const WeatherSnapshot &snapshot, WeatherScreenState state,
                const char *location_name, const char *status_text = nullptr);
    lv_obj_t *screen() const;

private:
    struct Binding { WeatherScreen *self; ActionCallback callback; void *context; };
    static void actionCallback(lv_event_t *event);
    lv_obj_t *createButton(const char *text, int x, int width, Binding *binding);
    lv_obj_t *createLabel(lv_obj_t *parent, const lv_font_t *font,
                          uint32_t color, int y, int width);
    void updateDay(lv_obj_t *label, const char *title,
                   const DailyWeather &weather);

    uint32_t background_color_;
    uint32_t primary_color_;
    uint32_t accent_color_;
    uint32_t muted_color_;
    uint32_t button_color_;
    lv_obj_t *screen_ = nullptr;
    lv_obj_t *location_label_ = nullptr;
    lv_obj_t *updated_label_ = nullptr;
    lv_obj_t *current_label_ = nullptr;
    lv_obj_t *details_label_ = nullptr;
    lv_obj_t *today_label_ = nullptr;
    lv_obj_t *tomorrow_label_ = nullptr;
    lv_obj_t *status_label_ = nullptr;
    Binding refresh_binding_ = {};
    Binding back_binding_ = {};
};
