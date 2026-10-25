// Rules adapted from Gabriele Cirulli's 2048 (MIT); see LICENSE-2048.txt.
#include "Game2048Core.h"

#include <algorithm>
#include <cstring>

namespace game2048 {
uint32_t random(State& state) {
  uint32_t v = state.random ? state.random : 1;
  v ^= v << 13;
  v ^= v >> 17;
  v ^= v << 5;
  state.random = v;
  return v;
}
uint8_t highest(const State& state) {
  uint8_t result = 0;
  for (auto cell : state.cells) result = std::max(result, cell);
  return result;
}
bool spawn(State& state) {
  uint8_t free[16];
  int count = 0;
  for (int n = 0; n < 16; ++n)
    if (!state.cells[n]) free[count++] = static_cast<uint8_t>(n);
  if (!count) return false;
  const int index = free[random(state) % count];
  state.cells[index] = random(state) % 10 == 0 ? 2 : 1;
  return true;
}
void start(State& state, uint32_t seed) {
  state = {};
  state.random = seed ? seed : 1;
  spawn(state);
  spawn(state);
}
bool canMove(const State& state) {
  for (int n = 0; n < 16; ++n) {
    if (!state.cells[n]) return true;
    if (state.cells[n] < 31 &&
        ((n % 4 < 3 && state.cells[n] == state.cells[n + 1]) || (n < 12 && state.cells[n] == state.cells[n + 4])))
      return true;
  }
  return false;
}
namespace {
int index(Direction direction, int line, int at) {
  switch (direction) {
    case Direction::Left:
      return line * 4 + at;
    case Direction::Right:
      return line * 4 + 3 - at;
    case Direction::Up:
      return at * 4 + line;
    case Direction::Down:
      return (3 - at) * 4 + line;
  }
  return 0;
}
}  // namespace
bool slide(State& state, Direction direction) {
  bool changed = false;
  for (int line = 0; line < 4; ++line) {
    uint8_t cells[4]{};
    int count = 0;
    for (int at = 0; at < 4; ++at) {
      const auto value = state.cells[index(direction, line, at)];
      if (value) cells[count++] = value;
    }
    uint8_t merged[4]{};
    int out = 0;
    for (int at = 0; at < count; ++at) {
      if (at + 1 < count && cells[at] == cells[at + 1] && cells[at] < 31) {
        const auto value = static_cast<uint8_t>(cells[at] + 1);
        merged[out++] = value;
        ++at;
        const auto gain = uint64_t{1} << value;
        state.score = gain > UINT64_MAX - state.score ? UINT64_MAX : state.score + gain;
      } else
        merged[out++] = cells[at];
    }
    for (int at = 0; at < 4; ++at) {
      auto& cell = state.cells[index(direction, line, at)];
      changed |= cell != merged[at];
      cell = merged[at];
    }
  }
  return changed;
}
bool move(State& state, Direction direction) {
  if (!slide(state, direction)) return false;
  if (state.moves < UINT32_MAX) ++state.moves;
  spawn(state);
  return true;
}
}  // namespace game2048
