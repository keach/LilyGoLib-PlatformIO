#include "pomodoro_timer_screen.h"

#include <Arduino.h>

namespace {

const char *phaseText(PomodoroPhase phase)
{
    return phase == PomodoroPhase::Focus ? "FOCUS" : "BREAK";
}

const char *stateText(PomodoroState state, PomodoroPhase phase)
{
    switch (state) {
    case PomodoroState::Idle:
        return "READY";
    case PomodoroState::Running:
        return "RUNNING";
    case PomodoroState::Paused:
        return "PAUSED";
    case PomodoroState::Alerting:
        return phase == PomodoroPhase::Focus
                   ? "FOCUS COMPLETE"
                   : "BREAK COMPLETE";
    }
    return "UNKNOWN";
}

}  // namespace

PomodoroTimerScreen::PomodoroTimerScreen(
    PomodoroTimer &timer,
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

void PomodoroTimerScreen::create(BackCallback back_callback,
                                 void *back_context,
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
    lv_label_set_text(title, "POMODORO");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(accent_color_), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    phase_label_ = lv_label_create(screen_);
    lv_label_set_text(phase_label_, "FOCUS");
    lv_obj_set_style_text_font(phase_label_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(phase_label_, lv_color_hex(muted_color_), 0);
    lv_obj_align(phase_label_, LV_ALIGN_TOP_MID, 0, 32);

    time_label_ = lv_label_create(screen_);
    lv_label_set_text(time_label_, "25:00");
    lv_obj_set_style_text_font(time_label_, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(time_label_, lv_color_hex(primary_color_), 0);
    lv_obj_align(time_label_, LV_ALIGN_TOP_MID, 0, 48);

    state_label_ = lv_label_create(screen_);
    lv_label_set_text(state_label_, "READY");
    lv_obj_set_style_text_font(state_label_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(state_label_, lv_color_hex(muted_color_), 0);
    lv_obj_align(state_label_, LV_ALIGN_TOP_MID, 0, 91);

    sessions_label_ = lv_label_create(screen_);
    lv_label_set_text(sessions_label_, "FOCUS SESSIONS: 0");
    lv_obj_set_style_text_font(sessions_label_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(sessions_label_, lv_color_hex(muted_color_), 0);
    lv_obj_align(sessions_label_, LV_ALIGN_TOP_MID, 0, 111);

    lv_obj_t *primary_button = createButton(
        "START", 20, 135, 200, 38, primaryActionCallback);
    primary_button_label_ = lv_obj_get_child(primary_button, 0);
    lv_obj_set_style_bg_color(primary_button, lv_color_hex(accent_color_), 0);
    lv_obj_set_style_text_color(primary_button_label_,
                                lv_color_hex(background_color_), 0);

    createButton("END", 10, 181, 70, 25, endCallback);
    createButton("SETTINGS", 85, 181, 95, 25, settingsCallback);
    createButton("BACK", 185, 181, 45, 25, backCallback);
    refresh(millis());
}

lv_obj_t *PomodoroTimerScreen::screen() const
{
    return screen_;
}

void PomodoroTimerScreen::refresh(uint32_t now_ms)
{
    timer_.update(now_ms);
    updateLabels(now_ms);
}

void PomodoroTimerScreen::primaryActionCallback(lv_event_t *event)
{
    auto *self = static_cast<PomodoroTimerScreen *>(
        lv_obj_get_user_data(lv_event_get_current_target_obj(event)));
    if (self != nullptr) {
        self->primaryAction();
    }
}

void PomodoroTimerScreen::endCallback(lv_event_t *event)
{
    auto *self = static_cast<PomodoroTimerScreen *>(
        lv_obj_get_user_data(lv_event_get_current_target_obj(event)));
    if (self != nullptr) {
        self->endSession();
    }
}

void PomodoroTimerScreen::settingsCallback(lv_event_t *event)
{
    auto *self = static_cast<PomodoroTimerScreen *>(
        lv_obj_get_user_data(lv_event_get_current_target_obj(event)));
    if (self != nullptr) {
        self->showSettings();
    }
}

void PomodoroTimerScreen::backCallback(lv_event_t *event)
{
    auto *self = static_cast<PomodoroTimerScreen *>(
        lv_obj_get_user_data(lv_event_get_current_target_obj(event)));
    if (self != nullptr) {
        self->goBack();
    }
}

void PomodoroTimerScreen::screenLoadCallback(lv_event_t *event)
{
    auto *self = static_cast<PomodoroTimerScreen *>(
        lv_event_get_user_data(event));
    if (self != nullptr) {
        self->refresh(millis());
    }
}

lv_obj_t *PomodoroTimerScreen::createButton(
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

void PomodoroTimerScreen::styleScreen()
{
    lv_obj_set_style_bg_color(screen_, lv_color_hex(background_color_), 0);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(screen_, lv_color_hex(primary_color_), 0);
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);
}

void PomodoroTimerScreen::styleButton(lv_obj_t *button)
{
    lv_obj_set_style_bg_color(button, lv_color_hex(button_color_), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_radius(button, 8, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_pad_all(button, 0, 0);
}

void PomodoroTimerScreen::primaryAction()
{
    const uint32_t now_ms = millis();
    switch (timer_.state()) {
    case PomodoroState::Idle:
        timer_.start(now_ms);
        break;
    case PomodoroState::Running:
        timer_.pause(now_ms);
        break;
    case PomodoroState::Paused:
        timer_.resume(now_ms);
        break;
    case PomodoroState::Alerting:
        timer_.startNextPhase(now_ms);
        break;
    }
    updateLabels(now_ms);
}

void PomodoroTimerScreen::endSession()
{
    timer_.reset();
    updateLabels(millis());
}

void PomodoroTimerScreen::showSettings()
{
    if (settings_callback_ != nullptr) {
        settings_callback_(settings_context_);
    }
}

void PomodoroTimerScreen::goBack()
{
    if (back_callback_ != nullptr) {
        back_callback_(back_context_);
    }
}

void PomodoroTimerScreen::updateLabels(uint32_t now_ms)
{
    if (screen_ == nullptr) {
        return;
    }
    const PomodoroPhase phase = timer_.phase();
    const PomodoroState state = timer_.state();
    const uint32_t seconds = timer_.remainingSeconds(now_ms);
    lv_label_set_text(phase_label_, phaseText(phase));
    lv_label_set_text_fmt(time_label_, "%02lu:%02lu",
                          static_cast<unsigned long>(seconds / 60),
                          static_cast<unsigned long>(seconds % 60));
    lv_label_set_text(state_label_, stateText(state, phase));
    lv_obj_set_style_text_color(
        state_label_,
        lv_color_hex(state == PomodoroState::Alerting
                         ? accent_color_
                         : muted_color_),
        0);
    lv_label_set_text_fmt(
        sessions_label_, "FOCUS SESSIONS: %lu",
        static_cast<unsigned long>(timer_.completedFocusSessions()));

    const char *primary_text = "START";
    if (state == PomodoroState::Running) {
        primary_text = "PAUSE";
    } else if (state == PomodoroState::Paused) {
        primary_text = "RESUME";
    } else if (state == PomodoroState::Alerting) {
        primary_text = phase == PomodoroPhase::Focus
                           ? "START BREAK"
                           : "START FOCUS";
    }
    lv_label_set_text(primary_button_label_, primary_text);
}
