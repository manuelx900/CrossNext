#pragma once

#include <cstdint>

namespace readerlinespacing {
inline constexpr uint8_t kPercentages[] = {100, 110, 120, 130, 145, 160, 175};
inline constexpr uint8_t kDefault = 2;
inline constexpr uint8_t kCount = sizeof(kPercentages) / sizeof(kPercentages[0]);
constexpr uint8_t fromLegacy(const uint8_t value) { return value < 4 ? value + 1 : kDefault; }
constexpr float multiplier(const uint8_t value) { return kPercentages[value < kCount ? value : kDefault] / 100.0f; }
}  // namespace readerlinespacing
