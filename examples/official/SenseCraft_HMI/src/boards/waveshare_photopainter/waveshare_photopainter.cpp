#if defined(BOARD_WAVESHARE_PHOTOPAINTER)

#include "boards/common/board.h"
#include "boards/waveshare_photopainter/config.h"

#include <Wire.h>
#include "ArduinoLog.h"
#include "TFT_eSPI.h"

namespace {

class PhotoPainterBoard : public Board {
public:
    PhotoPainterBoard()
        : led_(PHOTOPAINTER_GREEN_LED, LOW),
          boot_(PHOTOPAINTER_BOOT_BUTTON, true),
          key_(PHOTOPAINTER_KEY_BUTTON, true),
          power_(PHOTOPAINTER_POWER_BUTTON, false),
          display_(board_registry::current_board_screen()) {}

    const char* GetBoardType() const override { return "waveshare_photopainter"; }

    void InitSerial() override { Serial.begin(115200); }
    Print& GetSerial() override { return Serial; }
    void SetDebugOutput(bool enabled) override { Serial.setDebugOutput(enabled); }

    void InitEarlyHardware() override {
        led_.init();
        led_.set(false);
        boot_.init();
        key_.init();
        power_.init();
    }

    void InitHardware() override {
        Wire.begin(PHOTOPAINTER_PMIC_SDA, PHOTOPAINTER_PMIC_SCL);
        Wire.setClock(100000);
        Wire.beginTransmission(PHOTOPAINTER_PMIC_ADDRESS);
        if (Wire.endTransmission() != 0) {
            Log.warningln("[PhotoPainter] AXP2101 not detected at 0x34");
        }

        // Seeed_GFX ED2208 init sends POWER_ON (0x04). Its EPaper object
        // initially reports asleep; wake() first synchronizes that state,
        // then sleep() sends POWER_OFF (0x02) and waits for BUSY. Subsequent
        // update() calls wake -> refresh/BUSY -> sleep/BUSY automatically.
        display_.begin();
        display_.native().wake();
        display_.native().sleep();
        Log.infoln("[PhotoPainter] ED2208 initialized and controller powered off");
    }

    void PrepareForDeepSleep() override {
        // Seeed_GFX ED2208 sleep() is idempotent and sends 0x02/BUSY
        // if the last display operation left the controller awake.
        display_.native().sleep();
    }

    Led* GetLed() override { return &led_; }
    Button* GetButton(size_t index) override {
        switch (index) {
        case 0: return &boot_;
        case 1: return &key_;
        case 2: return &power_;
        default: return nullptr;
        }
    }
    EpaperDisplay* GetEpaperDisplay() override { return &display_; }
    EPaper& GetDisplay() override { return display_.native(); }

private:
    Led led_;
    Button boot_;
    Button key_;
    Button power_;
    EpaperDisplay display_;
};

}  // namespace

DECLARE_BOARD(PhotoPainterBoard)

#endif  // BOARD_WAVESHARE_PHOTOPAINTER
