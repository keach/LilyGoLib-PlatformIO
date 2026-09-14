#include "gotify_screen.h"

#include <string.h>

#include "japanese_font.h"

GotifyScreen::GotifyScreen(uint32_t background_color, uint32_t primary_color,
                           uint32_t accent_color, uint32_t muted_color,
                           uint32_t button_color)
    : background_color_(background_color), primary_color_(primary_color),
      accent_color_(accent_color), muted_color_(muted_color),
      button_color_(button_color) {}

lv_obj_t *GotifyScreen::createButton(lv_obj_t *parent, const char *text,
                                     int x, int y, int width, int height,
                                     Binding *binding)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    lv_obj_set_style_bg_color(button, lv_color_hex(button_color_), 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_radius(button, 8, 0);
    lv_obj_set_user_data(button, binding);
    lv_obj_add_event_cb(button, actionCallback, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
    lv_obj_center(label);
    return button;
}

void GotifyScreen::create(ActionCallback check_callback, void *check_context,
                          ActionCallback settings_callback,
                          void *settings_context,
                          ActionCallback back_callback, void *back_context,
                          ActionCallback dismiss_callback,
                          void *dismiss_context)
{
    check_binding_ = {check_callback, check_context};
    settings_binding_ = {settings_callback, settings_context};
    back_binding_ = {back_callback, back_context};
    dismiss_binding_ = {dismiss_callback, dismiss_context};
    screen_ = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(background_color_), 0);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *title = lv_label_create(screen_);
    lv_label_set_text(title, "GOTIFY");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(accent_color_), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    configuration_label_ = lv_label_create(screen_);
    last_check_label_ = lv_label_create(screen_);
    result_label_ = lv_label_create(screen_);
    message_id_label_ = lv_label_create(screen_);
    lv_obj_t *labels[] = {configuration_label_, last_check_label_,
                          result_label_, message_id_label_};
    for (size_t i = 0; i < 4; ++i) {
        lv_obj_set_width(labels[i], 220);
        lv_obj_set_style_text_align(labels[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(labels[i], &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(labels[i],
                                    lv_color_hex(i == 2 ? accent_color_
                                                       : primary_color_), 0);
        lv_obj_align(labels[i], LV_ALIGN_TOP_MID, 0, 49 + i * 25);
    }
    createButton(screen_, "CHECK NOW", 10, 151, 108, 34, &check_binding_);
    createButton(screen_, "NOTIFY", 122, 151, 108, 34, &settings_binding_);
    createButton(screen_, "BACK", 20, 202, 200, 32, &back_binding_);

    popup_ = lv_obj_create(lv_layer_top());
    lv_obj_set_pos(popup_, 8, 8);
    lv_obj_set_size(popup_, 224, 224);
    lv_obj_set_style_bg_color(popup_, lv_color_hex(background_color_), 0);
    lv_obj_set_style_bg_opa(popup_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(popup_, lv_color_hex(accent_color_), 0);
    lv_obj_set_style_border_width(popup_, 2, 0);
    lv_obj_set_style_radius(popup_, 10, 0);
    lv_obj_clear_flag(popup_, LV_OBJ_FLAG_SCROLLABLE);
    popup_title_ = lv_label_create(popup_);
    lv_obj_set_size(popup_title_, 196, 31);
    lv_label_set_long_mode(popup_title_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(popup_title_, japaneseFont16(), 0);
    lv_obj_set_style_text_color(popup_title_, lv_color_hex(accent_color_), 0);
    lv_obj_align(popup_title_, LV_ALIGN_TOP_MID, 0, 4);
    popup_date_ = lv_label_create(popup_);
    lv_obj_set_width(popup_date_, 196);
    lv_label_set_long_mode(popup_date_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(popup_date_, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(popup_date_, lv_color_hex(muted_color_), 0);
    lv_obj_align(popup_date_, LV_ALIGN_TOP_LEFT, 4, 38);
    popup_body_ = lv_label_create(popup_);
    lv_obj_set_size(popup_body_, 196, 112);
    lv_label_set_long_mode(popup_body_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(popup_body_, japaneseFont16(), 0);
    lv_obj_set_style_text_color(popup_body_, lv_color_hex(primary_color_), 0);
    lv_obj_align(popup_body_, LV_ALIGN_TOP_MID, 0, 58);
    createButton(popup_, "DISMISS", 12, 174, 196, 32, &dismiss_binding_);
    lv_obj_add_flag(popup_, LV_OBJ_FLAG_HIDDEN);
}

void GotifyScreen::show() {
    lv_screen_load_anim(screen_, LV_SCR_LOAD_ANIM_MOVE_LEFT, 180, 0, false);
}

void GotifyScreen::update(bool configured, bool checking,
                          time_t last_check_epoch, const char *result,
                          uint64_t last_message_id)
{
    lv_label_set_text(configuration_label_, configured ? "CONFIGURED"
                                                      : "NOT CONFIGURED");
    if (last_check_epoch > 0) {
        struct tm value = {};
        localtime_r(&last_check_epoch, &value);
        lv_label_set_text_fmt(last_check_label_, "LAST CHECK %02d:%02d",
                              value.tm_hour, value.tm_min);
    } else {
        lv_label_set_text(last_check_label_, "LAST CHECK --:--");
    }
    lv_label_set_text(result_label_, checking ? "CHECKING..."
                                               : (result ? result : "READY"));
    lv_label_set_text_fmt(message_id_label_, "LAST MESSAGE %llu",
                          static_cast<unsigned long long>(last_message_id));
}

void GotifyScreen::showMessage(const GotifyMessage &message)
{
    lv_label_set_text(popup_title_, message.title[0] ? message.title : "Gotify");
    lv_label_set_text(popup_body_, message.body);
    char date[kGotifyDateLength + 1] = {};
    strncpy(date, message.date, sizeof(date) - 1);
    if (strlen(date) > 19) date[19] = '\0';
    lv_label_set_text(popup_date_, date);
    lv_obj_remove_flag(popup_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(popup_);
}

void GotifyScreen::hideMessage() { lv_obj_add_flag(popup_, LV_OBJ_FLAG_HIDDEN); }
bool GotifyScreen::messageVisible() const {
    return popup_ != nullptr && !lv_obj_has_flag(popup_, LV_OBJ_FLAG_HIDDEN);
}
lv_obj_t *GotifyScreen::screen() const { return screen_; }

void GotifyScreen::actionCallback(lv_event_t *event)
{
    auto *binding = static_cast<Binding *>(
        lv_obj_get_user_data(lv_event_get_current_target_obj(event)));
    if (binding != nullptr && binding->callback != nullptr)
        binding->callback(binding->context);
}
