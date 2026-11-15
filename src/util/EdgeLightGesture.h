#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>

namespace edgelight {
enum class Action : uint8_t { None, Brightness, Warmth };
struct Event {
  Action action = Action::None;
  int8_t step = 0;
};
inline Event classify(int sx, int sy, int ex, int ey, int width, int height, uint8_t left, uint8_t right) {
  if (width <= 0 || height <= 0 || sx < 0 || sx >= width || sy < 0 || sy >= height || std::abs(ey - sy) < 32 ||
      std::abs(ey - sy) <= std::abs(ex - sx) * 2)
    return {};
  const int edge = std::max(20, width * 5 / 100);
  const unsigned action = sx < edge ? left : sx >= width - edge ? right : 0;
  if (!action || action > uint8_t(Action::Warmth)) return {};
  return {Action(action), int8_t(ey < sy ? 10 : -10)};
}
}  // namespace edgelight
