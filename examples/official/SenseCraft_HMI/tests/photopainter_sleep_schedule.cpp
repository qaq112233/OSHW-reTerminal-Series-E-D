#include "boards/common/sleep_schedule.h"
#include <cassert>
#include <stdint.h>

int main() {
    assert(sleep_schedule::wakeup_delay_seconds(0) == 1);
    assert(sleep_schedule::wakeup_delay_seconds(1) == 1);
    assert(sleep_schedule::wakeup_delay_seconds(19) == 1);
    assert(sleep_schedule::wakeup_delay_seconds(20) == 1);
    assert(sleep_schedule::wakeup_delay_seconds(21) == 1);
    assert(sleep_schedule::wakeup_delay_seconds(1800) == 1780);
    assert(sleep_schedule::wakeup_delay_seconds(UINT32_MAX) == UINT32_MAX - 20);
}
