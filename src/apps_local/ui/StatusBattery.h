#pragma once
#include <components/bars/battery-indicator.h>

#include <algorithm>

namespace statusbattery {
inline constexpr int width = 30, height = 19, terminal = 4;
inline freeink::ui::Rect headerIcon(int right, int top) {
  return freeink::ui::makeRect(right - 34, top + 27, width, height);
}
inline freeink::ui::Rect headerPercent(freeink::ui::Rect icon, int top) {
  return freeink::ui::makeRect(icon.x - 70, top + 8, 66, 56);
}
inline void paintIcon(freeink::ui::DrawTarget& target, freeink::ui::Rect body,
                      const freeink::ui::BatteryIndicatorProps& props) {
  namespace fui = freeink::ui;
  const auto ink = fui::Paint::solid(props.color);
  const auto paper = fui::Paint::solid(fui::invertedColor(props.color));
  const auto x = [&](int n) { return body.x + n * body.width / width; };
  const auto y = [&](int n) { return body.y + n * body.height / height; };
  const int border = body.height >= 16 ? 2 : 1;
  target.stroke(body, ink, border);
  target.fill(fui::makeRect(body.right(), y(5), props.terminalWidth, y(14) - y(5)), ink);
  const int cavityWidth = x(26) - x(4);
  const int percent = std::min(100, int(props.percent));
  const int fill = props.charging ? cavityWidth : cavityWidth * percent / 100;
  if (fill > 0) target.fill(fui::makeRect(x(4), y(4), fill, y(15) - y(4)), ink);
  if (props.charging) {
    target.line({int16_t(x(18)), int16_t(y(-1))}, {int16_t(x(13)), int16_t(y(10))}, border, paper);
    target.line({int16_t(x(13)), int16_t(y(10))}, {int16_t(x(20)), int16_t(y(8))}, border, paper);
    target.line({int16_t(x(20)), int16_t(y(8))}, {int16_t(x(15)), int16_t(y(19))}, border, paper);
  }
}
inline void configure(freeink::ui::BatteryIndicatorProps& props) {
  props.glyphWidth = width;
  props.glyphHeight = height;
  props.terminalWidth = terminal;
  props.iconPainter = &paintIcon;
}
inline void draw(freeink::ui::DrawTarget& target, freeink::ui::Rect body, int percent, bool charging,
                 freeink::ui::Color color) {
  freeink::ui::BatteryIndicatorProps props;
  configure(props);
  props.percent = static_cast<uint8_t>(std::clamp(percent, 0, 100));
  props.charging = charging;
  props.color = color;
  freeink::ui::paintBatteryIndicator(target, freeink::ui::makeRect(body.x, body.y, width + terminal, height), props);
}
}  // namespace statusbattery
