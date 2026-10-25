#pragma once
#include <Icon.h>

#include <array>
namespace pomodoro {
constexpr std::array<uint8_t, 128> timerBits() {
  std::array<uint8_t, 128> bits{};
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 32; ++x) {
      const int dx = x - 16, dy = y - 18, r = dx * dx + dy * dy;
      const bool ink = (r >= 100 && r <= 155) || (x >= 13 && x <= 18 && y >= 1 && y <= 4) ||
                       (x >= 15 && x <= 17 && y >= 8 && y <= 18) || (x >= 16 && x <= 23 && y >= 17 && y <= 19);
      if (!ink) bits[y * 4 + x / 8] |= uint8_t(0x80u >> (x % 8));
    }
  return bits;
}
inline constexpr auto timerBitmap = timerBits();
inline constexpr freeink::Icon timerIcon = {32, 32, 16, timerBitmap.data()};
}  // namespace pomodoro
