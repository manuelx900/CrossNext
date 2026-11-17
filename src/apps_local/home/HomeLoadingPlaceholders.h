#pragma once
#include <FreeInkUI.h>

#include <algorithm>

namespace homeui::loading {
namespace fui = freeink::ui;
inline void bar(fui::DrawTarget& target, fui::Rect area, int percent, fui::TextAlign align = fui::TextAlign::Left) {
  if (area.empty()) return;
  const int width = std::max(1, int(area.width) * percent / 100), height = std::min(8, int(area.height));
  const int x = area.x + (align == fui::TextAlign::Center ? (area.width - width) / 2 : 0);
  target.fill(fui::makeRect(x, area.y + (area.height - height) / 2, width, height),
              fui::Paint::dither(fui::Color::LightGray));
}
inline void metadata(fui::DrawTarget& target, fui::Rect title, fui::Rect author,
                     fui::TextAlign align = fui::TextAlign::Left) {
  bar(target, title, 82, align);
  bar(target, author, 58, align);
}
}  // namespace homeui::loading
