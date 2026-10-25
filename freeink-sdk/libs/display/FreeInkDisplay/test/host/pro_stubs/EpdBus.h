#pragma once
#include <cassert>
#include <cstdint>
#include <vector>
namespace freeink {
enum class BusyPolarity { ActiveHigh, ActiveLow, X3TwoPhase, UcIdleHigh };
struct EpdPins { int8_t sclk, mosi, cs, dc, rst, busy, powerEnable=-1; };
class EpdBus {
 public:
  struct Write { uint8_t command; std::vector<uint8_t> bytes; unsigned transactions=0; };
  std::vector<Write> writes;
  bool transaction=false;
  unsigned waits=0;
  void clear() { assert(!transaction); writes.clear(); waits=0; }
  void begin(const EpdPins&, uint32_t, BusyPolarity, int8_t=-1, int8_t=-1) {}
  void cmd(uint8_t c) { assert(!transaction); writes.push_back({c,{}}); }
  void data(uint8_t b) { data(&b,1); }
  void data(const uint8_t* p, uint16_t n) {
    assert(!transaction); ++writes.back().transactions;
    writes.back().bytes.insert(writes.back().bytes.end(),p,p+n);
  }
  void beginTxn() { assert(!transaction); transaction=true; ++writes.back().transactions; }
  void endTxn() { assert(transaction); transaction=false; }
  void rawWriteBytes(const uint8_t* p, uint16_t n) {
    assert(transaction); writes.back().bytes.insert(writes.back().bytes.end(),p,p+n);
  }
  void fillPlane(uint8_t c, uint8_t v, uint16_t h, uint16_t wb) {
    cmd(c); beginTxn(); writes.back().bytes.assign(size_t(h)*wb,v); endTxn();
  }
  void cmdData2(uint8_t c, uint8_t a, uint8_t b) { cmd(c); data(a); data(b); }
  void cmdData(uint8_t c, const uint8_t* p, uint16_t n) { cmd(c); data(p,n); }
  void reset(uint16_t=0) {}
  void waitBusy(const char* =nullptr) { assert(!transaction); ++waits; }
  void waitRefreshComplete(const char* =nullptr) { assert(!transaction); ++waits; }
  bool isBusy() const { return false; }
  EpdPins pins() const { return {}; }
  void setBusyWaitHooks(void (*)(), void (*)()) {}
  void setBusyWaitSliceHook(bool (*)(int8_t,uint8_t)) {}
};
}
