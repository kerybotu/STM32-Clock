#include <assert.h>
#include <stdint.h>
#include "../button_state.h"

static void test_short_press_waits_for_debounced_release(void) {
    ButtonState key = {0};
    assert(Key_Update(&key, 1, 0) == 0);
    assert(Key_Update(&key, 1, 20) == 0);
    assert(Key_Update(&key, 1, 21) == 0);
    assert(Key_Update(&key, 1, 22) == 0);
    assert(Key_Update(&key, 1, 23) == 0);
    assert(Key_Update(&key, 1, 24) == 0);
    assert(Key_Update(&key, 0, 100) == 0);
    assert(Key_Update(&key, 0, 119) == 0);
    assert(Key_Update(&key, 0, 120) == 1);
    assert(Key_Update(&key, 0, 121) == 0);
}

static void test_long_press_fires_at_threshold_without_release(void) {
    ButtonState key = {0};
    Key_Update(&key, 1, 0);
    Key_Update(&key, 1, 20);
    Key_Update(&key, 1, 21);
    Key_Update(&key, 1, 22);
    Key_Update(&key, 1, 23);
    Key_Update(&key, 1, 24);
    assert(Key_Update(&key, 1, 1519) == 0);
    assert(Key_Update(&key, 1, 1520) == 2);
    assert(Key_Update(&key, 1, 4000) == 0);
    assert(Key_Update(&key, 0, 4010) == 0);
    assert(Key_Update(&key, 0, 4030) == 0);
}

static void test_press_duration_survives_millisecond_wrap(void) {
    ButtonState key = {0};
    Key_Update(&key, 1, UINT32_MAX - 30u);
    Key_Update(&key, 1, UINT32_MAX - 10u);
    Key_Update(&key, 1, UINT32_MAX - 9u);
    Key_Update(&key, 1, UINT32_MAX - 8u);
    Key_Update(&key, 1, UINT32_MAX - 7u);
    Key_Update(&key, 1, UINT32_MAX - 6u);
    assert(Key_Update(&key, 1, 1488u) == 0);
    assert(Key_Update(&key, 1, 1489u) == 2);
}

int main(void) {
    test_short_press_waits_for_debounced_release();
    test_long_press_fires_at_threshold_without_release();
    test_press_duration_survives_millisecond_wrap();
    return 0;
}
