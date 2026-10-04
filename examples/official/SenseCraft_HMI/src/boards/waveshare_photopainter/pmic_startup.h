#pragma once

#include <stdint.h>
#include "boards/waveshare_photopainter/config.h"

// Pure startup policy, host-testable with a native-bus adapter. No rail enable.
namespace photopainter_pmic {
constexpr uint8_t kChipIdRegister = 0x03;
constexpr uint8_t kExpectedChipId = 0x4A;
constexpr unsigned kAttempts = 3;

enum class Failure { none, bus_begin, id_read, chip_id, library_init };
struct Attempt {
    int read_error = -1;
    const char* error_name = "NOT_ATTEMPTED";
    int callback_error = -1;
    int chip_id = -1;
    Failure failure = Failure::none;
};
struct Startup {
    bool ready = false;
    bool bus_ready = false;
    unsigned count = 0;
    int sda_before = -1, scl_before = -1, sda_after = -1, scl_after = -1;
    bool rtc_checked = false;
    const char* rtc_error_name = "NOT_ATTEMPTED";
    Attempt attempts[kAttempts];
};

template<class Bus, class Pmic, class Delay>
Startup initialize(Bus& bus, Pmic& pmic, Delay pause) {
    Startup result;
    result.bus_ready = bus.begin(PHOTOPAINTER_PMIC_SDA, PHOTOPAINTER_PMIC_SCL, 100000);
    if (!result.bus_ready) {
        result.count = 1;
        result.attempts[0].failure = Failure::bus_begin;
        return result;
    }
    result.sda_before = bus.sdaLevel();
    result.scl_before = bus.sclLevel();
    // A native register read is the probe. Do NOT require Arduino's unrelated
    // zero-length address-only transaction to work before attempting this.
    for (unsigned i = 0; i < kAttempts; ++i) {
        auto& attempt = result.attempts[i];
        ++result.count;
        attempt.read_error = bus.readId(attempt.chip_id);
        attempt.error_name = bus.errorName(attempt.read_error);
        if (attempt.read_error != 0 || attempt.chip_id < 0) {
            attempt.failure = Failure::id_read;
        } else if (attempt.chip_id != kExpectedChipId) {
            attempt.failure = Failure::chip_id;
        } else if (!bus.bind(pmic)) {
            attempt.callback_error = bus.callbackError();
            attempt.failure = Failure::library_init;
        } else {
            attempt.callback_error = bus.callbackError();
            result.ready = true;
            break;
        }
        if (i + 1 < kAttempts) pause(20);
    }
    result.sda_after = bus.sdaLevel();
    result.scl_after = bus.sclLevel();
    if (!result.ready) {
        result.rtc_checked = true;
        result.rtc_error_name = bus.errorName(bus.readRtcControl());
    }
    return result;
}

inline const char* failureName(Failure failure) {
    switch (failure) {
    case Failure::none: return "OK";
    case Failure::bus_begin: return "NATIVE_BUS_BEGIN_FAILED";
    case Failure::id_read: return "NATIVE_ID_READ_FAILED";
    case Failure::chip_id: return "CHIP_ID_MISMATCH";
    case Failure::library_init: return "XPOWERS_CALLBACK_INIT_FAILED";
    }
    return "UNKNOWN";
}

template<class Output>
void report(Output& out, const Startup& result) {
    out.printf("[PhotoPainter PMIC] diagnostics=v1-20261005-3 SDA=%d SCL=%d address=0x%02X clock=100000 timeout=50ms bus=%s transport=native-repeated-start\r\n",
               PHOTOPAINTER_PMIC_SDA, PHOTOPAINTER_PMIC_SCL,
               PHOTOPAINTER_PMIC_ADDRESS, result.bus_ready ? "CONFIGURED" : "FAILED");
    out.printf("[PhotoPainter I2C] idle_before SDA=%d SCL=%d idle_after SDA=%d SCL=%d\r\n",
               result.sda_before, result.scl_before, result.sda_after, result.scl_after);
    for (unsigned i = 0; i < result.count; ++i) {
        const auto& a = result.attempts[i];
        out.printf("[PhotoPainter PMIC] attempt=%u read_err=%d (%s) chip_id=%d expected=0x%02X callback_err=%d result=%s\r\n",
                   i + 1, a.read_error, a.error_name, a.chip_id,
                   kExpectedChipId, a.callback_error, failureName(a.failure));
    }
    if (result.rtc_checked) out.printf("[PhotoPainter I2C] RTC_0x51_Control1_read=%s (read-only peer check)\r\n", result.rtc_error_name);
    if (!result.ready) out.println("[PhotoPainter PMIC] Display NOT started; EPD_VCC off is UNCONFIRMED. Disconnect ALL power (including battery) if failure repeats.");
}
} // namespace photopainter_pmic
