#pragma once

#include <atomic>
#include <cstdint>

namespace questinput {
// Main-loop-owned coordinates carry the generation last presented by the render worker.
class PendingTouch {
 public:
  void pressed() {
    contact = true;
    pressedGeneration = shown.load(std::memory_order_acquire);
  }
  void capture(int x, int y) {
    const auto releasedGeneration = contact ? pressedGeneration : shown.load(std::memory_order_acquire);
    contact = false;
    if (pending) return;
    pointX = x;
    pointY = y;
    captured = releasedGeneration;
    pending = true;
  }
  bool hasPending() const { return pending; }
  void clear() { pending = false; }
  void invalidate() { generation.fetch_add(1, std::memory_order_acq_rel); }
  void painted() { shown.store(generation.load(std::memory_order_acquire), std::memory_order_release); }
  bool take(int& x, int& y) {
    if (!pending) return false;
    clear();
    if (captured != generation.load(std::memory_order_acquire) || shown.load(std::memory_order_acquire) != captured)
      return false;
    x = pointX;
    y = pointY;
    return true;
  }

 private:
  std::atomic<uint32_t> shown{0};
  std::atomic<uint32_t> generation{1};
  uint32_t captured = 0, pressedGeneration = 0;
  int pointX = 0, pointY = 0;
  bool pending = false, contact = false;
};
}  // namespace questinput
