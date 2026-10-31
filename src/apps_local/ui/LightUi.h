#pragma once
#include <FreeInkUI.h>

#include <algorithm>

#include "../../../lib/GfxRenderer/ChromeRegion.h"
namespace lightui {
namespace fui = freeink::ui;
inline constexpr auto ink = fui::Paint::solid(fui::Color::Black);
inline constexpr auto paper = fui::Paint::solid(fui::Color::White);
inline constexpr auto rule = fui::Paint::dither(fui::Color::DarkGray);
inline constexpr auto selected = fui::Paint::solid(fui::Color::Black);
inline constexpr auto highlight = fui::Paint::dither(fui::Color::LightGray);
inline constexpr uint8_t kControlRadius = 5;
inline uint8_t radius() { return chrome::lightHeader ? kControlRadius : 0; }
inline void outline(fui::DrawTarget& target, fui::Rect box, fui::Paint paint, uint8_t width = 2) {
  target.stroke(box, paint, chrome::lightHeader ? 1 : width, radius());
}
inline void surface(fui::DrawTarget& target, fui::Rect box, fui::Paint paint) { target.fill(box, paint, radius()); }
inline void trimCorners(fui::DrawTarget& target, fui::Rect box, fui::Paint background = paper) {
  const int r = std::min({int(radius()), int(box.width) / 2, int(box.height) / 2});
  for (int y = 0; y < r; ++y) {
    const int dy = r - y;
    int x = r;
    while (x > 0 && x * x + dy * dy > r * r) --x;
    const int inset = r - x;
    if (!inset) continue;
    for (const int yy : {int(box.y) + y, int(box.bottom()) - 1 - y}) {
      target.fill(fui::makeRect(box.x, yy, inset, 1), background);
      target.fill(fui::makeRect(box.right() - inset, yy, inset, 1), background);
    }
  }
}
inline void separator(fui::DrawTarget& target, fui::Rect box) {
  target.fill(fui::makeRect(box.x, box.bottom() - 1, box.width, 1), rule);
}
inline fui::StyleSet controlStyles(bool rows = false) {
  fui::StyleSet s;
  s.explicitlySet = true;
  s.normal.background = paper;
  s.normal.foreground = ink;
  s.normal.border = ink;
  s.normal.borderWidth = rows ? 0 : 1;
  s.normal.radius = rows ? 0 : kControlRadius;
  s.selected = s.focused = s.active = s.disabled = s.normal;
  s.selected.border = s.focused.border = s.active.border = ink;
  s.selected.borderWidth = s.focused.borderWidth = s.active.borderWidth = rows ? 0 : 1;
  if (rows) s.selected.background = s.focused.background = s.active.background = highlight;
  if (!rows) {
    s.selected.background = s.focused.background = s.active.background = selected;
    s.selected.foreground = s.focused.foreground = s.active.foreground = paper;
  }
  s.disabled.foreground = fui::Paint::dither(fui::Color::DarkGray);
  return s;
}
inline fui::StyleSet tabStyles() {
  auto styles = controlStyles();
  styles.normal.borderWidth = styles.selected.borderWidth = styles.focused.borderWidth = styles.active.borderWidth =
      styles.disabled.borderWidth = 0;
  return styles;
}
inline fui::Rect segmentRect(fui::Rect band, int index, int count) {
  const int left = band.x + band.width * index / count;
  return fui::makeRect(left, band.y, band.width * (index + 1) / count - band.width * index / count, band.height);
}
inline void segment(fui::DrawTarget& target, fui::Rect band, int index, int count, bool active) {
  if (index == 0) target.fill(band, paper, kControlRadius);
  if (active) target.fill(segmentRect(band, index, count).inset({1, 1, 1, 1}), selected, kControlRadius - 1);
  if (index == count - 1) target.stroke(band, ink, 1, kControlRadius);
}
template <size_t Capacity>
void tabs(fui::Frame<Capacity>& frame, fui::Rect band, fui::TabBarProps& props) {
  frame.target().fill(band, paper, kControlRadius);
  props.tabStyles = tabStyles();
  props.tabStyles.normal.background = fui::Paint{};
  props.tabInset = {1, 1, 1, 1};
  props.contentInset = {};
  fui::tabBar(frame, band, props);
  frame.target().stroke(band, ink, 1, kControlRadius);
}
inline void tick(fui::DrawTarget& t, fui::Rect r) {
  const int cy = r.y + r.height / 2;
  t.line({int16_t(r.x + 2), int16_t(cy)}, {int16_t(r.x + r.width / 3), int16_t(r.bottom() - 4)}, 2, ink);
  t.line({int16_t(r.x + r.width / 3), int16_t(r.bottom() - 4)}, {int16_t(r.right() - 2), int16_t(r.y + 3)}, 2, ink);
}
inline void shade(fui::DrawTarget& t, fui::Rect screen) {
  static constexpr uint8_t bits[] = {0xAA, 0x00};
  const fui::BitmapRef pattern{bits, 8, 2, fui::BitmapFormat::BW1};
  const int band = std::clamp(chrome::headerHeight, 0, int(screen.height));
  if (band) t.bitmap(fui::makeRect(screen.x, screen.y, screen.width, band), pattern, fui::BitmapMode::Tile, paper);
  t.bitmap(fui::makeRect(screen.x, screen.y + band, screen.width, screen.height - band), pattern, fui::BitmapMode::Tile,
           ink);
}
}  // namespace lightui
