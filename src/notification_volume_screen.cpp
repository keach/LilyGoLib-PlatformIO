#include "notification_volume_screen.h"

NotificationVolumeScreen::NotificationVolumeScreen(
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

void NotificationVolumeScreen::create(
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
    lv_label_set_text(title, "ALARM VOLUME");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(accent_color_), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 9);

    lv_obj_t *description = lv_label_create(screen_);
    lv_label_set_text(description, "MASTER NOTIFICATION VOLUME");
    lv_obj_set_style_text_font(description, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(description, lv_color_hex(muted_color_), 0);
    lv_obj_align(description, LV_ALIGN_TOP_MID, 0, 37);

    level_label_ = lv_label_create(screen_);
    lv_label_set_text(level_label_, "LEVEL 3 / 5  60%");
    lv_obj_set_width(level_label_, 120);
    lv_obj_set_style_text_align(level_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(level_label_, &lv_font_montserrat_16, 0);
    lv_obj_align(level_label_, LV_ALIGN_TOP_MID, 0, 70);

    createButton("-", 20, 64, 42, 36, decreaseCallback);
    createButton("+", 178, 64, 42, 36, increaseCallback);

    preset_label_ = lv_label_create(screen_);
    lv_label_set_text(preset_label_, "PREVIEW: SUCCESS");
    lv_obj_set_width(preset_label_, 210);
    lv_obj_set_style_text_align(preset_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(preset_label_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(preset_label_, lv_color_hex(muted_color_), 0);
    lv_obj_align(preset_label_, LV_ALIGN_TOP_MID, 0, 112);

    createButton("PREVIEW", 20, 135, 200, 38, previewCallback);
    createButton("SAVE", 10, 202, 108, 32, saveCallback);
    createButton("CANCEL", 122, 202, 108, 32, cancelCallback);
}

void NotificationVolumeScreen::show(
    NotificationVolumeLevel level,
    NotificationSoundPreset preview_preset)
{
    selected_level_ = resolveNotificationVolumeLevel(
        static_cast<uint8_t>(level));
    preview_preset_ = resolveNotificationSoundPreset(
        static_cast<uint8_t>(preview_preset));
    updateLabels();
    lv_screen_load_anim(screen_, LV_SCR_LOAD_ANIM_MOVE_LEFT, 180, 0, false);
}

lv_obj_t *NotificationVolumeScreen::screen() const
{
    return screen_;
}

void NotificationVolumeScreen::decreaseCallback(lv_event_t *event)
{
    auto *self = static_cast<NotificationVolumeScreen *>(
        lv_obj_get_user_data(lv_event_get_current_target_obj(event)));
    if (self != nullptr) {
        self->decrease();
    }
}

void NotificationVolumeScreen::increaseCallback(lv_event_t *event)
{
    auto *self = static_cast<NotificationVolumeScreen *>(
        lv_obj_get_user_data(lv_event_get_current_target_obj(event)));
    if (self != nullptr) {
        self->increase();
    }
}

void NotificationVolumeScreen::previewCallback(lv_event_t *event)
{
    auto *self = static_cast<NotificationVolumeScreen *>(
        lv_obj_get_user_data(lv_event_get_current_target_obj(event)));
    if (self != nullptr) {
        self->preview();
    }
}

void NotificationVolumeScreen::saveCallback(lv_event_t *event)
{
    auto *self = static_cast<NotificationVolumeScreen *>(
        lv_obj_get_user_data(lv_event_get_current_target_obj(event)));
    if (self != nullptr) {
        self->save();
    }
}

void NotificationVolumeScreen::cancelCallback(lv_event_t *event)
{
    auto *self = static_cast<NotificationVolumeScreen *>(
        lv_obj_get_user_data(lv_event_get_current_target_obj(event)));
    if (self != nullptr) {
        self->goBack();
    }
}

lv_obj_t *NotificationVolumeScreen::createButton(
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

void NotificationVolumeScreen::decrease()
{
    selected_level_ = decreaseNotificationVolumeLevel(selected_level_);
    updateLabels();
}

void NotificationVolumeScreen::increase()
{
    selected_level_ = increaseNotificationVolumeLevel(selected_level_);
    updateLabels();
}

void NotificationVolumeScreen::preview()
{
    if (preview_callback_ != nullptr) {
        preview_callback_(preview_preset_, selected_level_,
                          preview_context_);
    }
}

void NotificationVolumeScreen::save()
{
    if (save_callback_ != nullptr) {
        save_callback_(selected_level_, save_context_);
    }
    goBack();
}

void NotificationVolumeScreen::goBack()
{
    if (back_callback_ != nullptr) {
        back_callback_(back_context_);
    }
}

void NotificationVolumeScreen::updateLabels()
{
    if (level_label_ != nullptr) {
        lv_label_set_text_fmt(
            level_label_, "LEVEL %u / 5  %u%%",
            static_cast<unsigned>(static_cast<uint8_t>(selected_level_) + 1),
            static_cast<unsigned>(notificationVolumePercent(selected_level_)));
    }
    if (preset_label_ != nullptr) {
        lv_label_set_text_fmt(
            preset_label_, "PREVIEW: %s",
            notificationSoundPresetLabel(preview_preset_));
    }
}
