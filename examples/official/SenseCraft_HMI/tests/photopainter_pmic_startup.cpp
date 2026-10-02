#include "boards/waveshare_photopainter/pmic_startup.h"
#include <cassert>
#include <string>
#include <cstdio>
#include <vector>

using namespace photopainter_pmic;
struct Bus {
    bool begin_ok = true;
    int sda = -1, scl = -1;
    unsigned clock = 0, timeout = 0, transactions = 0, requests = 0;
    uint8_t ack = 0, id_tx = 0;
    int id = kExpectedChipId;
    unsigned count = 1;
    bool present = true;
    uint8_t address = 0, reg = 255;
    bool register_write = false;
    unsigned transient_nacks = 0;
    bool begin(int a, int b, unsigned c) { sda = a; scl = b; clock = c; return begin_ok; }
    void setClock(unsigned c) { clock = c; }
    void setTimeOut(unsigned t) { timeout = t; }
    void beginTransmission(uint8_t a) { address = a; register_write = false; ++transactions; }
    void write(uint8_t r) { reg = r; register_write = true; }
    uint8_t endTransmission() {
        if (!register_write && transient_nacks) { --transient_nacks; return 2; }
        return register_write ? id_tx : ack;
    }
    unsigned requestFrom(uint8_t a, uint8_t n) { assert(a == address && n == 1); ++requests; return count; }
    int available() { return present; }
    int read() { return id; }
};
struct Pmic {
    bool ok = true;
    unsigned calls = 0;
    bool init(Bus& b, int sda, int scl, uint8_t addr) {
        ++calls;
        assert(b.sda == sda && b.scl == scl && addr == b.address);
        assert(b.id == kExpectedChipId);
        return ok;
    }
};
struct Output {
    std::string text;
    template<class... Args> void printf(const char* fmt, Args... args) {
        char buf[512]; std::snprintf(buf, sizeof(buf), fmt, args...); text += buf;
    }
    void println(const char* s) { text += s; text += '\n'; }
};

int main() {
    // Documented V1 pins, not DevKitC defaults or inverted IDF arguments.
    static_assert(PHOTOPAINTER_PMIC_SDA == 47 && PHOTOPAINTER_PMIC_SCL == 48, "V1 pins");
    static_assert(PHOTOPAINTER_PMIC_ADDRESS == 0x34, "V1 PMIC address");
    std::vector<unsigned> waits;
    auto pause = [&](unsigned ms) { waits.push_back(ms); };
    { Bus b; Pmic p; auto r = initialize(b, p, pause);
      assert(r.ready && r.bus_ready && r.count == 1 && p.calls == 1);
      assert(b.sda == 47 && b.scl == 48 && b.clock == 100000 && b.timeout == 50);
      assert(b.reg == 0x03 && b.transactions == 2 && b.requests == 1);
      assert(waits.empty()); Output out; report(out, r);
      assert(out.text.find("chip_id=74") != std::string::npos);
      assert(out.text.find("UNCONFIRMED") == std::string::npos); }
    { Bus b; b.begin_ok = false; Pmic p; auto r = initialize(b, p, pause);
      assert(!r.ready && !r.bus_ready && r.count == 1 && b.transactions == 0 && p.calls == 0);
      assert(r.attempts[0].failure == Failure::bus_begin); }
    { Bus b; b.ack = 2; Pmic p; waits.clear(); auto r = initialize(b, p, pause);
      assert(!r.ready && r.count == 3 && b.requests == 0 && p.calls == 0);
      assert(r.attempts[2].failure == Failure::address_ack);
      assert(waits.size() == 2 && waits[0] == 20 && waits[1] == 20);
      Output out; report(out, r); assert(out.text.find("PMIC_NO_ACK") != std::string::npos);
      assert(out.text.find("EPD_VCC off is UNCONFIRMED") != std::string::npos); }
    { Bus b; b.id_tx = 3; Pmic p; auto r = initialize(b, p, pause);
      assert(!r.ready && b.requests == 0 && p.calls == 0);
      assert(r.attempts[0].id_status == 3 && r.attempts[0].failure == Failure::id_read); }
    { Bus b; b.count = 0; Pmic p; auto r = initialize(b, p, pause);
      assert(!r.ready && p.calls == 0 && r.attempts[0].chip_id == -1);
      assert(r.attempts[0].failure == Failure::id_read); }
    { Bus b; b.present = false; Pmic p; auto r = initialize(b, p, pause);
      assert(!r.ready && p.calls == 0 && r.attempts[0].failure == Failure::id_read); }
    for (int id : {0, 0x47, 0x49, 0xFF}) {
      Bus b; b.id = id; Pmic p; auto r = initialize(b, p, pause);
      assert(!r.ready && p.calls == 0 && r.attempts[0].failure == Failure::chip_id);
    }
    { Bus b; Pmic p; p.ok = false; auto r = initialize(b, p, pause);
      assert(!r.ready && p.calls == 3 && r.attempts[0].failure == Failure::library_init); }
    { Bus b; b.transient_nacks = 2; Pmic p; auto r = initialize(b, p, pause);
      assert(r.ready && r.count == 3 && p.calls == 1);
      assert(r.attempts[0].ack == 2 && r.attempts[2].chip_id == 74); }
    std::puts("PhotoPainter PMIC startup: bus/ACK/read/ID/library failure and bounded recovery passed");
}
