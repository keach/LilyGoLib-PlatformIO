#pragma once

#include <lvgl.h>

#ifndef T_WATCH_JAPANESE_FONT_BPP
#define T_WATCH_JAPANESE_FONT_BPP 2
#endif

#if T_WATCH_JAPANESE_FONT_BPP == 2
LV_FONT_DECLARE(lv_font_japanese_16_2bpp);
inline const lv_font_t *japaneseFont16()
{
    return &lv_font_japanese_16_2bpp;
}
#elif T_WATCH_JAPANESE_FONT_BPP == 4
LV_FONT_DECLARE(lv_font_japanese_16_4bpp);
inline const lv_font_t *japaneseFont16()
{
    return &lv_font_japanese_16_4bpp;
}
#else
#error "T_WATCH_JAPANESE_FONT_BPP must be 2 or 4"
#endif
