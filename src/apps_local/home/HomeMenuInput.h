#pragma once

#include <cstdint>

namespace homeui {
// Owned only by the main loop, including while the render task holds its lock.
class MenuInput {
 public:
  enum class Kind : uint8_t { Move, Confirm, Resume };
  struct Event {
    Kind kind = Kind::Move;
    int16_t steps = 0;
  };
  bool move(int direction, int rows) {
    if (rows <= 0) return true;
    if (count && events[(first + count - 1) % capacity].kind == Kind::Move) {
      auto& last = events[(first + count - 1) % capacity];
      last.steps = (last.steps + direction) % rows;
      return true;
    }
    return push({Kind::Move, int16_t(direction % rows)});
  }
  bool push(Event event) {
    if (count == capacity) return false;
    events[(first + count++) % capacity] = event;
    return true;
  }
  bool pop(Event& out) {
    if (!count) return false;
    out = events[first];
    first = (first + 1) % capacity;
    --count;
    return true;
  }
  void clear() { first = count = 0; }
  static int selection(int current, int bookCount, int rows, int steps) {
    if (rows <= 0) return bookCount;
    const int row = current >= bookCount && current < bookCount + rows ? current - bookCount : 0;
    return bookCount + ((row + steps) % rows + rows) % rows;
  }

 private:
  static constexpr unsigned capacity = 16;
  Event events[capacity]{};
  unsigned first = 0, count = 0;
};
}  // namespace homeui
