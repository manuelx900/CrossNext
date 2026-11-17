#pragma once

#include <array>
#include <cstdint>

// Main-loop-owned intents; no page objects or radio callbacks are retained.
class PendingPageTurns {
 public:
  static constexpr uint8_t CAPACITY = 8;
  bool push(bool forward) {
    if (count == CAPACITY) return false;
    turns[(first + count++) % CAPACITY] = forward;
    return true;
  }
  bool empty() const { return count == 0; }
  bool pop() {
    const bool forward = turns[first];
    first = (first + 1) % CAPACITY;
    --count;
    return forward;
  }
  void clear() { first = count = 0; }

 private:
  std::array<bool, CAPACITY> turns{};
  uint8_t first = 0, count = 0;
};
