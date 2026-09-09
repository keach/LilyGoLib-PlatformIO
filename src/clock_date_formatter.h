#pragma once

#include <stddef.h>
#include <time.h>

void formatJapaneseClockDate(char *buffer, size_t buffer_size,
                             const struct tm &date_time);
