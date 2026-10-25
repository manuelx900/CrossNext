#include <cassert>
#include <iostream>
#include <vector>

#include "driver/Uc8279cA4Driver.h"

int main() {
  using namespace freeink;
  constexpr size_t bytes = 768 / 8 * 552;
  std::vector<uint8_t> bw(bytes, 0xff), gray(bytes, 0xaa);
  EpdBus bus;
  auto& driver = uc8279cA4Driver();
  driver.begin(bus);

  // A real reader AA turn: base -> gray masks -> restore base -> different page.
  // The target differs from both the old base and the gray selector planes.
  for (size_t i = 0; i < bytes; ++i) bw[i] = static_cast<uint8_t>(i * 37);
  driver.display(bus, bw.data(), nullptr, RefreshMode::Full, false);
  driver.copyGrayscaleLsb(bus, gray.data());
  driver.copyGrayscaleMsb(bus, gray.data());
  driver.displayGray(bus, bw.data(), false, nullptr, false);
  driver.cleanupGrayscaleBuffers(bus, bw.data());
  for (auto& b : bw) b = static_cast<uint8_t>(~b);
  driver.display(bus, bw.data(), nullptr, RefreshMode::Fast, false);
  assert(bus.interval == 0xd7 && bus.lutBytes == 42);
  // X4's post-AA FAST path drives every target pixel, including old text to white.
  for (size_t i = 0; i < bytes; ++i) {
    const auto physical = (551 - i / 96) * 96 + i % 96;
    assert(bus.newAtRefresh[physical] == bw[i]);
    assert(bus.oldAtRefresh[physical] == static_cast<uint8_t>(~bw[i]) &&
           "post-AA FAST leaves white target pixels without a clearing transition");
  }
  assert(bus.oldPlane == bus.newPlane && "displayed B/W frame was not retained as the next baseline");
  // The next ordinary FAST paint must diff against the displayed frame.
  const auto previous = bus.newAtRefresh;
  for (auto& b : bw) b = static_cast<uint8_t>(b ^ 0x5a);
  driver.display(bus, bw.data(), nullptr, RefreshMode::Fast, true);
  assert(bus.oldAtRefresh == previous && "ordinary FAST diff lost its previous displayed frame");
  assert(bus.oldPlane == bus.newPlane);
  driver.begin(bus);

  // Unknown glass after boot still gets a full first paint.
  driver.display(bus, bw.data(), nullptr, RefreshMode::Fast, false);
  assert(bus.interval == 0x97 && bus.lutBytes == 49);

  // Menu moves do not acquire an independent periodic-full cadence.
  for (int move = 0; move < 20; ++move) {
    driver.display(bus, bw.data(), nullptr, RefreshMode::Fast, false);
    assert(bus.interval == 0xd7 && bus.lutBytes == 42 && "menu FAST request was promoted to FULL");
  }

  // Each AA page may be followed by a fast B/W base, including after cleanup.
  for (bool cleanup : {false, true}) {
    for (int page = 0; page < 3; ++page) {
      driver.copyGrayscaleLsb(bus, gray.data());
      driver.copyGrayscaleMsb(bus, gray.data());
      const auto before = bus.refreshes;
      driver.displayGray(bus, bw.data(), false, nullptr, false);
      assert(bus.refreshes == before + 1 && bus.lutBytes == 49);
      if (cleanup) driver.cleanupGrayscaleBuffers(bus, bw.data());
      for (auto& b : bw) b = static_cast<uint8_t>(b ^ 0x33);
      driver.display(bus, bw.data(), nullptr, RefreshMode::Fast, page == 2);
      assert(bus.interval == 0xd7 && bus.lutBytes == 42 && "post-AA FAST request was promoted to FULL");
      for (size_t i = 0; i < bytes; ++i) {
        const auto physical = (551 - i / 96) * 96 + i % 96;
        assert(bus.newAtRefresh[physical] == bw[i]);
        assert(bus.oldAtRefresh[physical] == static_cast<uint8_t>(~bw[i]));
      }
      assert(bus.oldPlane == bus.newPlane);
    }
  }

  // The caller still controls scheduled/manual cleanup.
  for (auto mode : {RefreshMode::Half, RefreshMode::Full}) {
    driver.display(bus, bw.data(), nullptr, mode, false);
    assert(bus.interval == 0x97 && bus.lutBytes == 49);
    driver.display(bus, bw.data(), nullptr, RefreshMode::Fast, false);
    assert(bus.interval == 0xd7 && bus.lutBytes == 42);
  }

  driver.deepSleep(bus);
  driver.begin(bus);
  driver.display(bus, bw.data(), nullptr, RefreshMode::Fast, false);
  assert(bus.interval == 0x97 && bus.lutBytes == 49);
  driver.display(bus, bw.data(), nullptr, RefreshMode::Fast, false);
  assert(bus.interval == 0xd7 && bus.lutBytes == 42);
  std::cout << "PASS: A4 FAST cadence, post-AA pixel transitions, retained baseline, explicit cleanup and wake\n";
}
