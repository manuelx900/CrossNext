// BatteryMonitor::loadDesignCapacity() against a model of the X3's BQ27220.
#include <array>
#include <cstdio>
#include <string>
#include <vector>

#include "../../src/BatteryMonitor.cpp"

namespace {

int checksRun = 0;
int checksFailed = 0;

#define CHECK(condition)                                               \
  do {                                                                 \
    ++checksRun;                                                       \
    if (!(condition)) {                                                \
      ++checksFailed;                                                  \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #condition); \
    }                                                                  \
  } while (0)

constexpr uint16_t TARGET = 650;
constexpr uint16_t DM_BASE = 0x9280;
constexpr uint16_t DM_FCC = 0x929D;
constexpr uint16_t DM_DC = 0x929F;

// Just enough of a BQ27220 to hold the load to TRM 6.1: the keys, CONFIG UPDATE, and
// Data Memory blocks that only take a write in CONFIG UPDATE with FULL ACCESS and
// the right checksum. Like the X3's gauge, it ignores keys sent less than 1.5 s
// apart, drops a second key sent more than 4 s after the first, and moves MACData()
// to the next block when MACDataSum() is read, and misses a block select sent right
// after a block write (seen once on an X3, 02/10/2026). FullChargeCapacity() copies
// Learned Full Charge Capacity at each reinit (TRM 1.1.10).
struct FakeGauge {
  uint8_t sec = 3;  // SEC[1:0]: 3 sealed, 2 unsealed, 1 full access
  bool cfg = false;
  int refuse = -1;  // index of the transaction the bus refuses
  int reinits = 0;
  uint16_t fcc = 3000;  // FullChargeCapacity()
  std::vector<std::string> log;
  std::array<uint8_t, 0x100> dm{};  // from DM_BASE
  uint16_t mac = 0;                 // selected block
  uint16_t staged = 0;              // MACData() written, not yet committed
  uint16_t lastKey = 0;
  unsigned long lastKeyAt = 0;
  unsigned long committedAt = 0;

  FakeGauge() {
    for (size_t i = 0; i < dm.size(); ++i) dm[i] = static_cast<uint8_t>(i * 37 + 11);
    set(DM_FCC, 3000);
    set(DM_DC, 3000);
  }
  void set(const uint16_t address, const uint16_t value) {
    dm.at(address - DM_BASE) = value >> 8;
    dm.at(address - DM_BASE + 1) = value & 0xFF;
  }
  uint16_t get(const uint16_t address) const { return dm.at(address - DM_BASE) << 8 | dm.at(address - DM_BASE + 1); }
  // MACDataSum() over the address and the block, with the first two bytes replaced.
  uint8_t sum(const uint8_t first, const uint8_t second) const {
    unsigned total = (mac & 0xFF) + (mac >> 8) + first + second;
    for (int i = 2; i < 32; ++i) total += dm.at(mac - DM_BASE + i);
    return static_cast<uint8_t>(255 - total);
  }
  bool keyFollows(const uint16_t first, const uint16_t second, const uint16_t word) const {
    const unsigned long gap = hostMillis - lastKeyAt;
    return word == second && lastKey == first && gap >= 1500 && gap < 4000;
  }

  bool write(const uint8_t reg, const uint16_t word) {
    if (!record("W" + hex(reg) + "=" + hex(word, 4))) return false;
    if (reg == 0x00) {
      if (sec == 3 && keyFollows(0x0414, 0x3672, word)) sec = 2;
      if (sec == 2 && keyFollows(0xFFFF, 0xFFFF, word)) sec = 1;
      if (word == 0x0090 && sec == 1) cfg = true;
      if (word == 0x0091) {
        ++reinits;
        fcc = get(DM_FCC);
      }
      if (word == 0x0091 || word == 0x0092) cfg = false;
      if (word == 0x0030) sec = 3;
      lastKey = word;
      lastKeyAt = hostMillis;
    }
    if (reg == 0x3E && hostMillis - committedAt >= 100) mac = word;
    if (reg == 0x40) staged = word;
    if (reg == 0x60 && cfg && sec == 1 && word == (0x2400 | sum(staged & 0xFF, staged >> 8))) {
      dm.at(mac - DM_BASE) = staged & 0xFF;
      dm.at(mac - DM_BASE + 1) = staged >> 8;
      committedAt = hostMillis;
    }
    return true;
  }

  bool read(const uint8_t reg, uint16_t& word) {
    if (!record("R" + hex(reg))) return false;
    if (reg == 0x3A) word = static_cast<uint16_t>((cfg ? 0x0400 : 0) | sec << 1);
    if (reg == 0x3C) word = get(DM_DC);
    if (reg == 0x12) word = fcc;
    if (reg == 0x40) word = static_cast<uint16_t>(dm.at(mac - DM_BASE) | dm.at(mac - DM_BASE + 1) << 8);
    if (reg == 0x60) {
      word = static_cast<uint16_t>(0x2400 | sum(dm.at(mac - DM_BASE), dm.at(mac - DM_BASE + 1)));
      mac += 32;
    }
    return true;
  }

  int writes() const {
    int count = 0;
    for (const auto& entry : log) count += entry[0] == 'W';
    return count;
  }

 private:
  static std::string hex(const unsigned value, const int digits = 2) {
    char text[8];
    std::snprintf(text, sizeof(text), "%0*X", digits, value);
    return text;
  }
  bool record(const std::string& entry) {
    const bool ok = static_cast<int>(log.size()) != refuse;
    log.push_back(entry + (ok ? "" : "!"));
    return ok;
  }
};

FakeGauge* gauge = nullptr;

// Calls the load every 10 ms, as a main loop would, like a fresh start.
void run(FakeGauge& fake, const BoardConfig::BoardProfile& board = BoardConfig::XTEINK_X3) {
  gauge = &fake;
  BoardConfig::ACTIVE = board;
  bq27220Load = {};
  for (unsigned long t = hostMillis + 10; t < hostMillis + 30000; t += 10) {
    hostMillis = t;
    const bool pending = BatteryMonitor::loadDesignCapacity();
    CHECK(hostMillis - t <= 45);  // one short step per call
    if (!pending) break;
  }
}

void testDefaultIsLoadedAndSealed() {
  CHECK(BoardConfig::XTEINK_X3.batteryGauge.designCapacityMah == TARGET);
  CHECK(BoardConfig::XTEINK_X3_UC8279.batteryGauge.designCapacityMah == TARGET);
  FakeGauge fake;
  const auto before = fake.dm;
  run(fake);
  CHECK(fake.get(DM_DC) == TARGET);
  CHECK(fake.get(DM_FCC) == TARGET);
  fake.set(DM_FCC, 3000);
  fake.set(DM_DC, 3000);
  CHECK(fake.dm == before);  // nothing else in the block changed
  CHECK(fake.reinits == 1);
  CHECK(!fake.cfg);
  CHECK(fake.sec == 3);
}

void testOtherCapacitiesAndBoardsAreLeftAlone() {
  FakeGauge other;
  other.set(DM_DC, 1200);
  run(other);
  CHECK(other.writes() == 0);
  CHECK(other.get(DM_DC) == 1200);
  // Loaded, with a capacity learned on the cell since: below it, or a little above.
  for (const uint16_t learned : {uint16_t{560}, TARGET, uint16_t{800}}) {
    FakeGauge fake;
    fake.set(DM_DC, TARGET);
    fake.set(DM_FCC, learned);
    fake.fcc = learned;
    run(fake);
    CHECK(fake.writes() == 0);
    CHECK(fake.get(DM_FCC) == learned);
  }
  auto noCapacity = BoardConfig::XTEINK_X3;
  noCapacity.batteryGauge.designCapacityMah = 0;
  for (const auto& board : {BoardConfig::XTEINK_X4, noCapacity}) {
    FakeGauge fake;
    run(fake, board);
    CHECK(fake.log.empty());
  }
}

// A Learned Full Charge Capacity of 2744 mAh, one learning cycle (at most 256 mAh
// down, TRM 1.1.3) from TI's 3000 default, kept FullChargeCapacity() at 2744 on an
// X3 whose Design Capacity read 650 (field report, 02/10/2026). It is replaced
// whether Design Capacity still reads 3000 or was loaded before; one learned on the
// cell below the target is kept.
void testCapacityLearnedAgainstTheDefaultIsReplaced() {
  for (const uint16_t dc : {uint16_t{3000}, TARGET}) {
    FakeGauge fake;
    fake.set(DM_DC, dc);
    fake.set(DM_FCC, 2744);
    fake.fcc = 2744;
    run(fake);
    CHECK(fake.get(DM_FCC) == TARGET);
    CHECK(fake.get(DM_DC) == TARGET);
    CHECK(fake.fcc == TARGET);
    CHECK(fake.reinits == 1);
    CHECK(!fake.cfg);
    CHECK(fake.sec == 3);
  }
  FakeGauge aged;
  aged.set(DM_FCC, 560);
  run(aged);
  CHECK(aged.get(DM_FCC) == 560);
  CHECK(aged.get(DM_DC) == TARGET);
}

// Every path ends out of CONFIG UPDATE and sealed, and the next start finishes the job,
// from TI's defaults and from the field report's 650 / 2744.
void testEveryRefusedTransactionEndsClosed() {
  for (const uint16_t dc : {uint16_t{3000}, TARGET}) {
    FakeGauge start;
    start.set(DM_DC, dc);
    if (dc == TARGET) {
      start.set(DM_FCC, 2744);
      start.fcc = 2744;
    }
    FakeGauge clean = start;
    run(clean);
    for (size_t refuse = 0; refuse < clean.log.size(); ++refuse) {
      FakeGauge fake = start;
      fake.refuse = static_cast<int>(refuse);
      run(fake);
      const std::string& refused = clean.log[refuse];
      const bool lostExit = refused == "W00=0091" || refused == "W00=0092";
      if (!lostExit) CHECK(!fake.cfg);
      if (refused != "W00=0030" && !lostExit) CHECK(fake.sec == 3);
      CHECK(fake.get(DM_DC) == dc || fake.get(DM_DC) == TARGET);

      fake.refuse = -1;
      fake.log.clear();
      run(fake);
      CHECK(fake.get(DM_DC) == TARGET);
      CHECK(fake.get(DM_FCC) == TARGET);
      CHECK(fake.fcc == TARGET);
      CHECK(!fake.cfg);
      CHECK(fake.sec == 3);
    }
  }
}

}  // namespace

bool hostI2cWrite(const uint8_t addr, const std::vector<uint8_t>& bytes) {
  return addr == 0x55 && bytes.size() == 3 && gauge->write(bytes[0], static_cast<uint16_t>(bytes[1] | bytes[2] << 8));
}

bool hostI2cRead(const uint8_t addr, const uint8_t reg, uint8_t* out, const size_t length) {
  uint16_t word = 0;
  if (addr != 0x55 || length != 2 || !gauge->read(reg, word)) return false;
  out[0] = word & 0xFF;
  out[1] = word >> 8;
  return true;
}

int main() {
  testDefaultIsLoadedAndSealed();
  testOtherCapacitiesAndBoardsAreLeftAlone();
  testCapacityLearnedAgainstTheDefaultIsReplaced();
  testEveryRefusedTransactionEndsClosed();

  std::printf("%d checks, %d failures\n", checksRun, checksFailed);
  return checksFailed == 0 ? 0 : 1;
}
