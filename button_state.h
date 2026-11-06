#ifndef BUTTON_STATE_H
#define BUTTON_STATE_H

#include <stdint.h>

#define KEY_DEBOUNCE_MS 20u
#define KEY_LONG_MS     1500u

typedef struct {
    uint8_t raw_last;
    uint8_t stable;
    uint8_t pressed;
    uint8_t long_sent;
    uint32_t change_ms;
    uint32_t press_ms;
} ButtonState;

/* raw: 1=pressed, 0=released; returns 1=short, 2=long. */
static inline uint8_t Key_Update(ButtonState *key, uint8_t raw, uint32_t now) {
    if (raw != key->raw_last) {
        key->raw_last = raw;
        key->change_ms = now;
        return 0;
    }
    if ((uint32_t)(now - key->change_ms) < KEY_DEBOUNCE_MS) return 0;

    if (raw != key->stable) {
        key->stable = raw;
        if (raw) {
            key->pressed = 1;
            key->press_ms = now;
            key->long_sent = 0;
        } else {
            key->pressed = 0;
            return key->long_sent ? 0 : 1;
        }
    }

    if (key->pressed && !key->long_sent &&
        (uint32_t)(now - key->press_ms) >= KEY_LONG_MS) {
        key->long_sent = 1;
        return 2;
    }
    return 0;
}

#endif
