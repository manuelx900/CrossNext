#pragma once
#include <algorithm>
#include <cstdio>

#include "PixelMenu.h"
namespace pageui {
namespace fui = freeink::ui;
struct Geometry {
  fui::Rect before, count, after;
};
inline fui::Rect afterContent(fui::Rect body, int bottom, int height = 44) {
  const int last = body.bottom() - height;
  const int top = std::min(last, bottom + std::max(12, (int(body.bottom()) - bottom - height) / 2));
  return fui::makeRect(body.x, std::max(int(body.y), top), body.width, height);
}
inline Geometry layout(fui::Rect band, int maxHeight = 48) {
  const int h = std::min(maxHeight, int(band.height)), gap = 10;
  const int count = std::min(maxHeight > 48 ? 180 : 150, std::max(70, int(band.width) - 2 * h - 2 * gap));
  const int x = band.x + (band.width - count - 2 * h - 2 * gap) / 2;
  const int y = band.y + (band.height - h) / 2;
  Geometry g{fui::makeRect(x, y, h, h), fui::makeRect(x + h + gap, y, count, h),
             fui::makeRect(x + h + 2 * gap + count, y, h, h)};
  return g;
}
inline Geometry draw(fui::DrawTarget& target, fui::Rect band, int page, int pages, fui::FontId font = pixelui::kSmall,
                     int maxHeight = 48) {
  const auto g = layout(band, maxHeight);
  const int h = g.before.height;
  const auto ink = fui::Paint::solid(fui::Color::Black);
  if (chrome::lightHeader) {
    lightui::outline(target, fui::makeRect(g.before.x, g.before.y, g.after.right() - g.before.x, g.before.height), ink);
    for (const int x : {int(g.before.right()), int(g.after.x)})
      target.line({int16_t(x), int16_t(g.before.y + 1)}, {int16_t(x), int16_t(g.before.bottom() - 1)}, 1, ink);
  } else
    for (const auto r : {g.before, g.count, g.after}) target.stroke(r, ink, pixelui::kBorder);
  for (int n = 0; n < 2; ++n) {
    const auto r = n ? g.after : g.before;
    const int cx = r.x + r.width / 2, cy = r.y + r.height / 2, d = std::max(5, h / (maxHeight > 48 ? 5 : 7)),
              sign = n ? 1 : -1;
    target.line({int16_t(cx - sign * d / 2), int16_t(cy - d)}, {int16_t(cx + sign * d / 2), int16_t(cy)},
                pixelui::kBorder, ink);
    target.line({int16_t(cx + sign * d / 2), int16_t(cy)}, {int16_t(cx - sign * d / 2), int16_t(cy + d)},
                pixelui::kBorder, ink);
  }
  char label[32];
  std::snprintf(label, sizeof(label), "%d / %d", std::clamp(page, 1, std::max(1, pages)), std::max(1, pages));
  pixelui::text(target, g.count, label, font, fui::Color::Black, fui::TextAlign::Center);
  return g;
}
template <class Screen>
void build(Screen& screen, fui::Rect band, int page, int pages, fui::ActionId previous, int previousValue,
           fui::ActionId next, int nextValue, fui::FontId font = pixelui::kSmall, int maxHeight = 48) {
  const auto g = draw(screen.target(), band, page, pages, font, maxHeight);
  if (page > 1) screen.frame().hit(g.before, previous, previousValue, fui::InputDefault | fui::InputPrev);
  if (page < pages) screen.frame().hit(g.after, next, nextValue, fui::InputDefault | fui::InputNext);
}
}  // namespace pageui
