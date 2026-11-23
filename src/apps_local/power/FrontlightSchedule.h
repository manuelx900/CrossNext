#pragma once

#include <cstdint>

namespace frontlightschedule {
// Equal endpoints disable the interval. A later start spans midnight.
constexpr bool activeAt(const unsigned minute, const unsigned start, const unsigned end) {
  if (start == end) return false;
  return start < end ? (minute >= start && minute < end) : (minute >= start || minute < end);
}

void poll();

enum ManualChannel : uint8_t { Brightness = 1, Warmth = 2, Power = 4 };
void noteManual(uint8_t channels);
bool powerForRestart();

struct Levels {
  uint8_t brightness = 0, warmth = 0;
  bool on = false;
  bool operator==(const Levels&) const = default;
};

// The manual preferences exclude any channels still showing scheduled values.
Levels manualLevels();
uint8_t manualWarmthMemory(uint8_t warmth);
}  // namespace frontlightschedule
