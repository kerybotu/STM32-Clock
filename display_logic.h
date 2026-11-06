#ifndef DISPLAY_LOGIC_H
#define DISPLAY_LOGIC_H

#include <stdint.h>

static inline uint8_t ProgressTarget(uint8_t second) {
    return (uint8_t)(((uint16_t)second * 80u) / 60u);
}

static inline uint8_t ProgressByte(uint8_t column, uint8_t filled) {
    if (column == 79u) return 0x7Eu;
    return (column < filled) ? 0x3Cu : 0x24u;
}

#endif
