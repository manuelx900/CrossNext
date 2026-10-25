#pragma once
namespace rotation {
constexpr int step(bool right) { return right ? 1 : 3; }
constexpr int toggle(int current, int initial, bool right) {
  return current == initial ? (initial + step(right)) % 4 : initial;
}
constexpr int pair(int current, bool right) { return (current + (current % 2 ? 4 - step(right) : step(right))) % 4; }
}  // namespace rotation
