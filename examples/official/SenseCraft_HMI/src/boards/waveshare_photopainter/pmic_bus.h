#pragma once

#include <stdint.h>
#include <atomic>
#include <driver/i2c.h>
#include <driver/gpio.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "boards/waveshare_photopainter/config.h"
#include "boards/waveshare_photopainter/pmic_gpio_diagnostics.h"

// Waveshare uses XPowers callbacks backed by native combined register reads.
// Arduino 2.0.17 ships IDF 4.4, so use its write_read API rather than copying
// the vendor's IDF 5 driver types or claiming the SDKs are identical.
namespace photopainter_pmic_bus {
constexpr i2c_port_t kPort = I2C_NUM_0;
constexpr unsigned kTimeoutMs = 50;
constexpr unsigned kFilterCycles = 7;

inline std::atomic<int>& lastCallbackError() { static std::atomic<int> error{ESP_OK}; return error; }

inline esp_err_t readRegister(uint8_t address, uint8_t reg, uint8_t* data, uint8_t len) {
    if (!data || !len) return ESP_ERR_INVALID_ARG;
    return i2c_master_write_read_device(kPort, address, &reg, 1, data, len,
                                       pdMS_TO_TICKS(kTimeoutMs));
}

inline int readCallback(uint8_t address, uint8_t reg, uint8_t* data, uint8_t len) {
    if (address != PHOTOPAINTER_PMIC_ADDRESS || !data || !len) {
        lastCallbackError().store(ESP_ERR_INVALID_ARG, std::memory_order_relaxed);
        return -1;
    }
    // Vendor power_bsp.cpp retries callbacks three times with 100 ms pauses.
    for (unsigned attempt = 0; attempt < 3; ++attempt) {
        const esp_err_t error = readRegister(address, reg, data, len);
        lastCallbackError().store(error, std::memory_order_relaxed);
        if (error == ESP_OK) return 0;
        if (attempt < 2) vTaskDelay(pdMS_TO_TICKS(100));
    }
    return -1;
}

inline int writeCallback(uint8_t address, uint8_t reg, uint8_t* data, uint8_t len) {
    if (address != PHOTOPAINTER_PMIC_ADDRESS || !data || !len) {
        lastCallbackError().store(ESP_ERR_INVALID_ARG, std::memory_order_relaxed);
        return -1;
    }
    // Register prefix and payload in ONE write, as in Waveshare i2c_write_buff.
    uint8_t payload[256];
    payload[0] = reg;
    for (unsigned i = 0; i < len; ++i) payload[i + 1] = data[i];
    for (unsigned attempt = 0; attempt < 3; ++attempt) {
        const esp_err_t error = i2c_master_write_to_device(kPort, address, payload,
                                                          unsigned(len) + 1,
                                                          pdMS_TO_TICKS(kTimeoutMs));
        lastCallbackError().store(error, std::memory_order_relaxed);
        if (error == ESP_OK) return 0;
        if (attempt < 2) vTaskDelay(pdMS_TO_TICKS(100));
    }
    return -1;
}

class NativeBus {
public:
    bool begin(int sda, int scl, unsigned frequency) {
        if (installed_) return true;
        before_setup_ = photopainter_gpio::capture();
        i2c_config_t config = {};
        config.mode = I2C_MODE_MASTER;
        config.sda_io_num = static_cast<gpio_num_t>(sda);
        config.scl_io_num = static_cast<gpio_num_t>(scl);
        config.sda_pullup_en = GPIO_PULLUP_ENABLE;
        config.scl_pullup_en = GPIO_PULLUP_ENABLE;
        config.master.clk_speed = frequency;
        config_error_ = i2c_param_config(kPort, &config);
        if (config_error_ != ESP_OK) return false;
        install_error_ = i2c_driver_install(kPort, I2C_MODE_MASTER, 0, 0, 0);
        if (install_error_ != ESP_OK) return false;
        filter_error_ = i2c_filter_enable(kPort, kFilterCycles);
        if (filter_error_ != ESP_OK) {
            i2c_driver_delete(kPort); // only delete the driver we just installed
            return false;
        }
        installed_ = true;
        after_setup_ = photopainter_gpio::capture();
        return true;
    }

    int readId(int& id) {
        uint8_t value = 0;
        const int error = readRegister(PHOTOPAINTER_PMIC_ADDRESS, 0x03, &value, 1);
        id = error == ESP_OK ? int(value) : -1;
        return error;
    }
    template<class Pmic>
    bool bind(Pmic& pmic) {
        return pmic.begin(PHOTOPAINTER_PMIC_ADDRESS, readCallback, writeCallback);
    }
    int callbackError() const { return lastCallbackError().load(std::memory_order_relaxed); }
    const char* errorName(int error) const { return esp_err_to_name(error); }
    int sdaLevel() const { return gpio_get_level(static_cast<gpio_num_t>(PHOTOPAINTER_PMIC_SDA)); }
    int sclLevel() const { return gpio_get_level(static_cast<gpio_num_t>(PHOTOPAINTER_PMIC_SCL)); }

    // Read-only comparison on the SAME documented bus, only after PMIC failure.
    // V1 PCF85063 has address 0x51; register 0x00 is Control_1. No scan, RTC
    // configuration or SHTC3 wake command, and no PMIC writes on unknown ID.
    int readRtcControl() {
        uint8_t value;
        return readRegister(0x51, 0x00, &value, 1);
    }
    template<class Output>
    void reportConfig(Output& out) const {
        out.printf("[PhotoPainter I2C] native IDF port=0 config=%s install=%s filter7=%s\r\n",
                   config_error_ == kNotAttempted ? "NOT_ATTEMPTED" : esp_err_to_name(config_error_),
                   install_error_ == kNotAttempted ? "NOT_ATTEMPTED" : esp_err_to_name(install_error_),
                   filter_error_ == kNotAttempted ? "NOT_ATTEMPTED" : esp_err_to_name(filter_error_));
        photopainter_gpio::report(out, "before_setup", before_setup_);
        photopainter_gpio::report(out, "after_setup", after_setup_);
        photopainter_gpio::report(out, "at_report", photopainter_gpio::capture());
    }
private:
    bool installed_ = false;
    photopainter_gpio::Snapshot before_setup_, after_setup_;
    // NOT_ATTEMPTED is rendered explicitly, not falsely as successful setup.
    static constexpr int kNotAttempted = -2;
    int config_error_ = kNotAttempted;
    int install_error_ = kNotAttempted;
    int filter_error_ = kNotAttempted;
};
} // namespace photopainter_pmic_bus
