#pragma once

#include <stdint.h>

namespace sleep_schedule {
// The firmware wakes 20 seconds before a configured refresh interval to leave
// time for boot/network setup. Short intervals must never wrap to ~136 years.
inline uint32_t wakeup_delay_seconds(uint32_t refresh_interval_seconds) {
    return refresh_interval_seconds > 20 ? refresh_interval_seconds - 20 : 1;
}
}  // namespace sleep_schedule
