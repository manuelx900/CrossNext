#pragma once

#include <cstdint>

namespace tools {
// Main-loop-owned input while the render task holds the display lock.
class CounterInput {
 public:
  enum class Kind : uint8_t { Add, Touch, Back, FocusNext, FocusPrevious, Confirm };
  struct Event {
    Kind kind = Kind::Add;
    int16_t x = 0, y = 0;
    uint16_t repeats = 1;
  };
  bool push(Kind kind, int x = 0, int y = 0) {
    if (count) {
      auto& last = events[(first + count - 1) % capacity];
      if (last.kind == kind && last.x == x && last.y == y && last.repeats < UINT16_MAX) {
        ++last.repeats;
        return true;
      }
    }
    if (count == capacity) return false;
    events[(first + count++) % capacity] = {kind, int16_t(x), int16_t(y), 1};
    return true;
  }
  bool pop(Event& out) {
    if (!count) return false;
    out = events[first];
    out.repeats = 1;
    if (--events[first].repeats == 0) {
      first = (first + 1) % capacity;
      --count;
    }
    return true;
  }
  void clear() { first = count = 0; }

 private:
  static constexpr unsigned capacity = 32;
  Event events[capacity]{};
  unsigned first = 0, count = 0;
};
inline int toggleOrientation(int current, int initial) { return current == initial ? (initial + 1) % 4 : initial; }
}  // namespace tools
