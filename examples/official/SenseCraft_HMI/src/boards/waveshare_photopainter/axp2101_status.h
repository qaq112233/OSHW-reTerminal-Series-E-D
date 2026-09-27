#pragma once
#include <stdint.h>

// AXP2101 STATUS1 (0x00) / STATUS2 (0x01) bit definitions as used by
// Waveshare's XPowersAXP2101. The legacy SenseCraft `charging` flag also
// means external power present (SY6974 uses "bus input" for this flag).
namespace photopainter_axp2101 {
inline bool battery_connected(uint8_t status1) { return status1 & 0x08; }
inline bool external_power(uint8_t status1, uint8_t status2) {
    return (status1 & 0x20) && !(status2 & 0x08);
}
inline bool charging_or_external_power(uint8_t status1, uint8_t status2) {
    return (status2 >> 5) == 1 || external_power(status1, status2);
}
}  // namespace photopainter_axp2101
