#if defined(PHOTOPAINTER_DISPLAY_BRINGUP)

#include <Arduino.h>
#include "TFT_eSPI.h"
#include "boards/waveshare_photopainter/config.h"

namespace {

EPaper* display = nullptr;
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
    const uint8_t rotation = orientation_test ? (step - kColorCount) : 0;
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
    display->update();
    Serial.printf("[Bring-up] pattern %u done, ED2208 controller off; press KEY for next\n",
                  static_cast<unsigned>(step + 1));
}

}  // namespace

void setup() {
    Serial.begin(115200);
    pinMode(PHOTOPAINTER_KEY_BUTTON, INPUT_PULLUP);
    // Construct after the Arduino runtime has initialized PSRAM.
    display = new EPaper();
    display->begin();
    // ED2208 init powers on the controller while EPaper initially marks it
    // asleep. Synchronize state, then execute the normal POWER_OFF/BUSY path.
    display->wake();
    display->sleep();
    show_step();
}

void loop() {
    const bool pressed = digitalRead(PHOTOPAINTER_KEY_BUTTON) == LOW;
    if (pressed && !last_pressed && millis() - last_press_ms > 250) {
        last_press_ms = millis();
        step = (step + 1) % (kColorCount + 4);
        show_step();
    }
    last_pressed = pressed;
    delay(20);
}

#endif  // PHOTOPAINTER_DISPLAY_BRINGUP
