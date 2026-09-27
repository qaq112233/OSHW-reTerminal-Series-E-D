#if defined(BOARD_WAVESHARE_PHOTOPAINTER)

#include "boards/common/board.h"
#include "boards/waveshare_photopainter/config.h"
#include "boards/waveshare_photopainter/axp2101_status.h"

#include <Wire.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#define XPOWERS_CHIP_AXP2101
#include <XPowersLib.h>
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
        // XPowersLib::init() initializes Wire on the Waveshare pins.
        pmic_ready_ = pmic_.init(Wire, PHOTOPAINTER_PMIC_SDA,
                                 PHOTOPAINTER_PMIC_SCL, PHOTOPAINTER_PMIC_ADDRESS);
        Wire.setClock(100000);
        if (!pmic_ready_) {
            Log.warningln("[PhotoPainter] AXP2101 init failed; power status unavailable");
        } else {
            // Mirror Waveshare power_bsp.cpp's register configuration.
            // Voltage selection does not imply enabling or disabling a rail;
            // the EPD power-domain mapping still needs the board schematic.
            if (pmic_.getDC1Voltage() != 3300) pmic_.setDC1Voltage(3300);
            if (pmic_.getALDO1Voltage() != 3300) pmic_.setALDO1Voltage(3300);
            if (pmic_.getALDO2Voltage() != 3300) pmic_.setALDO2Voltage(3300);
            if (pmic_.getALDO3Voltage() != 3300) pmic_.setALDO3Voltage(3300);
            if (pmic_.getALDO4Voltage() != 3300) pmic_.setALDO4Voltage(3300);
            pmic_.setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_2000MA);
            pmic_.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_50MA);
            pmic_.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_500MA);
            pmic_.setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_25MA);
            pmic_.enableBattDetection();
            pmic_.enableBattVoltageMeasure();
        }

        // Seeed_GFX ED2208 init sends POWER_ON (0x04). Its EPaper object
        // initially reports asleep; wake() first synchronizes that state,
        // then sleep() sends POWER_OFF (0x02) and waits for BUSY. Subsequent
        // update() calls wake -> refresh/BUSY -> sleep/BUSY automatically.
        display_.begin();
        // Waveshare display_bsp.cpp::EPD_Init uses PLL 0x30 = 0x03;
        // Seeed_GFX ED2208 defaults to 0x08 for the reTerminal E1002.
        display_.native().writecommand(0x30);
        display_.native().writedata(0x03);
        display_.native().wake();
        display_.native().sleep();
        // Activation and waiting pages may be the first draw; the image
        // parser sets rotation itself, but these pages use the initial state.
        display_.native().setRotation(display_.rotation());
        Log.infoln("[PhotoPainter] ED2208 initialized and controller powered off");
    }

    void PrepareForDeepSleep() override {
        // Seeed_GFX ED2208 sleep() is idempotent and sends 0x02/BUSY
        // if the last display operation left the controller awake.
        display_.native().sleep();
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
    Led led_;
    Button boot_;
    Button key_;
    Button power_;
    EpaperDisplay display_;
    XPowersPMU pmic_;
    bool pmic_ready_ = false;
};

}  // namespace

DECLARE_BOARD(PhotoPainterBoard)

#endif  // BOARD_WAVESHARE_PHOTOPAINTER
