#include "clock_date_formatter.h"

#include <stdio.h>

namespace {

const char *const kJapaneseWeekdays[] = {
    "日", "月", "火", "水", "木", "金", "土",
};

}  // namespace

void formatJapaneseClockDate(char *buffer, size_t buffer_size,
                             const struct tm &date_time)
{
    const char *weekday = "?";
    if (date_time.tm_wday >= 0 && date_time.tm_wday < 7) {
        weekday = kJapaneseWeekdays[date_time.tm_wday];
    }

    snprintf(buffer, buffer_size, "%d年%d月%d日 (%s)",
             date_time.tm_year + 1900, date_time.tm_mon + 1,
             date_time.tm_mday, weekday);
}
