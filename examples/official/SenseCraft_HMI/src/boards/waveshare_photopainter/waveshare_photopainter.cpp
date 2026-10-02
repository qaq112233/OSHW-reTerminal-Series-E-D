#if defined(BOARD_WAVESHARE_PHOTOPAINTER)

#include "boards/common/board.h"
#include "boards/waveshare_photopainter/config.h"
#include "boards/waveshare_photopainter/axp2101_status.h"
#include "boards/waveshare_photopainter/panel_power.h"

#include <Wire.h>
#include "boards/waveshare_photopainter/pmic_startup.h"

static_assert(photopainter_pmic::kExpectedChipId == XPOWERS_AXP2101_CHIP_ID, "PMIC ID drift");
static_assert(photopainter_pmic::kChipIdRegister == XPOWERS_AXP2101_IC_TYPE, "PMIC register drift");
#include <esp_sleep.h>
#include <driver/rtc_io.h>

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
        // Reset recovery: turn off EPD_VCC before mounting LittleFS or
        // starting network services (a previous refresh may have reset).
        const auto startup = photopainter_pmic::initialize(Wire, pmic_, [](unsigned ms) { delay(ms); });
        pmic_ready_ = startup.ready;
        if (!pmic_ready_) {
            for (unsigned i = 0; i < 4; ++i) {
                photopainter_pmic::report(Serial, startup);
                delay(250);
            }
            Serial.flush();
            Log.errorln("[PhotoPainter] PMIC unavailable; cannot establish safe EPD_VCC state");
            esp_restart();
            return;
        }
        guard_ready_ = rail_guard_.start(pmic_);
        if (!photopainter_panel::railOff(pmic_)) {
            // The guard may retry rail-off if restart stalls.
            if (guard_ready_) rail_guard_.arm();
            pmic_ready_ = false;
            Log.errorln("[PhotoPainter] Cannot disable EPD_VCC at startup; restarting");
            esp_restart();
            return;
        }
        photopainter_pmic::report(Serial, startup);
        Serial.println("[PhotoPainter] ALDO3 off register confirmed at boot");
    }

    void InitHardware() override {
        if (!pmic_ready_) {
            Log.errorln("[PhotoPainter] AXP2101 unavailable; display disabled");
            return;
        } else {
            // Mirror Waveshare power_bsp.cpp's register configuration.
            // V1 schematic maps ALDO3 to EPD_VCC; voltage selection happens
            // while the rail is off, then RefreshDisplay switches it on/off.
            if (pmic_.getDC1Voltage() != 3300) pmic_.setDC1Voltage(3300);
            if (pmic_.getALDO1Voltage() != 3300) pmic_.setALDO1Voltage(3300);
            if (pmic_.getALDO2Voltage() != 3300) pmic_.setALDO2Voltage(3300);
            if (pmic_.getALDO3Voltage() != 3300 && !pmic_.setALDO3Voltage(3300)) {
                Log.errorln("[PhotoPainter] Cannot set ALDO3 / EPD_VCC to 3300 mV");
                return;
            }
            if (pmic_.getALDO4Voltage() != 3300) pmic_.setALDO4Voltage(3300);
            pmic_.setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_2000MA);
            pmic_.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_50MA);
            pmic_.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_500MA);
            pmic_.setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_25MA);
            pmic_.enableBattDetection();
            pmic_.enableBattVoltageMeasure();
        }

        if (!guard_ready_) {
            Log.errorln("[PhotoPainter] Rail watchdog unavailable; display disabled");
            return;
        }

        // Seeed_GFX sets up SPI and the sprite once; the V1 panel then needs
        // Waveshare's register sequence after every physical VDD cycle.
        rail_guard_.arm();
        if (!photopainter_panel::railOn(pmic_)) {
            Log.errorln("[PhotoPainter] Cannot enable EPD_VCC for init");
            powerOffPanel();
            return;
        }
        rail_on_ = true;
        display_.begin();
        if (!photopainter_panel::initController(display_.native())) {
            Log.errorln("[PhotoPainter] EPD init BUSY timeout");
            powerOffPanel();
            return;
        }
        display_.native().wake(); // synchronize Seeed_GFX initial sleep flag
        display_.native().sleep(); // POWER_OFF + BUSY, before ALDO3 off
        if (!powerOffPanel()) return;
        display_.native().setRotation(display_.rotation());
        display_ready_ = true;
        Log.infoln("[PhotoPainter] ED2208 initialized; EPD_VCC off");
    }

    bool RefreshDisplay() override {
        if (!display_ready_ || !pmic_ready_) return false;
        if (rail_on_ && !powerOffPanel()) return false;
        rail_guard_.arm();
        if (!photopainter_panel::railOn(pmic_)) {
            Log.errorln("[PhotoPainter] Cannot enable EPD_VCC for refresh");
            powerOffPanel();
            return false;
        }
        rail_on_ = true;
        if (!photopainter_panel::initController(display_.native())) {
            Log.errorln("[PhotoPainter] EPD reinit BUSY timeout");
            powerOffPanel();
            return false;
        }
        // Reuse Seeed_GFX framebuffer transfer, refresh/BUSY, POWER_OFF/BUSY.
        display_.native().update();
        return powerOffPanel();
    }

    bool PrepareForDeepSleep() override {
        // Seeed_GFX ED2208 sleep() is idempotent and sends 0x02/BUSY
        // if the last display operation left the controller awake.
        if (rail_on_) {
            display_.native().sleep();
        }
        if (!powerOffPanel()) return false;
        // Waveshare Basic_mode uses GPIO0/4 as active-low EXT1 sources and
        // enables RTC pull-up on GPIO4. The common init path can return early
        // on low battery, so configure wakeup here as well.
        constexpr uint64_t mask = (1ULL << PHOTOPAINTER_BOOT_BUTTON) |
                                  (1ULL << PHOTOPAINTER_KEY_BUTTON);
        esp_err_t err = esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW);
        if (err != ESP_OK) {
            Log.errorln("[PhotoPainter] EXT1 wakeup configuration failed: %d", err);
        }
        rtc_gpio_pulldown_dis(static_cast<gpio_num_t>(PHOTOPAINTER_KEY_BUTTON));
        rtc_gpio_pullup_en(static_cast<gpio_num_t>(PHOTOPAINTER_KEY_BUTTON));
        // A held wake key causes an immediate wake-loop with ANY_LOW.
        // Wait for both inputs to be released, matching Waveshare's BOOT
        // release wait, with the panel already POWER_OFF/BUSY complete.
        while (boot_.isPressed() || key_.isPressed()) {
            delay(50);
        }
        return true;
    }

    bool ReadPowerStatus(BoardPowerStatus& status) override {
        if (!pmic_ready_) return false;
        // XPowersLib follows the same AXP2101 status/battery registers as
        // Waveshare. Guard failed I2C reads before interpreting any bits.
        const int battery = pmic_.readRegister(XPOWERS_AXP2101_STATUS1);
        const int power = pmic_.readRegister(XPOWERS_AXP2101_STATUS2);
        if (battery < 0 || power < 0) return false;
        status.battery_connected = photopainter_axp2101::battery_connected(battery);
        status.charging = photopainter_axp2101::charging_or_external_power(battery, power);
        if (status.battery_connected) {
            const int percent = pmic_.getBatteryPercent();
            const uint16_t millivolts = pmic_.getBattVoltage();
            if (percent >= 0 && percent <= 100) status.battery_percent = percent;
            if (millivolts >= 2500 && millivolts <= 5000)
                status.battery_voltage = millivolts / 1000.0f;
        }
        return true;
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
    bool powerOffPanel() {
        if (!pmic_ready_) return false;
        if (!photopainter_panel::railOff(pmic_)) {
            // Covers a failed deep-sleep check even if the last refresh was
            // already marked off; keep retrying instead of trusting a flag.
            if (guard_ready_) rail_guard_.arm();
            Log.errorln("[PhotoPainter] Failed to disable ALDO3 / EPD_VCC; restarting");
            esp_restart(); // never continue into networking/idle/deep sleep
            return false;
        }
        rail_on_ = false;
        rail_guard_.disarm();
        return true;
    }

    Led led_;
    Button boot_;
    Button key_;
    Button power_;
    EpaperDisplay display_;
    XPowersPMU pmic_;
    photopainter_panel::PanelPowerGuard rail_guard_;
    bool guard_ready_ = false;
    bool pmic_ready_ = false;
    bool display_ready_ = false;
    bool rail_on_ = false;
};

}  // namespace

DECLARE_BOARD(PhotoPainterBoard)

#endif  // BOARD_WAVESHARE_PHOTOPAINTER
