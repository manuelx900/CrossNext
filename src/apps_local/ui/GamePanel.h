#pragma once
#include "Dashboard.h"
namespace gamepanel {
namespace fui = freeink::ui;

inline fui::Rect centeredBlock(fui::Rect area, int height) {
  height = std::clamp(height, 0, int(area.height));
  return fui::makeRect(area.x, area.y + (area.height - height) / 2, area.width, height);
}
enum class Mark : uint8_t {
  Play,
  New,
  Help,
  Restart,
  Menu,
  Speed,
  Grid,
  Settings,
  Folder,
  Search,
  Computer,
  Link,
  Accuse,
  Book,
  Clock,
  Flag,
  Dig,
  Pause,
  Back
};
inline void mark(toybox::Screen& s, fui::Rect box, Mark type, bool inverse = false) {
  const int side = std::min(int(box.width), int(box.height));
  box = fui::makeRect(box.x + (box.width - side) / 2, box.y + (box.height - side) / 2, side, side);
  const auto ink = inverse ? dashboard::paper : dashboard::ink;
  const int x = box.x, y = box.y, w = box.width, u = std::max(1, w / 12), cx = x + w / 2, cy = y + w / 2;
  switch (type) {
    case Mark::Play:
      s.target().triangle({int16_t(x + 2 * u), int16_t(y + u)}, {int16_t(x + 2 * u), int16_t(y + w - u)},
                          {int16_t(x + w - u), int16_t(cy)}, ink);
      break;
    case Mark::Pause:
      for (int n : {0, 1}) s.target().fill(fui::makeRect(x + 2 * u + n * 5 * u, y + u, 3 * u, w - 2 * u), ink);
      break;
    case Mark::Back:
      s.target().line({int16_t(x + 2 * u), int16_t(cy)}, {int16_t(x + 11 * u), int16_t(cy)}, u, ink);
      s.target().line({int16_t(x + 2 * u), int16_t(cy)}, {int16_t(x + 6 * u), int16_t(cy - 4 * u)}, u, ink);
      s.target().line({int16_t(x + 2 * u), int16_t(cy)}, {int16_t(x + 6 * u), int16_t(cy + 4 * u)}, u, ink);
      break;
    case Mark::Menu:
      for (int n = 0; n < 3; ++n) s.target().fill(fui::makeRect(x + u, y + u + n * 4 * u, w - 2 * u, 2 * u), ink);
      break;
    case Mark::Grid:
      for (int n = 0; n < 9; ++n)
        s.target().fill(fui::makeRect(x + n % 3 * 4 * u, y + n / 3 * 4 * u, 3 * u, 3 * u), ink);
      break;
    case Mark::New: {
      s.target().stroke(box.inset(fui::Insets{int16_t(u), int16_t(2 * u), int16_t(u), int16_t(2 * u)}), ink,
                        std::max(2, w / 20));
      s.target().line({int16_t(cx), int16_t(y + 3 * u)}, {int16_t(cx), int16_t(y + 9 * u)}, std::max(2, w / 16), ink);
      s.target().line({int16_t(x + 3 * u), int16_t(cy)}, {int16_t(x + 9 * u), int16_t(cy)}, std::max(2, w / 16), ink);
      break;
    }
    case Mark::Folder:
      s.target().stroke(fui::makeRect(x + u, y + 3 * u, w - 2 * u, w - 4 * u), ink, std::max(2, w / 32));
      s.target().stroke(fui::makeRect(x + u, y + u, 5 * u, 3 * u), ink, std::max(2, w / 32));
      break;
    case Mark::Computer:
      s.target().stroke(fui::makeRect(x, y + u, w, w * 2 / 3), ink, 2 * u);
      s.target().fill(fui::makeRect(cx - u, y + w * 2 / 3, 2 * u, 3 * u), ink);
      s.target().fill(fui::makeRect(x + 3 * u, y + w - u, 6 * u, u), ink);
      break;
    case Mark::Help: {
      s.target().stroke(box, ink, std::max(2, w / 8), w / 2);
      static constexpr uint8_t points[][2] = {{11, 11}, {11, 9},  {13, 7},  {18, 7},
                                              {21, 10}, {21, 13}, {16, 17}, {16, 20}};
      const int weight = std::max(1, w / 12);
      for (unsigned n = 1; n < sizeof(points) / sizeof(points[0]); ++n)
        s.target().line({int16_t(x + points[n - 1][0] * w / 32), int16_t(y + points[n - 1][1] * w / 32)},
                        {int16_t(x + points[n][0] * w / 32), int16_t(y + points[n][1] * w / 32)}, weight, ink);
      s.target().fill(fui::makeRect(x + 15 * w / 32, y + 24 * w / 32, weight, weight), ink);
      break;
    }
    case Mark::Search:
      s.target().stroke(fui::makeRect(x, y, w * 3 / 4, w * 3 / 4), ink, 2 * u, w / 3);
      s.target().line({int16_t(x + w * 2 / 3), int16_t(y + w * 2 / 3)}, {int16_t(x + w - u), int16_t(y + w - u)}, 3 * u,
                      ink);
      break;
    case Mark::Book: {
      const int weight = std::max(2, w / 24);
      s.target().stroke(fui::makeRect(x + 2 * u, y + u, 8 * u, 10 * u), ink, weight);
      s.target().line({int16_t(x + 3 * u), int16_t(y + u)}, {int16_t(x + 3 * u), int16_t(y + 11 * u)}, weight, ink);
      s.target().stroke(fui::makeRect(x + 4 * u, y + 3 * u, 4 * u, 3 * u), ink, weight);
      for (int n = 0; n < 2; ++n)
        s.target().line({int16_t(x + 4 * u), int16_t(y + (8 + n) * u)}, {int16_t(x + 8 * u), int16_t(y + (8 + n) * u)},
                        weight, ink);
      break;
    }
    case Mark::Clock:
      dashboard::icon(s, box, extra_clock_64, inverse);
      break;
    case Mark::Speed:
      dashboard::icon(s, box, extra_speed_64, inverse);
      break;
    case Mark::Settings:
      for (int n = 0; n < 3; ++n) {
        const int yy = y + (2 + n * 4) * u, xx = x + (n == 1 ? 8 : 4) * u;
        s.target().line({int16_t(x), int16_t(yy)}, {int16_t(x + w), int16_t(yy)}, u, ink);
        s.target().stroke(fui::makeRect(xx - u, yy - u, 3 * u, 3 * u), ink, u);
      }
      break;
    case Mark::Restart:
      dashboard::icon(s, box, extra_rotate_cw_64, inverse);
      break;
    case Mark::Link:
      dashboard::icon(s, box, extra_route_64, inverse);
      break;
    case Mark::Accuse:
      s.target().line({int16_t(x + 3 * u), int16_t(y + 2 * u)}, {int16_t(x + 9 * u), int16_t(y + 8 * u)}, 4 * u, ink);
      s.target().line({int16_t(x + 8 * u), int16_t(y + 2 * u)}, {int16_t(x + 2 * u), int16_t(y + 8 * u)}, 2 * u, ink);
      s.target().fill(fui::makeRect(x + u, y + w - 2 * u, 8 * u, 2 * u), ink);
      break;
    case Mark::Flag:
      s.target().fill(fui::makeRect(x + 3 * u, y + u, u, 10 * u), ink);
      s.target().triangle({int16_t(x + 4 * u), int16_t(y + u)}, {int16_t(x + 4 * u), int16_t(y + 6 * u)},
                          {int16_t(x + 10 * u), int16_t(y + 3 * u)}, ink);
      s.target().fill(fui::makeRect(x + u, y + 10 * u, 6 * u, 2 * u), ink);
      break;
    case Mark::Dig:
      s.target().fill(fui::makeRect(cx - u, y + u, 2 * u, 6 * u), ink);
      s.target().fill(fui::makeRect(x + 3 * u, y + 5 * u, 6 * u, 4 * u), ink);
      s.target().triangle({int16_t(x + 3 * u), int16_t(y + 9 * u)}, {int16_t(x + 9 * u), int16_t(y + 9 * u)},
                          {int16_t(cx), int16_t(y + 11 * u)}, ink);
      break;
  }
}
inline void row(toybox::Screen& s, fui::Rect r, const char* label, fui::ActionId action, int value, Mark symbol,
                bool selected = false, const char* trailing = nullptr, bool arrow = true,
                const fui::BitmapRef* customMark = nullptr) {
  selected = s.frame().buttonSelected(action, value, selected);
  const int radius = chrome::lightHeader ? lightui::kControlRadius : 0;
  if (selected)
    s.target().fill(r, chrome::lightHeader ? lightui::selected : dashboard::ink, radius);
  else
    s.target().stroke(r, dashboard::ink, chrome::lightHeader ? 1 : pixelui::kBorder, radius);
  dashboard::hit(s, r, action, value);
  const int side = std::max(12, std::min({40, int(r.height) - 18, int(r.width) / 5}));
  const int margin = r.width < 180 ? 8 : 16;
  const auto markRect = fui::makeRect(r.x + margin, r.y + (r.height - side) / 2, side, side);
  if (customMark)
    s.target().bitmap(markRect, *customMark, fui::BitmapMode::Contain, selected ? dashboard::paper : dashboard::ink);
  else
    mark(s, markRect, symbol, selected);
  if (arrow) {
    const int x = r.right() - 22, y = r.y + r.height / 2, d = 6;
    const auto ink = selected ? dashboard::paper : dashboard::ink;
    s.target().line({int16_t(x - d), int16_t(y - d)}, {int16_t(x), int16_t(y)}, 3, ink);
    s.target().line({int16_t(x - d), int16_t(y + d)}, {int16_t(x), int16_t(y)}, 3, ink);
  }
  int reserve = arrow ? 38 : 12;
  if (trailing && *trailing) {
    fui::TextStyle style;
    style.font = 0;
    const int w = std::min(int(r.width) / 3, int(s.target().measureText(0, trailing, style).width) + 8);
    dashboard::text(s, fui::makeRect(r.right() - reserve - w, r.y, w, r.height), trailing, 0, fui::TextAlign::Right,
                    selected);
    reserve += w + 12;
  }
  dashboard::text(s, fui::makeRect(r.x + margin + side + 10, r.y, r.width - margin - side - 10 - reserve, r.height),
                  label, 1, fui::TextAlign::Left, selected);
}
inline void heading(toybox::Screen& s, fui::Rect r, const char* label) {
  char copy[128];
  if (chrome::lightHeader)
    pixelui::copyNatural(copy, sizeof(copy), label);
  else
    pixelui::copyCaps(copy, sizeof(copy), label);
  fui::TextStyle style;
  style.font = 0;
  const int width = std::min(int(r.width) - 12, int(s.target().measureText(0, copy, style).width) + 12);
  dashboard::text(s, fui::makeRect(r.x, r.y, width, r.height), copy, 0, fui::TextAlign::Left);
  if (width + 12 < r.width)
    s.target().line({int16_t(r.x + width + 8), int16_t(r.y + r.height / 2)}, {r.right(), int16_t(r.y + r.height / 2)},
                    2, dashboard::ink);
}
inline void segment(toybox::Screen& s, fui::Rect band, int index, int count, const char* label, fui::ActionId action,
                    int value, Mark symbol, bool selected) {
  const auto box = lightui::segmentRect(band, index, count);
  if (!chrome::lightHeader) {
    row(s, box, label, action, value, symbol, selected, nullptr, false);
    return;
  }
  selected = s.frame().buttonSelected(action, value, selected);
  lightui::segment(s.target(), band, index, count, selected);
  dashboard::hit(s, box, action, value);
  const int side = std::min(32, int(box.height) - 16);
  mark(s, fui::makeRect(box.x + 12, box.y + (box.height - side) / 2, side, side), symbol, selected);
  dashboard::text(s, fui::makeRect(box.x + side + 22, box.y, box.width - side - 32, box.height), label, 0,
                  fui::TextAlign::Center, selected);
}
}  // namespace gamepanel
