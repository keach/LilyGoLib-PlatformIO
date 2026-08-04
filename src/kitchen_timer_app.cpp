#include "kitchen_timer_app.h"

#include <Arduino.h>

KitchenTimerApp::KitchenTimerApp(uint32_t background_color,
                                 uint32_t primary_color,
                                 uint32_t accent_color,
                                 uint32_t muted_color,
                                 uint32_t button_color)
    : runtime_(timer_),
      screen_(timer_,
              background_color,
              primary_color,
              accent_color,
              muted_color,
              button_color)
{
}

void KitchenTimerApp::create(BackCallback back_callback,
                             void *back_context,
                             WakeCallback wake_callback,
                             void *wake_context,
                             AlertOutputCallback alert_output_callback,
                             void *alert_output_context)
{
    wake_callback_ = wake_callback;
    wake_context_ = wake_context;

    screen_.create(back_callback, back_context);
    runtime_.setWakeCallback(runtimeWakeCallback, this);
    runtime_.setAlertOutputCallback(alert_output_callback,
                                    alert_output_context);
}

void KitchenTimerApp::show()
{
    screen_.refresh(millis());
    lv_screen_load_anim(screen_.screen(),
                        LV_SCR_LOAD_ANIM_MOVE_LEFT,
                        180,
                        0,
                        false);
}

void KitchenTimerApp::update(uint32_t now_ms)
{
    runtime_.update(now_ms);
    if (lv_screen_active() == screen_.screen()) {
        screen_.refresh(now_ms);
    }
}

bool KitchenTimerApp::nextWakeDelayMilliseconds(uint32_t now_ms,
                                                uint32_t &delay_ms) const
{
    return runtime_.nextWakeDelayMilliseconds(now_ms, delay_ms);
}

bool KitchenTimerApp::requiresAwake() const
{
    return runtime_.requiresAwake();
}

KitchenTimerState KitchenTimerApp::state() const
{
    return timer_.state();
}

lv_obj_t *KitchenTimerApp::screen() const
{
    return screen_.screen();
}

void KitchenTimerApp::runtimeWakeCallback(void *context)
{
    auto *self = static_cast<KitchenTimerApp *>(context);
    if (self == nullptr) {
        return;
    }

    if (self->wake_callback_ != nullptr) {
        self->wake_callback_(self->wake_context_);
    }

    self->show();
}
