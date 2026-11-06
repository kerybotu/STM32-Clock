#include <assert.h>
#include <string.h>
#include "../weekday_display.h"

int main(void) {
    assert(strcmp(WeekdayAbbreviation(1), "MON") == 0);
    assert(strcmp(WeekdayAbbreviation(2), "TUE") == 0);
    assert(strcmp(WeekdayAbbreviation(3), "WED") == 0);
    assert(strcmp(WeekdayAbbreviation(4), "THU") == 0);
    assert(strcmp(WeekdayAbbreviation(5), "FRI") == 0);
    assert(strcmp(WeekdayAbbreviation(6), "SAT") == 0);
    assert(strcmp(WeekdayAbbreviation(7), "SUN") == 0);
    assert(WeekdayAbbreviation(0) == 0);
    assert(WeekdayAbbreviation(8) == 0);
    return 0;
}
