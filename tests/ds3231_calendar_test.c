#include <assert.h>
#include <stdint.h>
#include "../ds3231_calendar.h"

static void test_preset_calendar_encoding(void) {
    const DS3231Calendar preset = {26, 10, 18, 1, 16, 20, 0};
    uint8_t regs[7];
    assert(DS3231_EncodeCalendar(&preset, regs));
    assert(regs[0] == 0x00);
    assert(regs[1] == 0x20);
    assert(regs[2] == 0x16);
    assert(regs[3] == 0x01);
    assert(regs[4] == 0x18);
    assert(regs[5] == 0x10);
    assert(regs[6] == 0x26);
}

static void test_calendar_round_trip(void) {
    const uint8_t regs[7] = {0x59,0x58,0x23,0x07,0x31,0x12,0x99};
    DS3231Calendar actual;
    assert(DS3231_DecodeCalendar(regs, &actual));
    assert(actual.year == 99 && actual.month == 12 && actual.day == 31);
    assert(actual.weekday == 7 && actual.hour == 23 && actual.minute == 58 && actual.second == 59);
}

static void test_invalid_calendar_values_are_rejected(void) {
    DS3231Calendar calendar = {26, 2, 29, 1, 16, 20, 0};
    uint8_t regs[7];
    assert(!DS3231_EncodeCalendar(&calendar, regs));
    const uint8_t bad_bcd[7] = {0x6A,0x00,0x00,0x01,0x01,0x01,0x26};
    assert(!DS3231_DecodeCalendar(bad_bcd, &calendar));
    const uint8_t bad_hour[7] = {0x00,0x00,0x24,0x01,0x01,0x01,0x26};
    assert(!DS3231_DecodeCalendar(bad_hour, &calendar));
}

int main(void) {
    test_preset_calendar_encoding();
    test_calendar_round_trip();
    test_invalid_calendar_values_are_rejected();
    return 0;
}
