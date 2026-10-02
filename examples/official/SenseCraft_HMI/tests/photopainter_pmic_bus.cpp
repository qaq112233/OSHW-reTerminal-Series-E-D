#include "boards/waveshare_photopainter/pmic_bus.h"
#include "boards/waveshare_photopainter/pmic_startup.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

struct Driver {
    int config_error = ESP_OK, install_error = ESP_OK, filter_error = ESP_OK;
    int read_error = ESP_OK, write_error = ESP_OK;
    unsigned configs = 0, installs = 0, filters = 0, deletes = 0, reads = 0, writes = 0;
    unsigned transient_reads = 0, transient_writes = 0;
    int sda = 1, scl = 1;
    uint8_t address = 0, reg = 0, id = 0x4A;
    i2c_config_t config{};
    std::vector<uint8_t> payload;
    std::vector<unsigned> waits;
} d;

void vTaskDelay(TickType_t ticks) { d.waits.push_back(ticks); }
int gpio_get_level(gpio_num_t gpio) { assert(gpio == 47 || gpio == 48); return gpio == 47 ? d.sda : d.scl; }
const char* esp_err_to_name(int error) {
    switch (error) {
    case ESP_OK: return "ESP_OK";
    case ESP_FAIL: return "ESP_FAIL";
    case ESP_ERR_TIMEOUT: return "ESP_ERR_TIMEOUT";
    case ESP_ERR_INVALID_ARG: return "ESP_ERR_INVALID_ARG";
    case ESP_ERR_INVALID_STATE: return "ESP_ERR_INVALID_STATE";
    default: return "OTHER";
    }
}
esp_err_t i2c_param_config(i2c_port_t port, const i2c_config_t* config) {
    assert(port == 0); ++d.configs; d.config = *config; return d.config_error;
}
esp_err_t i2c_driver_install(i2c_port_t port, i2c_mode_t mode, size_t rx, size_t tx, int flags) {
    assert(port == 0 && mode == I2C_MODE_MASTER && rx == 0 && tx == 0 && flags == 0);
    ++d.installs; return d.install_error;
}
esp_err_t i2c_driver_delete(i2c_port_t port) { assert(port == 0); ++d.deletes; return ESP_OK; }
esp_err_t i2c_filter_enable(i2c_port_t port, uint8_t cycles) {
    assert(port == 0 && cycles == 7); ++d.filters; return d.filter_error;
}
esp_err_t i2c_master_write_read_device(i2c_port_t port, uint8_t address,
                                      const uint8_t* tx, size_t tx_len, uint8_t* rx,
                                      size_t rx_len, TickType_t timeout) {
    assert(port == 0 && tx_len == 1 && rx_len > 0 && tx && rx && timeout == 50);
    ++d.reads; d.address = address; d.reg = tx[0];
    if (d.transient_reads) { --d.transient_reads; return ESP_ERR_TIMEOUT; }
    if (d.read_error == ESP_OK) for (unsigned i = 0; i < rx_len; ++i) rx[i] = d.id;
    return d.read_error;
}
esp_err_t i2c_master_write_to_device(i2c_port_t port, uint8_t address,
                                    const uint8_t* tx, size_t len, TickType_t timeout) {
    assert(port == 0 && address == 0x34 && tx && len >= 2 && len <= 256 && timeout == 50);
    ++d.writes; d.payload.assign(tx, tx + len);
    if (d.transient_writes) { --d.transient_writes; return ESP_ERR_TIMEOUT; }
    return d.write_error;
}
struct Pmic {
    using Callback = int(*)(uint8_t, uint8_t, uint8_t*, uint8_t);
    unsigned calls = 0;
    bool begin(uint8_t address, Callback read, Callback write) {
        ++calls;
        assert(address == 0x34);
        assert(read == photopainter_pmic_bus::readCallback && write == photopainter_pmic_bus::writeCallback);
        uint8_t id = 0;
        return read(address, 0x03, &id, 1) == 0 && id == 0x4A;
    }
};
struct Output {
    std::string text;
    template<class... Args> void printf(const char* fmt, Args... args) {
        char buf[256]; std::snprintf(buf, sizeof(buf), fmt, args...); text += buf;
    }
};
int main() {
    using namespace photopainter_pmic_bus;
    { d = {}; NativeBus b; assert(b.begin(47,48,100000));
      assert(d.config.sda_io_num == 47 && d.config.scl_io_num == 48);
      assert(d.config.master.clk_speed == 100000 && d.config.sda_pullup_en && d.config.scl_pullup_en);
      assert(b.begin(47,48,100000) && d.installs == 1);
      int id = -1; assert(b.readId(id) == 0 && id == 74 && d.reg == 3 && d.address == 0x34);
      assert(d.writes == 0); Pmic p; assert(b.bind(p));
      assert(b.readRtcControl() == 0 && d.reg == 0 && d.address == 0x51 && d.writes == 0);
      d.sda = 0; assert(b.sdaLevel() == 0 && b.sclLevel() == 1); }
    { d = {}; d.config_error = ESP_FAIL; NativeBus b;
      assert(!b.begin(47,48,100000) && d.installs == 0 && d.filters == 0);
      Output o; b.reportConfig(o); assert(o.text.find("config=ESP_FAIL install=NOT_ATTEMPTED") != std::string::npos); }
    { d = {}; d.install_error = ESP_ERR_INVALID_STATE; NativeBus b;
      assert(!b.begin(47,48,100000) && d.filters == 0 && d.deletes == 0); }
    { d = {}; d.filter_error = ESP_FAIL; NativeBus b;
      assert(!b.begin(47,48,100000) && d.deletes == 1); }
    { d = {}; d.read_error = ESP_ERR_TIMEOUT; NativeBus b; int id = 74;
      assert(b.readId(id) == ESP_ERR_TIMEOUT && id == -1);
      uint8_t data = 0; d.reads = 0;
      assert(readCallback(0x34,3,&data,1) == -1 && d.reads == 3);
      assert(d.waits == std::vector<unsigned>({100,100}));
      assert(b.callbackError() == ESP_ERR_TIMEOUT); }
    { d = {}; d.transient_reads = 2; uint8_t data = 0;
      assert(readCallback(0x34,3,&data,1) == 0 && data == 74 && d.reads == 3); }
    { d = {}; uint8_t data[255]; for (unsigned i = 0; i < 255; ++i) data[i] = i;
      assert(writeCallback(0x34,0x90,data,255) == 0);
      assert(d.payload.size() == 256 && d.payload[0] == 0x90 && d.payload[255] == 254); }
    { d = {}; d.write_error = ESP_FAIL; uint8_t data = 0;
      assert(writeCallback(0x34,0x90,&data,1) == -1 && d.writes == 3);
      assert(d.waits == std::vector<unsigned>({100,100})); }
    { d = {}; uint8_t data = 0;
      assert(writeCallback(0x51,0,&data,1) == -1 && d.writes == 0);
      assert(readCallback(0x51,0,&data,1) == -1 && d.reads == 0);
      assert(writeCallback(0x34,0,nullptr,1) == -1 && d.writes == 0);
      assert(readCallback(0x34,0,&data,0) == -1 && d.reads == 0); }
    { d = {}; NativeBus b; Pmic p;
      auto result = photopainter_pmic::initialize(b, p, [](unsigned ms) { vTaskDelay(ms); });
      assert(result.ready && p.calls == 1 && d.reads == 2 && d.writes == 0); }
    { d = {}; d.id = 0x47; NativeBus b; Pmic p;
      auto result = photopainter_pmic::initialize(b, p, [](unsigned ms) { vTaskDelay(ms); });
      assert(!result.ready && p.calls == 0 && d.writes == 0 && d.reads == 4);
      assert(result.rtc_checked && result.attempts[0].failure == photopainter_pmic::Failure::chip_id); }
    { d = {}; d.read_error = ESP_ERR_TIMEOUT; NativeBus b; Pmic p;
      auto result = photopainter_pmic::initialize(b, p, [](unsigned ms) { vTaskDelay(ms); });
      assert(!result.ready && p.calls == 0 && d.writes == 0 && d.reads == 4);
      assert(result.sda_before == 1 && result.attempts[0].read_error == ESP_ERR_TIMEOUT); }
    std::puts("PhotoPainter native bus adapter: combined reads, prefixed writes, retries, setup errors and read-only RTC check passed (mock SDK)");
}
