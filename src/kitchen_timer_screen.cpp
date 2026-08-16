#include "kitchen_timer_screen.h"

#include <Arduino.h>
#include <stdint.h>

namespace {

const char *stateText(KitchenTimerState state)
{
    switch (state) {
    case KitchenTimerState::Idle:
        return "READY";
    case KitchenTimerState::Running:
        return "RUNNING";
    case KitchenTimerState::Paused:
        return "PAUSED";
    case KitchenTimerState::Alerting:
        return "TIME UP";
    }
    return "UNKNOWN";
}

}  // namespace

KitchenTimerScreen::KitchenTimerScreen(KitchenTimer &timer,
                                       uint32_t background_color,
                                       uint32_t primary_color,
                                       uint32_t accent_color,
                                       uint32_t muted_color,
                                       uint32_t button_color)
    : timer_(timer),
      background_color_(background_color),
      primary_color_(primary_color),
      accent_color_(accent_color),
      muted_color_(muted_color),
      button_color_(button_color)
{
}

void KitchenTimerScreen::create(BackCallback back_callback, void *back_context,
                                SettingsCallback settings_callback,
                                void *settings_context)
{
    back_callback_ = back_callback;
    back_context_ = back_context;
    settings_callback_ = settings_callback;
    settings_context_ = settings_context;

    screen_ = lv_obj_create(nullptr);
    styleScreen();
    lv_obj_add_event_cb(screen_, screenLoadCallback,
                        LV_EVENT_SCREEN_LOAD_START, this);

    lv_obj_t *title = lv_label_create(screen_);
    lv_label_set_text(title, "KITCHEN TIMER");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(accent_color_), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    time_label_ = lv_label_create(screen_);
    lv_label_set_text(time_label_, "00:00");
    lv_obj_set_style_text_font(time_label_, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(time_label_, lv_color_hex(primary_color_), 0);
    lv_obj_align(time_label_, LV_ALIGN_TOP_MID, 0, 30);

    state_label_ = lv_label_create(screen_);
    lv_label_set_text(state_label_, "READY");
    lv_obj_set_style_text_font(state_label_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(state_label_, lv_color_hex(muted_color_), 0);
    lv_obj_align(state_label_, LV_ALIGN_TOP_MID, 0, 66);

    constexpr uint32_t presets[] = {300, 600, 1800, 3600};
    const char *preset_labels[] = {"5M", "10M", "30M", "60M"};
    for (int index = 0; index < 4; ++index) {
        createButton(preset_labels[index], 10 + index * 56, 88, 52, 30,
                     presetCallback,
                     reinterpret_cast<void *>(
                         static_cast<uintptr_t>(presets[index])));
    }

    constexpr int32_t adjustments[] = {-600, -60, -10, 10, 60, 600};
    const char *adjustment_labels[] = {
        "-10M", "-1M", "-10S", "+10S", "+1M", "+10M"};
    for (int index = 0; index < 6; ++index) {
        createButton(adjustment_labels[index], 10 + index * 37, 124, 34, 30,
                     adjustCallback,
                     reinterpret_cast<void *>(
                         static_cast<intptr_t>(adjustments[index])),
                     &lv_font_montserrat_10);
    }

    primary_button_ = createButton("START", 10, 164, 108, 38,
                                   primaryActionCallback);
    primary_button_label_ = lv_obj_get_child(primary_button_, 0);
    lv_obj_set_style_bg_color(primary_button_, lv_color_hex(accent_color_), 0);
    lv_obj_set_style_text_color(primary_button_label_,
                                lv_color_hex(background_color_), 0);

    cancel_button_ = createButton("CANCEL", 122, 164, 108, 38,
                                  cancelCallback);
    cancel_button_label_ = lv_obj_get_child(cancel_button_, 0);

    createButton("SETTINGS", 10, 208, 108, 26, settingsCallback);
    createButton("BACK", 122, 208, 108, 26, backCallback);
    refresh(millis());
}

lv_obj_t *KitchenTimerScreen::screen() const
{
    return screen_;
}

void KitchenTimerScreen::refresh(uint32_t now_ms)
{
    timer_.update(now_ms);
    updateLabels(now_ms);
}

void KitchenTimerScreen::presetCallback(lv_event_t *event)
{
    auto *button = lv_event_get_current_target_obj(event);
    auto *self = static_cast<KitchenTimerScreen *>(lv_obj_get_user_data(button));
    if (self == nullptr) {
        return;
    }
    const uint32_t seconds = static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
    self->setDuration(seconds);
}

void KitchenTimerScreen::adjustCallback(lv_event_t *event)
{
    auto *button = lv_event_get_current_target_obj(event);
    auto *self = static_cast<KitchenTimerScreen *>(lv_obj_get_user_data(button));
    if (self == nullptr) {
        return;
    }
    const int32_t seconds = static_cast<int32_t>(
        reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
    self->adjustDuration(seconds);
}

void KitchenTimerScreen::primaryActionCallback(lv_event_t *event)
{
    auto *button = lv_event_get_current_target_obj(event);
    auto *self = static_cast<KitchenTimerScreen *>(lv_obj_get_user_data(button));
    if (self != nullptr) {
        self->primaryAction();
    }
}

void KitchenTimerScreen::cancelCallback(lv_event_t *event)
{
    auto *button = lv_event_get_current_target_obj(event);
    auto *self = static_cast<KitchenTimerScreen *>(lv_obj_get_user_data(button));
    if (self != nullptr) {
        self->cancelOrStop();
    }
}

void KitchenTimerScreen::backCallback(lv_event_t *event)
{
    auto *button = lv_event_get_current_target_obj(event);
    auto *self = static_cast<KitchenTimerScreen *>(lv_obj_get_user_data(button));
    if (self != nullptr) {
        self->goBack();
    }
}

void KitchenTimerScreen::settingsCallback(lv_event_t *event)
{
    auto *button = lv_event_get_current_target_obj(event);
    auto *self = static_cast<KitchenTimerScreen *>(lv_obj_get_user_data(button));
    if (self != nullptr) {
        self->showSettings();
    }
}

void KitchenTimerScreen::screenLoadCallback(lv_event_t *event)
{
    auto *self = static_cast<KitchenTimerScreen *>(lv_event_get_user_data(event));
    if (self != nullptr) {
        self->refresh(millis());
    }
}

lv_obj_t *KitchenTimerScreen::createButton(const char *text, int x, int y,
                                           int width, int height,
                                           lv_event_cb_t callback,
                                           void *user_data,
                                           const lv_font_t *font)
{
    lv_obj_t *button = lv_button_create(screen_);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    lv_obj_set_user_data(button, this);
    styleButton(button);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, user_data);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_center(label);
    return button;
}

void KitchenTimerScreen::styleScreen()
{
    lv_obj_set_style_bg_color(screen_, lv_color_hex(background_color_), 0);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(screen_, lv_color_hex(primary_color_), 0);
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);
}

void KitchenTimerScreen::styleButton(lv_obj_t *button)
{
    lv_obj_set_style_bg_color(button, lv_color_hex(button_color_), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_radius(button, 8, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_pad_all(button, 0, 0);
}

void KitchenTimerScreen::setDuration(uint32_t seconds)
{
    if (timer_.setDurationSeconds(seconds)) {
        updateLabels(millis());
    }
}

void KitchenTimerScreen::adjustDuration(int32_t seconds)
{
    timer_.adjustSeconds(seconds);
    updateLabels(millis());
}

void KitchenTimerScreen::primaryAction()
{
    const uint32_t now_ms = millis();
    switch (timer_.state()) {
    case KitchenTimerState::Idle:
        timer_.start(now_ms);
        break;
    case KitchenTimerState::Running:
        timer_.pause(now_ms);
        break;
    case KitchenTimerState::Paused:
        timer_.resume(now_ms);
        break;
    case KitchenTimerState::Alerting:
        timer_.stopAlert();
        break;
    }
    updateLabels(now_ms);
}

void KitchenTimerScreen::cancelOrStop()
{
    if (timer_.state() == KitchenTimerState::Idle ||
        timer_.state() == KitchenTimerState::Paused) {
        timer_.reset();
    } else if (timer_.state() == KitchenTimerState::Alerting) {
        timer_.stopAlert();
    } else {
        timer_.cancel();
    }
    updateLabels(millis());
}

void KitchenTimerScreen::showSettings()
{
    if (settings_callback_ != nullptr) {
        settings_callback_(settings_context_);
    }
}

void KitchenTimerScreen::goBack()
{
    if (back_callback_ != nullptr) {
        back_callback_(back_context_);
    }
}

void KitchenTimerScreen::updateLabels(uint32_t now_ms)
{
    if (screen_ == nullptr) {
        return;
    }

    const KitchenTimerState state = timer_.state();
    const uint32_t seconds = state == KitchenTimerState::Idle
                                 ? timer_.configuredSeconds()
                                 : timer_.remainingSeconds(now_ms);
    lv_label_set_text_fmt(time_label_, "%02lu:%02lu",
                          static_cast<unsigned long>(seconds / 60),
                          static_cast<unsigned long>(seconds % 60));
    lv_label_set_text(state_label_, stateText(state));
    lv_obj_set_style_text_color(
        state_label_,
        lv_color_hex(state == KitchenTimerState::Alerting
                         ? accent_color_
                         : muted_color_),
        0);

    const char *primary_text = "START";
    if (state == KitchenTimerState::Running) {
        primary_text = "PAUSE";
    } else if (state == KitchenTimerState::Paused) {
        primary_text = "RESUME";
    } else if (state == KitchenTimerState::Alerting) {
        primary_text = "STOP";
    }
    lv_label_set_text(primary_button_label_, primary_text);
    const char *cancel_text = "CANCEL";
    if (state == KitchenTimerState::Idle ||
        state == KitchenTimerState::Paused) {
        cancel_text = "RESET";
    } else if (state == KitchenTimerState::Alerting) {
        cancel_text = "STOP";
    }
    lv_label_set_text(cancel_button_label_, cancel_text);

    if (state == KitchenTimerState::Idle &&
        timer_.configuredSeconds() == 0) {
        lv_obj_add_state(primary_button_, LV_STATE_DISABLED);
    } else {
        lv_obj_remove_state(primary_button_, LV_STATE_DISABLED);
    }
}
