#pragma once

#include <stdint.h>
#include "boards/waveshare_photopainter/config.h"

// No GPIO/address guessing and no rail-enable operations here. Testable on the
// host; constants are checked against XPowersLib in the firmware callers.
namespace photopainter_pmic {
constexpr uint8_t kChipIdRegister = 0x03;
constexpr uint8_t kExpectedChipId = 0x4A;
constexpr unsigned kAttempts = 3;

enum class Failure { none, bus_begin, address_ack, id_read, chip_id, library_init };
struct Attempt {
    uint8_t ack = 255;
    uint8_t id_status = 255;
    unsigned received = 0;
    int chip_id = -1;
    Failure failure = Failure::none;
};
struct Startup {
    bool ready = false;
    bool bus_ready = false;
    unsigned count = 0;
    Attempt attempts[kAttempts];
};

// begin()'s result was previously discarded inside XPowersLib. Establish the
// documented bus explicitly first, then use exactly the library's stop/read
// transaction and strict chip ID. Retrying does NOT ignore identification.
template<class Bus, class Pmic, class Delay>
Startup initialize(Bus& bus, Pmic& pmic, Delay pause) {
    Startup result;
    result.bus_ready = bus.begin(PHOTOPAINTER_PMIC_SDA, PHOTOPAINTER_PMIC_SCL, 100000);
    if (!result.bus_ready) {
        result.count = 1;
        result.attempts[0].failure = Failure::bus_begin;
        return result;
    }
    bus.setClock(100000);
    bus.setTimeOut(50);
    for (unsigned i = 0; i < kAttempts; ++i) {
        auto& attempt = result.attempts[i];
        ++result.count;
        bus.beginTransmission(PHOTOPAINTER_PMIC_ADDRESS);
        attempt.ack = bus.endTransmission();
        if (attempt.ack != 0) {
            attempt.failure = Failure::address_ack;
        } else {
            bus.beginTransmission(PHOTOPAINTER_PMIC_ADDRESS);
            bus.write(kChipIdRegister);
            attempt.id_status = bus.endTransmission();
            if (attempt.id_status == 0) {
                attempt.received = bus.requestFrom(uint8_t(PHOTOPAINTER_PMIC_ADDRESS), uint8_t(1));
                if (attempt.received == 1 && bus.available()) attempt.chip_id = bus.read();
            }
            if (attempt.chip_id < 0) {
                attempt.failure = Failure::id_read;
            } else if (attempt.chip_id != kExpectedChipId) {
                attempt.failure = Failure::chip_id;
            } else if (!pmic.init(bus, PHOTOPAINTER_PMIC_SDA,
                                  PHOTOPAINTER_PMIC_SCL, PHOTOPAINTER_PMIC_ADDRESS)) {
                attempt.failure = Failure::library_init;
            } else {
                result.ready = true;
                return result;
            }
        }
        if (i + 1 < kAttempts) pause(20);
    }
    return result;
}

inline const char* failureName(Failure failure) {
    switch (failure) {
    case Failure::none: return "OK";
    case Failure::bus_begin: return "WIRE_BEGIN_FAILED";
    case Failure::address_ack: return "PMIC_NO_ACK";
    case Failure::id_read: return "CHIP_ID_READ_FAILED";
    case Failure::chip_id: return "CHIP_ID_MISMATCH";
    case Failure::library_init: return "XPOWERS_INIT_FAILED";
    }
    return "UNKNOWN";
}

template<class Output>
void report(Output& out, const Startup& result) {
    out.printf("[PhotoPainter PMIC] diagnostics=v1-20261002-1 SDA=%d SCL=%d address=0x%02X clock=100000 timeout=50ms bus=%s\n",
               PHOTOPAINTER_PMIC_SDA, PHOTOPAINTER_PMIC_SCL,
               PHOTOPAINTER_PMIC_ADDRESS, result.bus_ready ? "OK" : "FAILED");
    for (unsigned i = 0; i < result.count; ++i) {
        const auto& a = result.attempts[i];
        out.printf("[PhotoPainter PMIC] attempt=%u ACK=%u ID_TX=%u received=%u chip_id=%d expected=0x%02X result=%s\n",
                   i + 1, a.ack, a.id_status, a.received, a.chip_id,
                   kExpectedChipId, failureName(a.failure));
    }
    // Decimal ID avoids treating failed reads (-1) as an actual 0xFF chip.
    // 74 = 0x4A. 255 in ACK/ID_TX means that stage was not attempted.
    if (!result.ready) out.println("[PhotoPainter PMIC] Display NOT started; EPD_VCC off is UNCONFIRMED. Disconnect ALL power (including battery) if failure repeats.");
}
} // namespace photopainter_pmic
