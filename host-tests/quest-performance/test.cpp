#include <FreeInkUIDisplayTarget.h>

#include <array>
#include <cassert>
#include <cstdio>
#include <vector>

#include "../../src/apps_local/quest/QuestAssets.h"
#include "../../src/apps_local/quest/QuestCampaign.h"
#include "../../src/apps_local/quest/QuestIcons.h"
#include "../../src/apps_local/quest/QuestPendingTouch.h"
#include "../../src/apps_local/quest/QuestRequests.h"
#include "../../src/apps_local/ui/SdBitmapPack.h"

namespace fui = freeink::ui;
void touches() {
  questinput::PendingTouch input;
  int x = -1, y = -1;
  input.capture(10, 20);
  assert(!input.take(x, y));
  input.painted();
  input.capture(10, 20);
  input.capture(80, 90);
  assert(input.take(x, y) && x == 10 && y == 20);
  assert(!input.take(x, y));
  input.capture(30, 40);
  input.invalidate();
  input.painted();
  assert(!input.take(x, y));
  input.invalidate();
  input.capture(50, 60);
  input.painted();
  assert(!input.take(x, y));
  input.capture(70, 80);
  assert(input.take(x, y) && x == 70 && y == 80);
  input.pressed();
  input.invalidate();
  input.painted();
  input.capture(1, 2);
  assert(!input.take(x, y));
  input.pressed();
  input.capture(1, 2);
  input.clear();
  assert(!input.take(x, y));
}
void spriteCache() {
  constexpr char path[] = "resources/apps/reading-adventure/graphics.bin";
  constexpr unsigned count = questassets::COUNT;
  sdart::Pack cached, reference;
  assert(reference.open(path, count));
  const auto a = reference.image(0), b = reference.image(1), c = reference.image(2);
  assert(a.bytes == b.bytes && b.bytes == c.bytes);
  assert(cached.open(path, count, a.bytes + b.bytes));
  const auto box = fui::makeRect(0, 0, 280, 320);
  int step = 0;
  for (unsigned id : {0u, 1u, 0u, 2u, 0u, 1u, 2u, 0u, 1u, 2u}) {
    std::vector<uint8_t> expected(35 * 320, 255), actual(expected);
    fui::DisplayTarget oldTarget(expected.data(), 280, 320, 35), newTarget(actual.data(), 280, 320, 35);
    reference.draw(oldTarget, box, id);
    sdartReadCalls = sdartReadBytes = sdartSeekCalls = 0;
    cached.draw(newTarget, box, id);
    assert(cached.ready() && expected == actual);
    if (step == 2 || step == 4) assert(sdartReadCalls == 0);
    ++step;
    if (id == 0) {
      // A second draw must be served from the retained sprite, including after compaction.
      sdartReadCalls = sdartReadBytes = sdartSeekCalls = 0;
      cached.draw(newTarget, box, id);
      assert(sdartReadCalls == 0 && sdartReadBytes == 0 && sdartSeekCalls == 0);
    }
  }
  cached.close();
  reference.close();
  sdartMemory::allocationLimit = count * 16 + 4096;
  assert(cached.open(path, count, 160 * 1024));
  assert(sdartMemory::liveBytes <= sdartMemory::allocationLimit);
  std::vector<uint8_t> pixels(35 * 320, 255);
  fui::DisplayTarget target(pixels.data(), 280, 320, 35);
  cached.draw(target, box, 0);
  assert(cached.ready());
  cached.close();
  assert(sdartMemory::liveBytes == 0);
  sdartMemory::allocationLimit = size_t(-1);
  assert(cached.open(path, count, 160 * 1024));
  sdartFailReadCall = sdartReadCalls + 1;
  cached.draw(target, box, 0);
  assert(!cached.ready() && sdartMemory::liveBytes == 0);
  sdartFailReadCall = 0;
  sdartMemory::available = false;
  assert(cached.open(path, count, 160 * 1024));
  cached.draw(target, box, 0);
  assert(cached.ready() && sdartMemory::liveBytes == 0);
  cached.close();
  sdartMemory::available = true;
}
void catalogs() {
  quest::Profile p;
  p.campaign.initialized = p.campaign.prologueSeen = true;
  p.campaign.introducedBooks = 12;
  for (unsigned chapter : {0u, 19u, 119u}) {
    p.campaign.encounter = quest::battleAnchor(chapter);
    for (unsigned level : {1u, 50u, 150u}) {
      p.xp = quest::threshold(level);
      for (int slot = 0; slot < quest::kCategoryCount; ++slot) {
        const auto category = quest::Slot(slot);
        const int count = quest::shopCount(p, category);
        for (int capacity : {3, 4})
          for (int page = 0; page <= count / capacity + 1; ++page) {
            uint16_t items[4];
            assert(quest::shopPage(p, category, page, capacity, items) == count);
            for (int n = 0; n < capacity; ++n) assert(items[n] == quest::shopItem(p, category, page * capacity + n));
          }
      }
      int ids[quest::MAX_AVAILABLE_HUNTS];
      const int count = quest::availableHunts(p, ids);
      assert(count == quest::requestCount(p, false));
      for (int n = 0; n < count; ++n) assert(ids[n] == quest::requestAt(p, false, n));
    }
  }
}
int main() {
  touches();
  spriteCache();
  catalogs();
  std::puts("PASS: generation-bound touches, sprite reuse/eviction/pixels/OOM/I-O failure and catalog equivalence");
}
