#include "notification_settings_screen.h"

#include <stdint.h>

NotificationSettingsScreen::NotificationSettingsScreen(
    uint32_t background_color,
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

void NotificationSettingsScreen::create(const char *title,
                                        SaveCallback save_callback,
                                        void *save_context,
                                        SoundSettingsCallback
                                            sound_settings_callback,
                                        void *sound_settings_context,
                                        BackCallback back_callback,
                                        void *back_context)
{
    save_callback_ = save_callback;
    save_context_ = save_context;
    sound_settings_callback_ = sound_settings_callback;
    sound_settings_context_ = sound_settings_context;
    back_callback_ = back_callback;
    back_context_ = back_context;

    screen_ = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(background_color_), 0);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(screen_, lv_color_hex(primary_color_), 0);
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_label = lv_label_create(screen_);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(accent_color_), 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 7);

    lv_obj_t *description = lv_label_create(screen_);
    lv_label_set_text(description, "END NOTIFICATION MODE");
    lv_obj_set_style_text_font(description, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(description, lv_color_hex(muted_color_), 0);
    lv_obj_align(description, LV_ALIGN_TOP_MID, 0, 34);

    constexpr NotificationMode modes[] = {
        NotificationMode::SoundAndVibration,
        NotificationMode::SoundOnly,
        NotificationMode::VibrationOnly,
    };
    for (int index = 0; index < 3; ++index) {
        mode_buttons_[index] = createButton(
            notificationModeLabel(modes[index]), 20, 53 + index * 35,
            200, 29, modeCallback,
            reinterpret_cast<void *>(static_cast<uintptr_t>(modes[index])));
    }

    lv_obj_t *sound_button = createButton(
        "SOUND: CLASSIC", 20, 160, 200, 30, soundSettingsCallback);
    sound_button_label_ = lv_obj_get_child(sound_button, 0);

    createButton("SAVE", 10, 202, 108, 32, saveCallback);
    createButton("CANCEL", 122, 202, 108, 32, cancelCallback);
    updateSelection();
}

void NotificationSettingsScreen::show(
    NotificationMode mode, NotificationSoundPreset sound_preset)
{
    selected_mode_ = mode;
    selected_sound_preset_ = resolveNotificationSoundPreset(
        static_cast<uint8_t>(sound_preset));
    updateSelection();
    updateSoundLabel();
    lv_screen_load_anim(screen_, LV_SCR_LOAD_ANIM_MOVE_LEFT, 180, 0, false);
}

void NotificationSettingsScreen::showPending()
{
    updateSelection();
    updateSoundLabel();
    lv_screen_load_anim(screen_, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 180, 0, false);
}

void NotificationSettingsScreen::updateSoundPreset(
    NotificationSoundPreset sound_preset)
{
    selected_sound_preset_ = resolveNotificationSoundPreset(
        static_cast<uint8_t>(sound_preset));
    updateSoundLabel();
}

NotificationSoundPreset NotificationSettingsScreen::soundPreset() const
{
    return selected_sound_preset_;
}

lv_obj_t *NotificationSettingsScreen::screen() const
{
    return screen_;
}

void NotificationSettingsScreen::modeCallback(lv_event_t *event)
{
    auto *button = lv_event_get_current_target_obj(event);
    auto *self = static_cast<NotificationSettingsScreen *>(
        lv_obj_get_user_data(button));
    if (self == nullptr) {
        return;
    }
    self->selectMode(static_cast<NotificationMode>(
        reinterpret_cast<uintptr_t>(lv_event_get_user_data(event))));
}

void NotificationSettingsScreen::saveCallback(lv_event_t *event)
{
    auto *button = lv_event_get_current_target_obj(event);
    auto *self = static_cast<NotificationSettingsScreen *>(
        lv_obj_get_user_data(button));
    if (self != nullptr) {
        self->save();
    }
}

void NotificationSettingsScreen::cancelCallback(lv_event_t *event)
{
    auto *button = lv_event_get_current_target_obj(event);
    auto *self = static_cast<NotificationSettingsScreen *>(
        lv_obj_get_user_data(button));
    if (self != nullptr) {
        self->goBack();
    }
}

void NotificationSettingsScreen::soundSettingsCallback(lv_event_t *event)
{
    auto *button = lv_event_get_current_target_obj(event);
    auto *self = static_cast<NotificationSettingsScreen *>(
        lv_obj_get_user_data(button));
    if (self != nullptr) {
        self->showSoundSettings();
    }
}

lv_obj_t *NotificationSettingsScreen::createButton(
    const char *text, int x, int y, int width, int height,
    lv_event_cb_t callback, void *user_data)
{
    lv_obj_t *button = lv_button_create(screen_);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    lv_obj_set_user_data(button, this);
    lv_obj_set_style_bg_color(button, lv_color_hex(button_color_), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_radius(button, 8, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_pad_all(button, 0, 0);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, user_data);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    lv_obj_center(label);
    return button;
}

void NotificationSettingsScreen::selectMode(NotificationMode mode)
{
    selected_mode_ = mode;
    updateSelection();
}

void NotificationSettingsScreen::updateSelection()
{
    if (screen_ == nullptr) {
        return;
    }
    for (uint8_t index = 0; index < 3; ++index) {
        const bool selected = index == static_cast<uint8_t>(selected_mode_);
        lv_obj_set_style_bg_color(
            mode_buttons_[index],
            lv_color_hex(selected ? accent_color_ : button_color_), 0);
        lv_obj_t *label = lv_obj_get_child(mode_buttons_[index], 0);
        lv_obj_set_style_text_color(
            label, lv_color_hex(selected ? background_color_ : primary_color_),
            0);
    }
}

void NotificationSettingsScreen::updateSoundLabel()
{
    if (sound_button_label_ == nullptr) {
        return;
    }
    lv_label_set_text_fmt(
        sound_button_label_, "SOUND: %s",
        notificationSoundPresetLabel(selected_sound_preset_));
}

void NotificationSettingsScreen::showSoundSettings()
{
    if (sound_settings_callback_ != nullptr) {
        sound_settings_callback_(selected_sound_preset_,
                                 sound_settings_context_);
    }
}

void NotificationSettingsScreen::save()
{
    if (save_callback_ != nullptr) {
        save_callback_(selected_mode_, selected_sound_preset_, save_context_);
    }
    goBack();
}

void NotificationSettingsScreen::goBack()
{
    if (back_callback_ != nullptr) {
        back_callback_(back_context_);
    }
}
