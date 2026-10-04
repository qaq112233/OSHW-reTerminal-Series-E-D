#pragma once

#include <stdint.h>
#include <soc/soc.h>
#include <soc/gpio_reg.h>
#include <soc/gpio_sig_map.h>
#include <soc/io_mux_reg.h>
#include "boards/waveshare_photopainter/config.h"

// ESP32-S3 V1 bus evidence only. No GPIO reconfiguration, recovery clocks,
// pad-voltage selection, eFuse access or PMIC writes are performed here.
namespace photopainter_gpio {
static_assert(PHOTOPAINTER_PMIC_SDA == 47 && PHOTOPAINTER_PMIC_SCL == 48,
              "Update the S3 register snapshot if the board pins change");

struct Pin {
    uint32_t mux, config, output_select, input_select;
};
struct Snapshot {
    uint32_t input1 = 0, enable1 = 0, output1 = 0, pad_control = 0;
    Pin sda{}, scl{};
    bool captured = false;
};

inline Snapshot capture() {
    Snapshot s;
    s.input1 = REG_READ(GPIO_IN1_REG);
    s.enable1 = REG_READ(GPIO_ENABLE1_REG);
    s.output1 = REG_READ(GPIO_OUT1_REG);
    s.pad_control = REG_READ(PIN_CTRL);
    s.sda = {REG_READ(IO_MUX_GPIO47_REG), REG_READ(GPIO_PIN47_REG),
             REG_READ(GPIO_FUNC47_OUT_SEL_CFG_REG),
             REG_READ(GPIO_FUNC0_IN_SEL_CFG_REG + I2CEXT0_SDA_IN_IDX * 4)};
    s.scl = {REG_READ(IO_MUX_GPIO48_REG), REG_READ(GPIO_PIN48_REG),
             REG_READ(GPIO_FUNC48_OUT_SEL_CFG_REG),
             REG_READ(GPIO_FUNC0_IN_SEL_CFG_REG + I2CEXT0_SCL_IN_IDX * 4)};
    s.captured = true;
    return s;
}

template<class Output>
void reportPin(Output& out, const char* phase, unsigned gpio, const Pin& pin,
               const Snapshot& s) {
    // GPIO_IN1 bit 15/16 is GPIO47/48, not bit 47/48 of a 32-bit word.
    out.printf("[PhotoPainter GPIO] %s gpio=%u level=%u IE=%u PU=%u PD=%u OD=%u FUNC=%u mux=0x%08lX pin=0x%08lX outsel=0x%08lX insel=0x%08lX\r\n",
               phase, gpio, unsigned((s.input1 >> (gpio - 32)) & 1),
               unsigned(bool(pin.mux & FUN_IE)), unsigned(bool(pin.mux & FUN_PU)),
               unsigned(bool(pin.mux & FUN_PD)), unsigned(bool(pin.config & GPIO_PIN47_PAD_DRIVER)),
               unsigned((pin.mux & MCU_SEL_M) >> MCU_SEL_S),
               (unsigned long)pin.mux, (unsigned long)pin.config,
               (unsigned long)pin.output_select, (unsigned long)pin.input_select);
}

template<class Output>
void report(Output& out, const char* phase, const Snapshot& s) {
    if (!s.captured) {
        out.printf("[PhotoPainter GPIO] %s NOT_CAPTURED\r\n", phase);
        return;
    }
    out.printf("[PhotoPainter GPIO] %s in1=0x%08lX enable1=0x%08lX out1=0x%08lX pad_ctrl=0x%08lX (read-only snapshot)\r\n",
               phase, (unsigned long)s.input1, (unsigned long)s.enable1,
               (unsigned long)s.output1, (unsigned long)s.pad_control);
    reportPin(out, phase, 47, s.sda, s);
    reportPin(out, phase, 48, s.scl, s);
}
} // namespace photopainter_gpio
