#ifndef WEEKDAY_DISPLAY_H
#define WEEKDAY_DISPLAY_H

#include <stdint.h>

static inline const char *WeekdayAbbreviation(uint8_t weekday) {
    static const char *const abbreviations[7] = {
        "MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"
    };
    if (weekday < 1u || weekday > 7u) return 0;
    return abbreviations[weekday - 1u];
}

#endif
