#include "boards/waveshare_photopainter/pmic_startup.h"
#include <cassert>
#include <string>
#include <cstdio>
#include <vector>

using namespace photopainter_pmic;
struct Pmic { unsigned calls = 0; bool ok = true; };
struct Bus {
    bool begin_ok = true;
    int sda = -1, scl = -1;
    unsigned clock = 0, reads = 0, peers = 0;
    int error = 0, id = kExpectedChipId;
    int sda_idle = 1, scl_idle = 1;
    unsigned transient_timeouts = 0;
    bool begin(int a, int b, unsigned c) { sda = a; scl = b; clock = c; return begin_ok; }
    int readId(int& result) {
        ++reads;
        int e = error;
        if (transient_timeouts) { --transient_timeouts; e = 263; }
        result = e == 0 ? id : -1;
        return e;
    }
    bool bind(Pmic& p) { ++p.calls; assert(error == 0 && id == kExpectedChipId); return p.ok; }
    int callbackError() { return 0; }
    const char* errorName(int e) { return e == 0 ? "ESP_OK" : (e == 263 ? "ESP_ERR_TIMEOUT" : "ESP_FAIL"); }
    int sdaLevel() { return sda_idle; }
    int sclLevel() { return scl_idle; }
    int readRtcControl() { ++peers; return error; }
};
struct Output {
    std::string text;
    template<class... Args> void printf(const char* fmt, Args... args) {
        char buf[512]; std::snprintf(buf, sizeof(buf), fmt, args...); text += buf;
    }
    void println(const char* s) { text += s; text += "\r\n"; }
};
int main() {
    static_assert(PHOTOPAINTER_PMIC_SDA == 47 && PHOTOPAINTER_PMIC_SCL == 48, "V1 pins");
    static_assert(PHOTOPAINTER_PMIC_ADDRESS == 0x34, "V1 PMIC address");
    std::vector<unsigned> waits;
    auto pause = [&](unsigned ms) { waits.push_back(ms); };
    { Bus b; Pmic p; auto r = initialize(b, p, pause);
      assert(r.ready && r.bus_ready && r.count == 1 && p.calls == 1);
      assert(b.sda == 47 && b.scl == 48 && b.clock == 100000);
      assert(b.reads == 1 && b.peers == 0 && waits.empty());
      Output out; report(out, r); assert(out.text.find("chip_id=74") != std::string::npos);
      assert(out.text.find("UNCONFIRMED") == std::string::npos);
      assert(out.text.find("native-repeated-start") != std::string::npos);
      assert(out.text.find("idle_before SDA=1 SCL=1") != std::string::npos); }
    { Bus b; b.begin_ok = false; Pmic p; auto r = initialize(b, p, pause);
      assert(!r.ready && !r.bus_ready && r.count == 1 && b.reads == 0 && p.calls == 0);
      assert(!r.rtc_checked && r.sda_before == -1 && r.attempts[0].failure == Failure::bus_begin); }
    for (int err : {-1, 263}) {
      Bus b; b.error = err; b.sda_idle = 0; Pmic p; waits.clear(); auto r = initialize(b, p, pause);
      assert(!r.ready && r.count == 3 && b.reads == 3 && p.calls == 0 && b.peers == 1);
      assert(r.attempts[2].failure == Failure::id_read && r.attempts[2].chip_id == -1);
      assert(r.sda_before == 0 && r.sda_after == 0);
      assert(waits.size() == 2 && waits[0] == 20 && waits[1] == 20);
      Output out; report(out, r); assert(out.text.find("EPD_VCC off is UNCONFIRMED") != std::string::npos);
      assert(out.text.find(err == 263 ? "ESP_ERR_TIMEOUT" : "ESP_FAIL") != std::string::npos);
    }
    for (int id : {0, 0x47, 0x49, 0xFF}) {
      Bus b; b.id = id; Pmic p; auto r = initialize(b, p, pause);
      assert(!r.ready && p.calls == 0 && r.attempts[0].failure == Failure::chip_id);
    }
    { Bus b; Pmic p; p.ok = false; auto r = initialize(b, p, pause);
      assert(!r.ready && p.calls == 3 && r.attempts[0].failure == Failure::library_init); }
    { Bus b; b.transient_timeouts = 2; Pmic p; auto r = initialize(b, p, pause);
      assert(r.ready && r.count == 3 && p.calls == 1 && !r.rtc_checked);
      assert(r.attempts[0].read_error == 263 && r.attempts[2].chip_id == 74); }
    std::puts("PhotoPainter PMIC native startup: read errors, strict ID, bounded retries, bus levels and RTC peer policy passed");
}
