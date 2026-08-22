#include "notification_sound_settings_screen.h"

NotificationSoundSettingsScreen::NotificationSoundSettingsScreen(
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

void NotificationSoundSettingsScreen::create(
    SaveCallback save_callback,
    void *save_context,
    PreviewCallback preview_callback,
    void *preview_context,
    BackCallback back_callback,
    void *back_context)
{
    save_callback_ = save_callback;
    save_context_ = save_context;
    preview_callback_ = preview_callback;
    preview_context_ = preview_context;
    back_callback_ = back_callback;
    back_context_ = back_context;

    screen_ = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(background_color_), 0);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(screen_, lv_color_hex(primary_color_), 0);
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(screen_);
    lv_label_set_text(title, "NOTIFICATION SOUND");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(accent_color_), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *description = lv_label_create(screen_);
    lv_label_set_text(description, "SELECT A PRESET");
    lv_obj_set_style_text_font(description, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(description, lv_color_hex(muted_color_), 0);
    lv_obj_align(description, LV_ALIGN_TOP_MID, 0, 44);

    preset_label_ = lv_label_create(screen_);
    lv_label_set_text(preset_label_, "SUCCESS");
    lv_obj_set_width(preset_label_, 120);
    lv_obj_set_style_text_align(preset_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(preset_label_, &lv_font_montserrat_16, 0);
    lv_obj_align(preset_label_, LV_ALIGN_TOP_MID, 0, 77);

    createButton("<", 20, 70, 42, 36, previousCallback);
    createButton(">", 178, 70, 42, 36, nextCallback);
    createButton("PREVIEW", 20, 128, 200, 42, previewCallback);
    createButton("SAVE", 10, 202, 108, 32, saveCallback);
    createButton("CANCEL", 122, 202, 108, 32, cancelCallback);
}

void NotificationSoundSettingsScreen::show(NotificationSoundPreset preset)
{
    selected_preset_ = resolveNotificationSoundPreset(
        static_cast<uint8_t>(preset));
    updateLabel();
    lv_screen_load_anim(screen_, LV_SCR_LOAD_ANIM_MOVE_LEFT, 180, 0, false);
}

lv_obj_t *NotificationSoundSettingsScreen::screen() const
{
    return screen_;
}

void NotificationSoundSettingsScreen::previousCallback(lv_event_t *event)
{
    auto *self = static_cast<NotificationSoundSettingsScreen *>(
        lv_obj_get_user_data(lv_event_get_current_target_obj(event)));
    if (self != nullptr) {
        self->selectPrevious();
    }
}

void NotificationSoundSettingsScreen::nextCallback(lv_event_t *event)
{
    auto *self = static_cast<NotificationSoundSettingsScreen *>(
        lv_obj_get_user_data(lv_event_get_current_target_obj(event)));
    if (self != nullptr) {
        self->selectNext();
    }
}

void NotificationSoundSettingsScreen::previewCallback(lv_event_t *event)
{
    auto *self = static_cast<NotificationSoundSettingsScreen *>(
        lv_obj_get_user_data(lv_event_get_current_target_obj(event)));
    if (self != nullptr) {
        self->preview();
    }
}

void NotificationSoundSettingsScreen::saveCallback(lv_event_t *event)
{
    auto *self = static_cast<NotificationSoundSettingsScreen *>(
        lv_obj_get_user_data(lv_event_get_current_target_obj(event)));
    if (self != nullptr) {
        self->save();
    }
}

void NotificationSoundSettingsScreen::cancelCallback(lv_event_t *event)
{
    auto *self = static_cast<NotificationSoundSettingsScreen *>(
        lv_obj_get_user_data(lv_event_get_current_target_obj(event)));
    if (self != nullptr) {
        self->goBack();
    }
}

lv_obj_t *NotificationSoundSettingsScreen::createButton(
    const char *text, int x, int y, int width, int height,
    lv_event_cb_t callback)
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
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    lv_obj_center(label);
    return button;
}

void NotificationSoundSettingsScreen::selectPrevious()
{
    selected_preset_ = previousNotificationSoundPreset(selected_preset_);
    updateLabel();
}

void NotificationSoundSettingsScreen::selectNext()
{
    selected_preset_ = nextNotificationSoundPreset(selected_preset_);
    updateLabel();
}

void NotificationSoundSettingsScreen::preview()
{
    if (preview_callback_ != nullptr) {
        preview_callback_(selected_preset_, preview_context_);
    }
}

void NotificationSoundSettingsScreen::save()
{
    if (save_callback_ != nullptr) {
        save_callback_(selected_preset_, save_context_);
    }
    goBack();
}

void NotificationSoundSettingsScreen::goBack()
{
    if (back_callback_ != nullptr) {
        back_callback_(back_context_);
    }
}

void NotificationSoundSettingsScreen::updateLabel()
{
    if (preset_label_ != nullptr) {
        lv_label_set_text(
            preset_label_, notificationSoundPresetLabel(selected_preset_));
    }
}
