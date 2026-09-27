#include "boards/waveshare_photopainter/axp2101_status.h"
#include <cassert>

int main() {
    using namespace photopainter_axp2101;
    assert(battery_connected(0x08));
    assert(!battery_connected(0));
    assert(external_power(0x20, 0x00));
    assert(!external_power(0x20, 0x08));
    assert(!external_power(0, 0));
    assert(charging_or_external_power(0x00, 0x20));
    assert(charging_or_external_power(0x20, 0x00)); // full/no battery on USB
    assert(!charging_or_external_power(0x00, 0x40)); // discharge on battery
}
