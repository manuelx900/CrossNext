#pragma once
#include "NumberTypography.h"
#include "ToyboxScreen.h"

// Tools and statistics use the same Jersey numerals as the firmware clock.
namespace toolsdigits {
inline constexpr auto& glyphs = numerals::glyphs;
inline constexpr auto& bits = numerals::bits;
inline constexpr auto face = numerals::defaultFace;
inline void draw(toybox::Screen& screen, freeink::ui::Rect box, const char* value) {
  numerals::draw(screen.target(), box, value);
}
inline void counter(toybox::Screen& screen, freeink::ui::Rect box, const char* value, int maxInkHeight) {
  if (!value || box.empty()) return;
  int count = 0;
  for (const char* p = value; *p; ++p) {
    if ((*p < '0' || *p > '9') && *p != '-') return;
    ++count;
  }
  if (!count) return;
  constexpr int advance = 65, height = 82;
  const int scale = std::min(int(box.width) * 1024 / (std::max(4, count) * advance),
                             std::min(int(box.height), maxInkHeight) * 1024 / height);
  const int x = box.x + (box.width - count * advance * scale / 1024) / 2;
  const int y = box.y + (box.height - height * scale / 1024) / 2;
  for (int n = 0; n < count; ++n) {
    const auto& glyph = *numerals::glyph(value[n]);
    const char digit[] = {value[n], 0};
    numerals::draw(screen.target(),
                   freeink::ui::makeRect(x + n * advance * scale / 1024, y + (height - glyph.top) * scale / 1024,
                                         advance * scale / 1024, glyph.height * scale / 1024),
                   digit);
  }
}
}  // namespace toolsdigits
