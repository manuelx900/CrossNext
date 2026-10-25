#pragma once

#include <cstdint>

#include "../../fontIds.h"

namespace flashcards {
enum class TextApp : uint8_t { Flashcards, Words };
inline constexpr uint8_t kTextSizeCount = 5;
inline constexpr uint8_t kTextPoints[] = {12, 14, 16, 18, 10};
inline constexpr uint8_t kTextSizeOrder[] = {4, 0, 1, 2, 3};
inline constexpr uint8_t kLargestTextSize = 3, kSmallestTextSize = 4;
inline constexpr uint8_t kSmartTextFloor = 0;  // 12 pt; 10 pt remains available manually or for an oversized word.
inline uint8_t smallerTextSize(uint8_t value) { return value == 0 || value == 4 ? 4 : value - 1; }
inline uint8_t validTextSize(uint8_t value) { return value < kTextSizeCount ? value : 0; }
inline int textSizeFont(uint8_t value) {
  static constexpr int fonts[] = {NOTOSERIF_12_FONT_ID, NOTOSERIF_14_FONT_ID, NOTOSERIF_16_FONT_ID,
                                  NOTOSERIF_18_FONT_ID, UI_10_FONT_ID};
  value = validTextSize(value);
  return fonts[value];
}
}  // namespace flashcards
