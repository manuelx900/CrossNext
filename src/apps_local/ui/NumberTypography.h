#pragma once
#include <FreeInkUI.h>

#include <algorithm>
#include <cstddef>

#include "fonts/Numerals.h"
namespace numerals {
struct Face {
  const Glyph* glyphs;
  size_t count;
  const uint8_t* bits;
};
inline constexpr Face defaultFace{glyphs, sizeof(glyphs) / sizeof(glyphs[0]), bits};
inline const Glyph* glyph(unsigned char ch, const Face& face = defaultFace) {
  for (size_t n = 0; n < face.count; ++n)
    if (face.glyphs[n].code == ch) return &face.glyphs[n];
  return nullptr;
}
inline void draw(freeink::ui::DrawTarget& target, freeink::ui::Rect box, const char* value,
                 freeink::ui::Color color = freeink::ui::Color::Black, const Face& face = defaultFace) {
  if (!value || box.empty()) return;
  int advance = 0, left = 0, right = 0, top = 0, bottom = 0;
  bool visible = false;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(value); *p; ++p) {
    const auto* g = glyph(*p, face);
    if (!g) return;
    if (g->width) {
      left = visible ? std::min(left, advance + g->left) : advance + g->left;
      right = std::max(right, advance + g->left + g->width);
      top = std::max(top, int(g->top));
      bottom = std::max(bottom, int(g->height) - g->top);
      visible = true;
    }
    advance += g->advance;
  }
  if (!visible || right <= left || top + bottom <= 0) return;
  const int scale = std::min(int(box.width) * 1024 / (right - left), int(box.height) * 1024 / (top + bottom));
  if (!scale) return;
  const int x = box.x + (box.width - (right - left) * scale / 1024) / 2 - left * scale / 1024;
  const int y = box.y + (box.height - (top + bottom) * scale / 1024) / 2;
  const auto ink = freeink::ui::Paint::solid(color);
  advance = 0;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(value); *p; ++p) {
    const auto& g = *glyph(*p, face);
    for (int row = 0; row < g.height; ++row) {
      const int y0 = y + (top - g.top + row) * scale / 1024;
      const int y1 = y + (top - g.top + row + 1) * scale / 1024;
      if (y1 <= y0) continue;
      int start = -1;
      for (int col = 0; col <= g.width; ++col) {
        const int bit = row * g.width + col;
        const bool on = col < g.width && (face.bits[g.offset + bit / 8] & (128 >> (bit % 8)));
        if (on && start < 0) start = col;
        if (!on && start >= 0) {
          const int x0 = x + (advance + g.left + start) * scale / 1024;
          const int x1 = x + (advance + g.left + col) * scale / 1024;
          if (x1 > x0) target.fill(freeink::ui::makeRect(x0, y0, x1 - x0, y1 - y0), ink);
          start = -1;
        }
      }
    }
    advance += g.advance;
  }
}
}  // namespace numerals
