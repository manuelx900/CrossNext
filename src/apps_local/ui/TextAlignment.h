#pragma once
#include <FreeInkUI.h>

#include <algorithm>
#include <cstring>

namespace textalignment {
inline freeink::ui::TextAlign align(uint8_t value) {
  using freeink::ui::TextAlign;
  return value == 1 ? TextAlign::Center : value == 2 ? TextAlign::Right : TextAlign::Left;
}
inline void line(freeink::ui::DrawTarget& target, freeink::ui::Rect box, const char* text, freeink::ui::TextStyle style,
                 uint8_t alignment, bool last) {
  style.align = align(alignment);
  if (alignment != 3 || last) {
    target.text(box, text, style);
    return;
  }
  char word[128];
  int words = 0, natural = 0;
  const int space = target.measureText(style.font, " ", style).width;
  for (const char* p = text; *p;) {
    while (*p == ' ') ++p;
    const char* start = p;
    while (*p && *p != ' ') ++p;
    if (p == start) break;
    if (p - start >= 128) {
      target.text(box, text, style);
      return;
    }
    std::memcpy(word, start, p - start);
    word[p - start] = 0;
    natural += target.measureText(style.font, word, style).width;
    if (words++) natural += space;
  }
  const int extra = box.width - natural;
  if (words < 2 || extra <= 0) {
    target.text(box, text, style);
    return;
  }
  int x = box.x, ordinal = 0;
  for (const char* p = text; *p;) {
    while (*p == ' ') ++p;
    const char* start = p;
    while (*p && *p != ' ') ++p;
    if (p == start) break;
    std::memcpy(word, start, p - start);
    word[p - start] = 0;
    const int width = target.measureText(style.font, word, style).width;
    target.text(freeink::ui::makeRect(x, box.y, width + 1, box.height), word, style);
    x += width + space + ((ordinal + 1) * extra / (words - 1) - ordinal * extra / (words - 1));
    ++ordinal;
  }
}
}  // namespace textalignment
