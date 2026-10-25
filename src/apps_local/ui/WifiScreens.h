#pragma once
#include <FreeInkUI.h>

#include <algorithm>

#include "PixelMenu.h"
namespace wifiscreens {
namespace fui = freeink::ui;
inline void text(fui::DrawTarget& target, fui::Rect r, const char* value, bool bold = false, int font = 0) {
  fui::TextStyle style;
  style.font = font;
  style.bold = bold;
  style.maxLines = 2;
  target.text(r, value, style);
}
inline void check(fui::DrawTarget& target, fui::Rect r, bool checked) {
  const auto black = fui::Paint::solid(fui::Color::Black);
  target.stroke(r, black, 2);
  if (!checked) return;
  target.fill(r, black);
  const auto white = fui::Paint::solid(fui::Color::White);
  target.line({int16_t(r.x + 5), int16_t(r.y + r.height / 2)}, {int16_t(r.x + 12), int16_t(r.bottom() - 7)}, 3, white);
  target.line({int16_t(r.x + 12), int16_t(r.bottom() - 7)}, {int16_t(r.right() - 5), int16_t(r.y + 7)}, 3, white);
}
inline void signal(fui::DrawTarget& target, fui::Rect r, int rssi, bool encrypted, bool securityKnown = true) {
  const auto black = fui::Paint::solid(fui::Color::Black);
  const int bars = rssi >= -50 ? 4 : rssi >= -60 ? 3 : rssi >= -70 ? 2 : 1;
  for (int n = 0; n < 4; ++n) {
    const auto b = fui::makeRect(r.right() - 40 + n * 10, r.bottom() - 10 - n * 6, 7, 10 + n * 6);
    if (n < bars)
      target.fill(b, black);
    else
      target.stroke(b, black, 1);
  }
  if (!securityKnown) {
    text(target, fui::makeRect(r.right() - 77, r.bottom() - 32, 22, 32), "?", true, 4);
  } else if (encrypted) {
    const int x = r.right() - 77, y = r.bottom() - 31;
    target.stroke(fui::makeRect(x + 4, y, 14, 18), black, 3, 7);
    target.fill(fui::makeRect(x, y + 12, 22, 19), black);
    target.fill(fui::makeRect(x + 10, y + 18, 3, 7), fui::Paint::solid(fui::Color::White));
  }
}
template <typename Screen>
void button(Screen& screen, fui::ButtonProps& props, fui::Rect box, const char* label, int action, int value,
            bool primary = false) {
  props = {};
  props.label = label;
  props.action = action;
  props.value = value;
  props.text.font = 0;
  props.text.bold = true;
  props.text.align = fui::TextAlign::Center;
  props.styles = pixelui::rowStyles();
  props.styles.normal.borderWidth = 2;
  props.minTouchSize = 0;
  if (primary) props.state = fui::StateSelected;
  screen.button(props, box);
}
inline void detail(fui::DrawTarget& target, fui::Rect box, const char* title, const char* value) {
  target.stroke(box, fui::Paint::solid(fui::Color::Black), 2);
  text(target, fui::makeRect(box.x + 14, box.y + 10, box.width - 28, 34), title, true);
  text(target, fui::makeRect(box.x + 14, box.y + 44, box.width - 28, box.height - 48), value, false, 4);
}
}  // namespace wifiscreens
