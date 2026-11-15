#include <cassert>
#include <chrono>
#include <cstdio>

#include "src/apps_local/readerbars/ReaderBarsJson.h"
#include "src/apps_local/readerbars/ReaderPace.h"
using namespace readerbars;
int main() {
  static_assert(sizeof(Pace) <= 40);
  static_assert(sizeof(Settings) <= 32);
  Pace pace;
  assert(pace.remaining(100000) == UNKNOWN_TIME);
  pace.painted(1000, 100000);
  pace.turn(true, true, 31000);
  pace.painted(32000, 110000);
  pace.turn(true, true, 62000);
  pace.painted(63000, 120000);
  assert(pace.remaining(120000) == 2640);
  assert(pace.remaining(120000, 200000) == 240);
  assert(pace.remaining(1000000) == 0);
  const auto estimate = pace.remaining(120000);
  Pace menuPace = pace;
  menuPace.pause();
  menuPace.painted(200000, 120000, false);  // page repainted below menu
  menuPace.painted(260000, 120000, true);   // menu dismissed; starts timing here
  menuPace.turn(true, true, 290000);
  menuPace.painted(291000, 130000);
  assert(menuPace.remaining(120000) == estimate);  // excludes the 60 s menu

  pace.pause();
  pace.painted(200000, 120000);
  pace.turn(true, true, 200100);
  pace.painted(200500, 130000);  // rapid skip
  assert(pace.remaining(120000) == estimate);
  pace.turn(true, true, 600000);
  pace.painted(601000, 140000);  // idle
  assert(pace.remaining(120000) == estimate);
  pace.turn(false, true);
  pace.painted(602000, 100000);  // backwards
  assert(pace.remaining(120000) == estimate);
  pace.turn(true, true, 632000);
  pace.painted(633000, 900000);  // large jump
  assert(pace.remaining(120000) == estimate);
  pace.reset();
  assert(pace.remaining(0) == UNKNOWN_TIME);
  pace.painted(UINT32_MAX - 20000, 100000);
  pace.turn(true, true, 9999);
  pace.painted(10999, 110000);
  pace.turn(true, true, 40999);
  pace.painted(41999, 120000);
  assert(pace.remaining(120000) == 2640);  // millis rollover
  Settings original;
  JsonDocument oldSettings;
  assert(readJson(oldSettings, original));
  assert(!original.customized);
  writeJson(oldSettings, original);
  assert(oldSettings["readerBars"].isNull());
  Settings settings;
  assert(!settings.needsPace());
  settings.top.slots[3] = Item::TimeChapter;
  settings.customized = true;
  assert(settings.needsPace());
  settings.top.visible = false;
  assert(!settings.needsPace());
  settings.top.visible = true;
  settings.bottom.slots[0] = Item::Battery;
  JsonDocument doc;
  writeJson(doc, settings);
  Settings loaded;
  assert(readJson(doc, loaded));
  assert(loaded.top.slots == settings.top.slots);
  doc["readerBars"]["bottom"]["slots"][6] = -1;
  assert(!readJson(doc, loaded));
  assert(loaded.top.slots == settings.top.slots);
  doc["readerBars"]["bottom"]["slots"][6] = 100;
  assert(!readJson(doc, loaded));
  doc["readerBars"]["version"] = 2;
  assert(!readJson(doc, loaded));
  JsonDocument absent;
  assert(readJson(absent, loaded));
  assert(loaded.customized);
  for (int width = 0; width <= 800; ++width) {
    std::array<int, SLOT_COUNT> widths = {150, 80, 40, 400, 160, 70, 120};
    fitWidths(widths, width, 8);
    int used = 0, count = 0;
    for (unsigned i = 0; i < SLOT_COUNT; ++i)
      if (i != 3 && widths[i]) {
        used += widths[i];
        ++count;
      }
    assert(used + std::max(0, count - 1) * 8 <= width);
  }
  const auto start = std::chrono::steady_clock::now();
  for (unsigned i = 0; i < 1000000; ++i) {
    pace.turn(true, true, i * 30000u);
    pace.painted(i * 30000u + 10, 100000 + (i % 80) * 10000);
  }
  std::printf("benchmark checksum: %u\n", pace.remaining(500000));
  const auto elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
  std::printf(
      "reader bars: pace, malformed JSON, hidden items, UTF-8 geometry, rollover passed; 1M pace updates: %lld ms; "
      "Pace=%zu Settings=%zu bytes\n",
      static_cast<long long>(elapsed), sizeof(Pace), sizeof(Settings));
}
