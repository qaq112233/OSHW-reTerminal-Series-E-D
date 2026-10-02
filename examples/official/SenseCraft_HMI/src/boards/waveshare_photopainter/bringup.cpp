#if defined(PHOTOPAINTER_DISPLAY_BRINGUP)

#include <Arduino.h>
#include "TFT_eSPI.h"
#include "boards/waveshare_photopainter/config.h"
#include "boards/waveshare_photopainter/panel_power.h"
#include <Wire.h>
#include "boards/waveshare_photopainter/pmic_startup.h"

static_assert(photopainter_pmic::kExpectedChipId == XPOWERS_AXP2101_CHIP_ID, "PMIC ID drift");
static_assert(photopainter_pmic::kChipIdRegister == XPOWERS_AXP2101_IC_TYPE, "PMIC register drift");

namespace {

EPaper* display = nullptr;
XPowersPMU pmic;
photopainter_panel::PanelPowerGuard rail_guard;
bool panel_ready = false;
constexpr uint16_t kColors[] = {
    TFT_WHITE, TFT_BLACK, TFT_RED, TFT_YELLOW, TFT_BLUE, TFT_GREEN
};
constexpr const char* kNames[] = {
    "WHITE", "BLACK", "RED", "YELLOW", "BLUE", "GREEN"
};
constexpr size_t kColorCount = sizeof(kColors) / sizeof(kColors[0]);
size_t step = 0;
bool last_pressed = false;
uint32_t last_press_ms = 0;

void show_step() {
    const bool orientation_test = step >= kColorCount;
    const uint8_t rotation = (2 + (orientation_test ? step - kColorCount : 0)) % 4;
    const uint16_t background = orientation_test ? TFT_WHITE : kColors[step];
    const uint16_t foreground = background == TFT_BLACK ? TFT_WHITE : TFT_BLACK;

    display->setRotation(rotation);
    display->fillScreen(background);
    display->drawRect(0, 0, display->width(), display->height(), foreground);
    display->drawRect(8, 8, display->width() - 16, display->height() - 16, foreground);
    display->setTextColor(foreground, background);
    display->setTextSize(3);
    display->setCursor(30, 36);
    display->print(orientation_test ? "ROTATION" : kNames[step]);
    display->setCursor(30, 78);
    display->printf("%u / %u   %u x %u", static_cast<unsigned>(step + 1),
                    static_cast<unsigned>(kColorCount + 4),
                    display->width(), display->height());
    display->drawLine(0, 0, display->width() - 1, display->height() - 1, foreground);
    display->drawLine(display->width() - 1, 0, 0, display->height() - 1, foreground);

    // Seeed_GFX ED2208: POWER_ON, framebuffer, DISPLAY_REFRESH, BUSY,
    // POWER_OFF (0x02), BUSY. Never wait for a key while the driver is awake.
    rail_guard.arm();
    if (!photopainter_panel::railOn(pmic)) {
        if (photopainter_panel::railOff(pmic)) rail_guard.disarm();
        else esp_restart();
        Serial.println("[Bring-up] ALDO3 enable failed; no refresh");
        return;
    }
    if (!photopainter_panel::initController(*display)) {
        Serial.println("[Bring-up] EPD init BUSY timeout; turning rail off");
        if (photopainter_panel::railOff(pmic)) rail_guard.disarm();
        else esp_restart();
        return;
    }
    display->update();
    if (!photopainter_panel::railOff(pmic)) {
        panel_ready = false;
        Serial.println("[Bring-up] ALDO3 disable failed; restarting");
        esp_restart();
        return;
    }
    rail_guard.disarm();
    Serial.printf("[Bring-up] pattern %u done; controller POWER_OFF, EPD_VCC rail off; press KEY for next\n",
                  static_cast<unsigned>(step + 1));
}

}  // namespace

void setup() {
    Serial.begin(115200);
    pinMode(PHOTOPAINTER_KEY_BUTTON, INPUT_PULLUP);
    const auto startup = photopainter_pmic::initialize(Wire, pmic, [](unsigned ms) { delay(ms); });
    if (!startup.ready) {
        // No display calls or rail enable on failure. Repeat the retained
        // evidence briefly so USB CDC can reconnect before the reset.
        for (unsigned i = 0; i < 4; ++i) {
            photopainter_pmic::report(Serial, startup);
            delay(250);
        }
        Serial.println("[Bring-up] PMIC initialization failed; restarting (EPD_VCC off UNCONFIRMED)");
        Serial.flush();
        esp_restart();
        return;
    }
    if (!rail_guard.start(pmic)) {
        Serial.println("[Bring-up] Rail watchdog unavailable; no display");
        if (!photopainter_panel::railOff(pmic)) esp_restart();
        return;
    }
    if (!photopainter_panel::railOff(pmic)) {
        rail_guard.arm(); // retry if boot left ALDO3 on
        Serial.println("[Bring-up] Cannot shut down EPD_VCC at boot; restarting");
        esp_restart();
        return;
    }
    // Only wait for USB after boot rail-off has been confirmed. No wait for a
    // host is allowed while the panel is enabled.
    const uint32_t usb_started = millis();
    while (!Serial && millis() - usb_started < 1500) delay(10);
    photopainter_pmic::report(Serial, startup);
    Serial.println("[Bring-up] ALDO3 off register confirmed at boot; starting display test");
    if (!pmic.setALDO3Voltage(3300)) {
        Serial.println("[Bring-up] ALDO3 voltage setup failed");
        return;
    }
    rail_guard.arm();
    if (!photopainter_panel::railOn(pmic)) {
        if (photopainter_panel::railOff(pmic)) rail_guard.disarm();
        else esp_restart();
        Serial.println("[Bring-up] EPD_VCC enable failed");
        return;
    }
    // Construct only after the Arduino runtime has initialized PSRAM.
    display = new EPaper();
    display->begin();
    if (!photopainter_panel::initController(*display)) {
        if (photopainter_panel::railOff(pmic)) rail_guard.disarm();
        else esp_restart();
        Serial.println("[Bring-up] EPD init BUSY timeout");
        return;
    }
    display->wake();
    display->sleep();
    if (!photopainter_panel::railOff(pmic)) {
        Serial.println("[Bring-up] ALDO3 disable failed; restarting");
        esp_restart();
        return;
    }
    rail_guard.disarm();
    Serial.printf("[Bring-up] display allocated: %u x %u; PSRAM=%u bytes\n",
                  display->width(), display->height(), ESP.getPsramSize());
    panel_ready = true;
    show_step();
}

void loop() {
    const bool pressed = digitalRead(PHOTOPAINTER_KEY_BUTTON) == LOW;
    if (panel_ready && pressed && !last_pressed && millis() - last_press_ms > 250) {
        last_press_ms = millis();
        step = (step + 1) % (kColorCount + 4);
        show_step();
    }
    last_pressed = pressed;
    delay(20);
}

#endif  // PHOTOPAINTER_DISPLAY_BRINGUP
