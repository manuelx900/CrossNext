#pragma once
#include "FitText.h"
#include "GamePanel.h"
#include "PageIndicator.h"
namespace guideui {
namespace fui = freeink::ui;
inline void text(toybox::Screen& screen, fui::Rect r, const char* label, int lines = 2,
                 fui::TextAlign align = fui::TextAlign::Left, int font = 4) {
  fittext::draw(screen.target(), r, label, font, lines, align);
}
inline void badge(toybox::Screen& screen, fui::Rect r, int n) {
  screen.target().fill(r, dashboard::ink);
  char label[8];
  std::snprintf(label, sizeof(label), "%d", n);
  dashboard::text(screen, r, label, pixelui::kBody, fui::TextAlign::Center, true);
}
inline void grid(toybox::Screen& screen, fui::Rect r, int n, bool inverse = false, bool filled = false) {
  const int cell = std::min(int(r.width), int(r.height)) / n;
  const int x = r.x + (r.width - cell * n) / 2, y = r.y + (r.height - cell * n) / 2;
  const auto paint = inverse ? dashboard::paper : dashboard::ink;
  for (int row = 0; row < n; ++row)
    for (int col = 0; col < n; ++col) {
      auto box = fui::makeRect(x + col * cell, y + row * cell, cell, cell);
      if (filled)
        screen.target().fill(box.inset(fui::Insets{1, 1, 1, 1}), paint);
      else
        screen.target().stroke(box, paint, std::max(1, cell / 8));
    }
}
inline void tile(toybox::Screen& screen, fui::Rect r, const char* value) {
  dashboard::frame(screen, r, 4);
  dashboard::digits(screen, r.inset(fui::Insets{8, 8, 8, 8}), value);
}
inline void arrow(toybox::Screen& screen, fui::Rect r, int direction = 1) {
  const int cy = r.y + r.height / 2, u = std::max(2, std::min(int(r.width), int(r.height)) / 8);
  if (direction == 1) {
    screen.target().fill(fui::makeRect(r.x + u, cy - u, r.width - 4 * u, 2 * u), dashboard::ink);
    screen.target().triangle({int16_t(r.right() - u), int16_t(cy)}, {int16_t(r.right() - 4 * u), int16_t(cy - 3 * u)},
                             {int16_t(r.right() - 4 * u), int16_t(cy + 3 * u)}, dashboard::ink);
  }
}
inline void fourArrows(toybox::Screen& screen, fui::Rect r) {
  const int cx = r.x + r.width / 2, cy = r.y + r.height / 2,
            u = std::max(2, std::min(int(r.width), int(r.height)) / 10);
  for (int n = 0; n < 4; ++n) {
    const int dx = n == 0 ? -1 : n == 1 ? 1 : 0, dy = n == 2 ? -1 : n == 3 ? 1 : 0;
    screen.target().line({int16_t(cx + dx * 2 * u), int16_t(cy + dy * 2 * u)},
                         {int16_t(cx + dx * 4 * u), int16_t(cy + dy * 4 * u)}, 2 * u, dashboard::ink);
    screen.target().triangle({int16_t(cx + dx * 5 * u), int16_t(cy + dy * 5 * u)},
                             {int16_t(cx + dx * 3 * u - dy * 2 * u), int16_t(cy + dy * 3 * u + dx * 2 * u)},
                             {int16_t(cx + dx * 3 * u + dy * 2 * u), int16_t(cy + dy * 3 * u - dx * 2 * u)},
                             dashboard::ink);
  }
}
inline void touchHand(toybox::Screen& screen, fui::Rect r, bool hold = false) {
  static constexpr uint8_t points[][2] = {{9, 30},  {4, 24},  {2, 20},  {2, 18},  {4, 17},  {8, 21},  {8, 7},
                                          {9, 5},   {11, 5},  {12, 7},  {12, 15}, {14, 13}, {16, 15}, {18, 14},
                                          {20, 16}, {22, 16}, {24, 18}, {24, 24}, {22, 30}, {9, 30}};
  const int size = std::min(int(r.width), int(r.height)), x = r.x + (r.width - size) / 2,
            y = r.y + (r.height - size) / 2, weight = std::max(2, size / 18);
  for (size_t n = 1; n < sizeof(points) / sizeof(points[0]); ++n)
    screen.target().line({int16_t(x + points[n - 1][0] * size / 32), int16_t(y + points[n - 1][1] * size / 32)},
                         {int16_t(x + points[n][0] * size / 32), int16_t(y + points[n][1] * size / 32)}, weight,
                         dashboard::ink);
  if (hold)
    screen.target().stroke(fui::makeRect(x + 3 * size / 32, y, 14 * size / 32, 14 * size / 32), dashboard::ink, weight,
                           7 * size / 32);
  else
    for (int n = 0; n < 3; ++n) {
      const int dx = n == 0 ? -1 : n == 2 ? 1 : 0;
      screen.target().line({int16_t(x + (10 + dx * 5) * size / 32), int16_t(y + size / 32)},
                           {int16_t(x + (10 + dx * 3) * size / 32), int16_t(y + 3 * size / 32)}, weight,
                           dashboard::ink);
    }
}
inline void finishFlag(toybox::Screen& screen, fui::Rect r) {
  const int unit = std::max(1, std::min(int(r.width), int(r.height)) / 8);
  screen.target().fill(fui::makeRect(r.x + unit, r.y, unit, 8 * unit), dashboard::ink);
  const auto flag = fui::makeRect(r.x + 2 * unit, r.y, 6 * unit, 4 * unit);
  dashboard::frame(screen, flag, unit);
  for (int row = 0; row < 4; ++row)
    for (int col = 0; col < 6; ++col)
      if ((row + col) % 2 == 0)
        screen.target().fill(fui::makeRect(flag.x + col * unit, flag.y + row * unit, unit, unit), dashboard::ink);
}
inline void restartArt(toybox::Screen& screen, fui::Rect r) {
  const int size = std::min(int(r.width), int(r.height)), cell = size / 2;
  const int x = r.x + (r.width - size) / 2, y = r.y + (r.height - size) / 2;
  for (int n = 0; n < 4; ++n) {
    const auto b = fui::makeRect(x + n % 2 * cell, y + n / 2 * cell, cell - 5, cell - 5);
    if (n == 1 || n == 2)
      screen.target().fill(b, dashboard::ink);
    else
      dashboard::frame(screen, b, 3);
  }
  const auto centre = fui::makeRect(x + size / 4, y + size / 4, size / 2, size / 2);
  screen.target().fill(centre, dashboard::paper);
  gamepanel::mark(screen, centre, gamepanel::Mark::Restart);
}
}  // namespace guideui
