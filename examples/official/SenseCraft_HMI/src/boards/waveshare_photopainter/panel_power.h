#pragma once

#include <Arduino.h>
#include <initializer_list>
#include <atomic>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#define XPOWERS_CHIP_AXP2101
#include <XPowersLib.h>
#include "TFT_eSPI.h"

// V1 schematic: AXP2101 ALDO3 (pin 16) -> EPD_VCC -> panel J1 VDD (pin 38).
// Keep this board-only sequence separate from Seeed_GFX's shared ED2208 driver.
namespace photopainter_panel {

inline bool railOn(XPowersPMU& pmic) {
    if (!pmic.enableALDO3()) return false;
    const int state = pmic.readRegister(XPOWERS_AXP2101_LDO_ONOFF_CTRL0);
    if (state < 0 || (state & 0x04) == 0) return false;
    delay(20);  // allow the panel supply to settle before hardware reset
    return true;
}

inline bool railOff(XPowersPMU& pmic) {
    // A failed write or read is not proof that VDD is off; retry boundedly.
    for (unsigned attempt = 0; attempt < 3; ++attempt) {
        pmic.disableALDO3();
        const int state = pmic.readRegister(XPOWERS_AXP2101_LDO_ONOFF_CTRL0);
        if (state >= 0 && (state & 0x04) == 0) return true;
        delay(10);
    }
    return false;
}

// Seeed_GFX ED2208's CHECK_BUSY has no timeout. This separate FreeRTOS task
// attempts physical rail-off and resets the MCU if the driver never returns.
// 180 s is a failure bound, not a claim about the panel's refresh duration.
class PanelPowerGuard {
public:
    bool start(XPowersPMU& pmic) {
        pmic_ = &pmic;
        return xTaskCreatePinnedToCore(run, "epd_rail_guard", 4096, this, 3,
                                       nullptr, 1) == pdPASS;
    }
    void arm() {
        started_ms_.store(millis(), std::memory_order_relaxed);
        active_.store(true, std::memory_order_release);
    }
    void disarm() { active_.store(false, std::memory_order_release); }
private:
    static void run(void* context) {
        auto& self = *static_cast<PanelPowerGuard*>(context);
        while (true) {
            if (self.active_.load(std::memory_order_acquire) &&
                millis() - self.started_ms_.load(std::memory_order_relaxed) > 180000) {
                // Fault recovery: BUSY completion cannot be guaranteed when
                // stuck; do not wait indefinitely with EPD_VCC powered.
                Serial.println("[PhotoPainter] EPD rail timeout; forcing ALDO3 off and restarting");
                const bool off = railOff(*self.pmic_);
                Serial.println(off ? "[PhotoPainter] ALDO3 off register confirmed" :
                                     "[PhotoPainter] ALDO3 off UNCONFIRMED");
                esp_restart();
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    XPowersPMU* pmic_ = nullptr;
    std::atomic<uint32_t> started_ms_{0};
    std::atomic<bool> active_{false};
};

inline bool waitReady(uint32_t timeout_ms = 90000) {
    const uint32_t started = millis();
    while (digitalRead(TFT_BUSY) == LOW) {
        if (millis() - started > timeout_ms) return false;
        delay(10);
    }
    return true;
}

inline bool initController(EPaper& epd) {
    // Waveshare display_bsp.cpp::EPD_Init, run again after every physical VDD
    // cycle. EPaper::begin() cannot be repeated: Seeed_GFX guards its init.
    digitalWrite(TFT_RST, HIGH);
    delay(50);
    digitalWrite(TFT_RST, LOW);
    delay(20);
    digitalWrite(TFT_RST, HIGH);
    delay(50);
    if (!waitReady()) return false;
    delay(50);
    auto send = [&epd](uint8_t cmd, std::initializer_list<uint8_t> bytes) {
        epd.writecommand(cmd);
        for (uint8_t byte : bytes) epd.writedata(byte);
    };
    send(0xAA, {0x49, 0x55, 0x20, 0x08, 0x09, 0x18});
    send(0x01, {0x3F});
    send(0x00, {0x5F, 0x69});
    send(0x03, {0x00, 0x54, 0x00, 0x44});
    send(0x05, {0x40, 0x1F, 0x1F, 0x2C});
    send(0x06, {0x6F, 0x1F, 0x17, 0x49});
    send(0x08, {0x6F, 0x1F, 0x1F, 0x22});
    send(0x30, {0x03});
    send(0x50, {0x3F});
    send(0x60, {0x02, 0x00});
    send(0x61, {0x03, 0x20, 0x01, 0xE0});
    send(0x84, {0x01});
    send(0xE3, {0x2F});
    epd.writecommand(0x04);
    return waitReady();
}

}  // namespace photopainter_panel
