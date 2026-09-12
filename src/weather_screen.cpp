#include "weather_screen.h"

#include <stdio.h>
#include <time.h>

#include "japanese_font.h"

namespace {
int roundedTemperature(int16_t tenths)
{
    return tenths >= 0 ? (tenths + 5) / 10 : (tenths - 5) / 10;
}
}

WeatherScreen::WeatherScreen(uint32_t background_color,
                             uint32_t primary_color,
                             uint32_t accent_color,
                             uint32_t muted_color,
                             uint32_t button_color)
    : background_color_(background_color), primary_color_(primary_color),
      accent_color_(accent_color), muted_color_(muted_color),
      button_color_(button_color) {}

void WeatherScreen::create(ActionCallback refresh_callback,
                           void *refresh_context,
                           ActionCallback back_callback,
                           void *back_context)
{
    refresh_binding_ = {this, refresh_callback, refresh_context};
    back_binding_ = {this, back_callback, back_context};
    screen_ = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(background_color_), 0);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);

    auto *title = createLabel(screen_, &lv_font_montserrat_18,
                              accent_color_, 7, 220);
    lv_label_set_text(title, "WEATHER");
    location_label_ = createLabel(screen_, japaneseFont16(), primary_color_,
                                  30, 220);
    updated_label_ = createLabel(screen_, &lv_font_montserrat_12,
                                 muted_color_, 51, 220);
    current_label_ = createLabel(screen_, japaneseFont16(), primary_color_,
                                 73, 220);
    details_label_ = createLabel(screen_, japaneseFont16(), muted_color_,
                                 97, 220);
    today_label_ = createLabel(screen_, japaneseFont16(), primary_color_,
                               122, 108);
    lv_obj_align(today_label_, LV_ALIGN_TOP_LEFT, 6, 122);
    tomorrow_label_ = createLabel(screen_, japaneseFont16(), primary_color_,
                                  122, 108);
    lv_obj_align(tomorrow_label_, LV_ALIGN_TOP_RIGHT, -6, 122);
    status_label_ = createLabel(screen_, &lv_font_montserrat_12,
                                accent_color_, 177, 220);
    createButton("REFRESH", 10, 108, &refresh_binding_);
    createButton("BACK", 122, 108, &back_binding_);
}

lv_obj_t *WeatherScreen::createLabel(lv_obj_t *parent, const lv_font_t *font,
                                     uint32_t color, int y, int width)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_width(label, width);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
    return label;
}

lv_obj_t *WeatherScreen::createButton(const char *text, int x, int width,
                                      Binding *binding)
{
    lv_obj_t *button = lv_button_create(screen_);
    lv_obj_set_pos(button, x, 204);
    lv_obj_set_size(button, width, 30);
    lv_obj_set_user_data(button, binding);
    lv_obj_set_style_bg_color(button, lv_color_hex(button_color_), 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_radius(button, 8, 0);
    lv_obj_add_event_cb(button, actionCallback, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    lv_obj_center(label);
    return button;
}

void WeatherScreen::actionCallback(lv_event_t *event)
{
    auto *binding = static_cast<Binding *>(
        lv_obj_get_user_data(lv_event_get_current_target_obj(event)));
    if (binding != nullptr && binding->callback != nullptr)
        binding->callback(binding->context);
}

void WeatherScreen::show()
{
    lv_screen_load_anim(screen_, LV_SCR_LOAD_ANIM_MOVE_LEFT, 180, 0, false);
}

void WeatherScreen::updateDay(lv_obj_t *label, const char *title,
                              const DailyWeather &weather)
{
    if (!weather.valid) {
        lv_label_set_text_fmt(label, "%s  --", title);
        return;
    }
    lv_label_set_text_fmt(label, "%s  %s\n%d℃ / %d℃\n降水 %u%%", title,
                          weatherConditionJapanese(weather.condition_id),
                          roundedTemperature(weather.maximum_temperature_tenths),
                          roundedTemperature(weather.minimum_temperature_tenths),
                          static_cast<unsigned>(weather.maximum_precipitation_percent));
}

void WeatherScreen::update(const WeatherSnapshot &snapshot,
                           WeatherScreenState state,
                           const char *location_name,
                           const char *status_text)
{
    lv_label_set_text(location_label_,
                      location_name != nullptr ? location_name : "");
    if (snapshot.valid) {
        struct tm updated = {};
        localtime_r(&snapshot.updated_epoch, &updated);
        lv_label_set_text_fmt(updated_label_, "UPDATED %02d:%02d",
                              updated.tm_hour, updated.tm_min);
        lv_label_set_text_fmt(current_label_, "%s  %d℃",
            weatherConditionJapanese(snapshot.current_condition_id),
            roundedTemperature(snapshot.current_temperature_tenths));
        lv_label_set_text_fmt(details_label_, "体感 %d℃  湿度 %u%%",
            roundedTemperature(snapshot.feels_like_temperature_tenths),
            static_cast<unsigned>(snapshot.humidity_percent));
        updateDay(today_label_, "今日", snapshot.today);
        updateDay(tomorrow_label_, "明日", snapshot.tomorrow);
    } else {
        lv_label_set_text(updated_label_, "NOT UPDATED");
        lv_label_set_text(current_label_, "天気を取得してください");
        lv_label_set_text(details_label_, "");
        lv_label_set_text(today_label_, "今日  --");
        lv_label_set_text(tomorrow_label_, "明日  --");
    }
    const char *state_text = "";
    switch (state) {
    case WeatherScreenState::Updating: state_text = "UPDATING..."; break;
    case WeatherScreenState::Stale: state_text = "STALE - REFRESH"; break;
    case WeatherScreenState::Unavailable: state_text = "NO WEATHER DATA"; break;
    case WeatherScreenState::Error: state_text = status_text != nullptr ? status_text : "UPDATE FAILED"; break;
    default: state_text = status_text != nullptr ? status_text : ""; break;
    }
    lv_label_set_text(status_label_, state_text);
}

lv_obj_t *WeatherScreen::screen() const { return screen_; }
