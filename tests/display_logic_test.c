#include <assert.h>
#include <stdint.h>
#include "../display_logic.h"

int main(void) {
    assert(ProgressTarget(0) == 0);
    assert(ProgressTarget(30) == 40);
    assert(ProgressTarget(59) == 78);
    assert(ProgressByte(0, 0) == 0x24);
    assert(ProgressByte(39, 40) == 0x3C);
    assert(ProgressByte(40, 40) == 0x24);
    assert(ProgressByte(79, 78) == 0x7E);
    assert(ProgressTarget(0) < ProgressTarget(59));
    return 0;
}
