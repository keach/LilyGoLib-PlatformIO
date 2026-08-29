#include "app_hub_screen.h"

AppHubScreen::AppHubScreen(uint32_t background_color,
                           uint32_t primary_color,
                           uint32_t accent_color,
                           uint32_t muted_color,
                           uint32_t button_color)
    : background_color_(background_color),
      primary_color_(primary_color),
      accent_color_(accent_color),
      muted_color_(muted_color),
      button_color_(button_color)
{
}

void AppHubScreen::create(ActionCallback kitchen_timer_callback,
                          void *kitchen_timer_context,
                          ActionCallback alarm_volume_callback,
                          void *alarm_volume_context,
                          ActionCallback back_callback,
                          void *back_context)
{
    kitchen_timer_binding_ = {
        this, kitchen_timer_callback, kitchen_timer_context,
    };
    alarm_volume_binding_ = {
        this, alarm_volume_callback, alarm_volume_context,
    };
    back_binding_ = {
        this, back_callback, back_context,
    };

    screen_ = lv_obj_create(nullptr);
    styleScreen();

    lv_obj_t *title = lv_label_create(screen_);
    lv_label_set_text(title, "APPS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(accent_color_), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t *subtitle = lv_label_create(screen_);
    lv_label_set_text(subtitle, "SELECT AN APP");
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(muted_color_), 0);
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 40);

    createButton("KITCHEN TIMER",
                 20,
                 76,
                 200,
                 48,
                 &kitchen_timer_binding_);

    createButton("ALARM VOLUME",
                 20,
                 136,
                 200,
                 48,
                 &alarm_volume_binding_);

    createButton("BACK",
                 20,
                 202,
                 200,
                 30,
                 &back_binding_);
}

lv_obj_t *AppHubScreen::screen() const
{
    return screen_;
}

void AppHubScreen::show(bool returning_from_child)
{
    if (screen_ == nullptr) {
        return;
    }
    lv_screen_load_anim(
        screen_,
        returning_from_child ? LV_SCR_LOAD_ANIM_MOVE_RIGHT
                             : LV_SCR_LOAD_ANIM_MOVE_LEFT,
        180,
        0,
        false);
}

void AppHubScreen::actionCallback(lv_event_t *event)
{
    auto *binding = static_cast<ActionBinding *>(
        lv_event_get_user_data(event));
    if (binding == nullptr || binding->screen == nullptr ||
        binding->callback == nullptr) {
        return;
    }
    binding->callback(binding->context);
}

lv_obj_t *AppHubScreen::createButton(const char *text,
                                     int x,
                                     int y,
                                     int width,
                                     int height,
                                     ActionBinding *binding)
{
    lv_obj_t *button = lv_button_create(screen_);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    styleButton(button);
    lv_obj_add_event_cb(button,
                        actionCallback,
                        LV_EVENT_CLICKED,
                        binding);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_center(label);
    return button;
}

void AppHubScreen::styleScreen()
{
    lv_obj_set_style_bg_color(screen_, lv_color_hex(background_color_), 0);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(screen_, lv_color_hex(primary_color_), 0);
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);
}

void AppHubScreen::styleButton(lv_obj_t *button)
{
    lv_obj_set_style_bg_color(button, lv_color_hex(button_color_), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_radius(button, 8, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_pad_all(button, 0, 0);
}
