#pragma once
#include <FreeInkUI.h>

#include <algorithm>
#include <cstring>
namespace fittext {
inline void draw(freeink::ui::DrawTarget& target, freeink::ui::Rect box, const char* copy, int font = 3, int lines = 10,
                 freeink::ui::TextAlign align = freeink::ui::TextAlign::Left,
                 freeink::ui::Color color = freeink::ui::Color::Black) {
  if (box.empty() || !copy) return;
  freeink::ui::TextStyle style;
  style.font = font;
  style.maxLines = 1;
  style.align = align;
  style.color = color;
  const int candidates[] = {font, font == 2 ? 1 : 0, 0, 3, 4};
  for (int candidate : candidates) {
    const int height = target.lineHeight(candidate);
    if (height <= 0 || height > box.height || height > target.lineHeight(font) || (font == 4 && candidate != 4))
      continue;
    style.font = candidate;
    style.maxLines = std::min(lines, std::max(1, int(box.height) / height));
    bool fits = true;
    freeink::ui::layoutText(target, box, copy, style, [&](const char* run, freeink::ui::Rect) {
      if (std::strstr(run, "…")) fits = false;
    });
    if (fits) break;
  }
  auto lineStyle = style;
  lineStyle.maxLines = 1;
  lineStyle.align = freeink::ui::TextAlign::Left;
  freeink::ui::layoutText(target, box, copy, style,
                          [&](const char* run, freeink::ui::Rect line) { target.text(line, run, lineStyle); });
}
}  // namespace fittext
