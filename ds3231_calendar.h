#ifndef DS3231_CALENDAR_H
#define DS3231_CALENDAR_H

#include <stdint.h>

typedef struct {
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t weekday;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} DS3231Calendar;

static inline uint8_t DS3231_ToBCD(uint8_t value) {
    return (uint8_t)(((value / 10u) << 4) | (value % 10u));
}

static inline uint8_t DS3231_FromBCD(uint8_t value, uint8_t *result) {
    uint8_t tens = (uint8_t)((value >> 4) & 0x0Fu);
    uint8_t ones = (uint8_t)(value & 0x0Fu);
    if (tens > 9u || ones > 9u) return 0;
    *result = (uint8_t)(tens * 10u + ones);
    return 1;
}

static inline uint8_t DS3231_DaysInMonth(uint8_t year, uint8_t month) {
    static const uint8_t days[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month < 1u || month > 12u) return 0;
    if (month == 2u && (year % 4u) == 0u) return 29;
    return days[month - 1u];
}

static inline uint8_t DS3231_EncodeCalendar(const DS3231Calendar *calendar, uint8_t regs[7]) {
    if (!calendar || !regs || calendar->year > 99u || calendar->month < 1u || calendar->month > 12u ||
        calendar->day < 1u || calendar->day > DS3231_DaysInMonth(calendar->year, calendar->month) ||
        calendar->weekday < 1u || calendar->weekday > 7u || calendar->hour > 23u ||
        calendar->minute > 59u || calendar->second > 59u) return 0;
    regs[0] = DS3231_ToBCD(calendar->second);
    regs[1] = DS3231_ToBCD(calendar->minute);
    regs[2] = DS3231_ToBCD(calendar->hour);
    regs[3] = calendar->weekday;
    regs[4] = DS3231_ToBCD(calendar->day);
    regs[5] = DS3231_ToBCD(calendar->month);
    regs[6] = DS3231_ToBCD(calendar->year);
    return 1;
}

static inline uint8_t DS3231_DecodeCalendar(const uint8_t regs[7], DS3231Calendar *calendar) {
    if (!regs || !calendar || (regs[2] & 0x40u) || (regs[5] & 0x80u) ||
        (regs[3] & 0xF8u) || (regs[4] & 0xC0u) || (regs[5] & 0x60u)) return 0;
    if (!DS3231_FromBCD((uint8_t)(regs[0] & 0x7Fu), &calendar->second) ||
        !DS3231_FromBCD((uint8_t)(regs[1] & 0x7Fu), &calendar->minute) ||
        !DS3231_FromBCD((uint8_t)(regs[2] & 0x3Fu), &calendar->hour) ||
        !DS3231_FromBCD((uint8_t)(regs[4] & 0x3Fu), &calendar->day) ||
        !DS3231_FromBCD((uint8_t)(regs[5] & 0x1Fu), &calendar->month) ||
        !DS3231_FromBCD(regs[6], &calendar->year)) return 0;
    calendar->weekday = (uint8_t)(regs[3] & 0x07u);
    if (calendar->second > 59u || calendar->minute > 59u || calendar->hour > 23u ||
        calendar->weekday < 1u || calendar->weekday > 7u ||
        calendar->day < 1u || calendar->day > DS3231_DaysInMonth(calendar->year, calendar->month)) return 0;
    return 1;
}

#endif
