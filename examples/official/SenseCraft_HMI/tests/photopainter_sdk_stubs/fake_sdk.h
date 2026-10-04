#pragma once
// Minimal HOST mock declarations, not an SDK replacement for device builds.
#include <cstdint>
#include <cstddef>
using esp_err_t = int;
using i2c_port_t = int;
using gpio_num_t = int;
using TickType_t = uint32_t;
using i2c_mode_t = int;
constexpr int ESP_OK = 0, ESP_FAIL = -1, ESP_ERR_INVALID_ARG = 0x102,
              ESP_ERR_INVALID_STATE = 0x103, ESP_ERR_TIMEOUT = 0x107;
constexpr int I2C_NUM_0 = 0, I2C_MODE_MASTER = 1, GPIO_PULLUP_ENABLE = 1;
struct i2c_config_t {
    int mode, sda_io_num, scl_io_num, sda_pullup_en, scl_pullup_en;
    struct { unsigned clk_speed; } master;
    unsigned clk_flags;
};
inline TickType_t pdMS_TO_TICKS(unsigned ms) { return ms; }
void vTaskDelay(TickType_t ticks);
int gpio_get_level(gpio_num_t gpio);
const char* esp_err_to_name(int error);
esp_err_t i2c_param_config(i2c_port_t, const i2c_config_t*);
esp_err_t i2c_driver_install(i2c_port_t, i2c_mode_t, size_t, size_t, int);
esp_err_t i2c_driver_delete(i2c_port_t);
esp_err_t i2c_filter_enable(i2c_port_t, uint8_t);
esp_err_t i2c_master_write_read_device(i2c_port_t, uint8_t, const uint8_t*, size_t,
                                      uint8_t*, size_t, TickType_t);
esp_err_t i2c_master_write_to_device(i2c_port_t, uint8_t, const uint8_t*, size_t, TickType_t);

// Read-only GPIO snapshot fixture. Production builds use the S3 SDK headers.
uint32_t fake_register_read(uint32_t address);
#define REG_READ(address) fake_register_read(address)
constexpr uint32_t GPIO_IN1_REG = 0, GPIO_ENABLE1_REG = 4, GPIO_OUT1_REG = 8,
    PIN_CTRL = 12, IO_MUX_GPIO47_REG = 16, GPIO_PIN47_REG = 20,
    GPIO_FUNC47_OUT_SEL_CFG_REG = 24, IO_MUX_GPIO48_REG = 28,
    GPIO_PIN48_REG = 32, GPIO_FUNC48_OUT_SEL_CFG_REG = 36,
    GPIO_FUNC0_IN_SEL_CFG_REG = 1000;
constexpr unsigned I2CEXT0_SDA_IN_IDX = 90, I2CEXT0_SCL_IN_IDX = 89;
constexpr uint32_t FUN_IE = 1 << 9, FUN_PU = 1 << 8, FUN_PD = 1 << 7,
    GPIO_PIN47_PAD_DRIVER = 1 << 2, MCU_SEL_M = 7 << 12, MCU_SEL_S = 12;
