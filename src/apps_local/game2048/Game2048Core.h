#pragma once

#include <cstdint>

namespace game2048 {
enum class Direction { Up, Right, Down, Left };
struct State {
  uint8_t cells[16]{};
  uint64_t score = 0;
  uint32_t moves = 0, random = 1;
};
void start(State& state, uint32_t seed);
bool slide(State& state, Direction direction);
bool move(State& state, Direction direction);
bool spawn(State& state);
bool canMove(const State& state);
uint8_t highest(const State& state);
uint32_t random(State& state);
}  // namespace game2048
