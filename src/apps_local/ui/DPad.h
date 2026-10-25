#pragma once
#include <FreeInkUI.h>

#include <algorithm>

namespace dpad {
namespace fui = freeink::ui;
inline fui::Rect key(fui::Rect area, int direction) {
  const int size = std::min<int>(area.width / 3, area.height / 3);
  static constexpr int x[] = {1, 2, 1, 0}, y[] = {0, 1, 2, 1};
  return fui::makeRect(area.x + (area.width - size * 3) / 2 + x[direction] * size,
                       area.y + (area.height - size * 3) / 2 + y[direction] * size, size, size);
}
template <typename Screen>
void build(Screen& screen, fui::Rect area, fui::ActionId action) {
  const auto ink = fui::Paint::solid(fui::Color::Black);
  for (int n = 0; n < 4; ++n) {
    const auto r = key(area, n);
    screen.target().stroke(r.inset(fui::Insets{1, 1, 1, 1}), ink, 3);
    const int x = r.x + r.width / 2, y = r.y + r.height / 2, d = std::max(6, int(r.width) / 6);
    const int dx = n == 1 ? 1 : n == 3 ? -1 : 0, dy = n == 2 ? 1 : n == 0 ? -1 : 0;
    screen.target().line({int16_t(x - dx * d), int16_t(y - dy * d)}, {int16_t(x + dx * d), int16_t(y + dy * d)}, 3,
                         ink);
    screen.target().line({int16_t(x + dx * d), int16_t(y + dy * d)}, {int16_t(x - dy * d), int16_t(y + dx * d)}, 3,
                         ink);
    screen.target().line({int16_t(x + dx * d), int16_t(y + dy * d)}, {int16_t(x + dy * d), int16_t(y - dx * d)}, 3,
                         ink);
    screen.frame().hit(r, action, n, fui::InputTouch);
  }
}
}  // namespace dpad
