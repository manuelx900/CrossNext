#pragma once

#include "../../../lib/Utf8/Utf8.h"
#include "../ui/PixelText.h"

namespace dictionaryui {
// Display-only initial capital; lookup keys and the remaining spelling stay intact.
inline void copyWordHeading(char* out, size_t capacity, const char* word) {
  pixelui::copyNatural(out, capacity, word, pixelui::LabelCase::Verbatim);
  if (!capacity) return;
  const size_t first = std::strspn(out, " \t\r\n");
  auto* value = reinterpret_cast<unsigned char*>(out + first);
  uint32_t cp = value[0];
  if (cp >= 0x80) {
    if (cp < 0xC2 || cp >= 0xE0 || !value[1]) return;
    cp = ((cp & 0x1F) << 6) | (value[1] & 0x3F);
  }
  const auto capital = utf8SmallCapsUpper(cp);
  if (capital == cp) return;
  if (capital < 0x80)
    value[0] = static_cast<unsigned char>(capital);
  else if (capital < 0x800) {
    value[0] = static_cast<unsigned char>(0xC0 | (capital >> 6));
    value[1] = static_cast<unsigned char>(0x80 | (capital & 0x3F));
  }
}
}  // namespace dictionaryui
