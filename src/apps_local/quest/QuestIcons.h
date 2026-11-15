#pragma once
#include <FreeInkUIIcon.h>

#include <algorithm>
#include <cstdlib>

#include "../ui/ExtraIcons.h"
namespace questicons {
inline void monsterHead(freeink::ui::DrawTarget& target, freeink::ui::Rect r, bool hunt, bool inverse = false) {
  namespace fui = freeink::ui;
  const auto ink = fui::Paint::solid(inverse ? fui::Color::White : fui::Color::Black);
  const auto paper = fui::Paint::solid(inverse ? fui::Color::Black : fui::Color::White);
  const auto point = [&](int x, int y) {
    return fui::Point{int16_t(r.x + x * r.width / 48), int16_t(r.y + y * r.height / 48)};
  };
  const auto fill = [&](int x, int y, int width, int height, fui::Paint paint, int radius) {
    target.fill(
        fui::makeRect(r.x + x * r.width / 48, r.y + y * r.height / 48, width * r.width / 48, height * r.height / 48),
        paint, radius * std::min(r.width, r.height) / 48);
  };
  const auto triangle = [&](fui::Point a, fui::Point b, fui::Point c, fui::Paint paint) {
    if (a.y > b.y) std::swap(a, b);
    if (b.y > c.y) std::swap(b, c);
    if (a.y > b.y) std::swap(a, b);
    if (a.y == c.y) return;
    for (int y = a.y; y <= c.y; ++y) {
      const int first = a.x + (c.x - a.x) * (y - a.y) / (c.y - a.y);
      const int second = y < b.y      ? a.x + (b.x - a.x) * (y - a.y) / (b.y - a.y)
                         : c.y == b.y ? b.x
                                      : b.x + (c.x - b.x) * (y - b.y) / (c.y - b.y);
      target.fill(fui::makeRect(std::min(first, second), y, std::abs(second - first) + 1, 1), paint);
    }
  };
  if (hunt) {
    triangle(point(3, 3), point(8, 25), point(20, 17), ink);
    triangle(point(45, 3), point(40, 25), point(28, 17), ink);
    triangle(point(7, 9), point(10, 19), point(15, 16), paper);
    triangle(point(41, 9), point(38, 19), point(33, 16), paper);
  } else {
    triangle(point(9, 2), point(5, 25), point(23, 17), ink);
    triangle(point(39, 2), point(43, 25), point(25, 17), ink);
    triangle(point(11, 8), point(10, 18), point(17, 16), paper);
    triangle(point(37, 8), point(38, 18), point(31, 16), paper);
  }
  fill(7, 15, 34, 26, ink, 11);
  triangle(point(7, 28), point(24, 46), point(41, 28), ink);
  if (!hunt) {
    triangle(point(4, 27), point(11, 26), point(9, 35), ink);
    triangle(point(44, 27), point(37, 26), point(39, 35), ink);
  }
  triangle(point(12, 23), point(21, 26), point(13, 29), paper);
  triangle(point(36, 23), point(27, 26), point(35, 29), paper);
  fill(13, 31, 22, 10, paper, 5);
  triangle(point(20, 32), point(28, 32), point(24, 36), ink);
  target.line(point(24, 35), point(24, 39), std::max(1, int(r.width) / 24), ink);
  if (hunt) {
    triangle(point(16, 32), point(19, 32), point(17, 38), ink);
    triangle(point(32, 32), point(29, 32), point(31, 38), ink);
  }
}
inline void bow(freeink::ui::DrawTarget& target, freeink::ui::Rect r) {
  namespace fui = freeink::ui;
  const auto ink = fui::Paint::solid(fui::Color::Black);
  const auto paper = fui::Paint::solid(fui::Color::White);
  const int side = std::min(r.width, r.height);
  const auto point = [&](int x, int y) {
    return fui::Point{int16_t(r.x + (r.width - side) / 2 + x * side / 64),
                      int16_t(r.y + (r.height - side) / 2 + y * side / 64)};
  };
  static constexpr uint8_t LIMB[][2] = {{12, 7}, {24, 10}, {34, 16}, {43, 25}, {49, 36}, {52, 47}, {51, 57}};
  const int edge = std::max(1, side / 28);
  target.line(point(12, 7), point(17, 45), edge, ink);
  target.line(point(17, 45), point(51, 57), edge, ink);
  for (int layer = 0; layer < 2; ++layer)
    for (unsigned i = 1; i < sizeof(LIMB) / sizeof(LIMB[0]); ++i)
      target.line(point(LIMB[i - 1][0], LIMB[i - 1][1]), point(LIMB[i][0], LIMB[i][1]), layer == 0 ? edge * 3 : edge,
                  layer == 0 ? ink : paper);
  target.line(point(9, 54), point(55, 12), edge * 2, ink);
  target.line(point(5, 51), point(8, 44), edge, ink);
  target.line(point(8, 44), point(18, 43), edge, ink);
  target.line(point(18, 43), point(18, 52), edge, ink);
  target.line(point(18, 52), point(11, 59), edge, ink);
  target.line(point(11, 59), point(10, 54), edge, ink);
  target.line(point(10, 54), point(5, 51), edge, ink);
  const auto tip = point(60, 7), upper = point(47, 11), lower = point(55, 20);
  if (tip.y < lower.y)
    for (int y = tip.y; y <= lower.y; ++y) {
      const int first = tip.x + (lower.x - tip.x) * (y - tip.y) / (lower.y - tip.y);
      const int second = y < upper.y          ? tip.x + (upper.x - tip.x) * (y - tip.y) / (upper.y - tip.y)
                         : upper.y == lower.y ? upper.x
                                              : upper.x + (lower.x - upper.x) * (y - upper.y) / (lower.y - upper.y);
      target.fill(fui::makeRect(std::min(first, second), y, std::abs(second - first) + 1, 1), ink);
    }
  target.line(point(6, 24), point(9, 26), edge, ink);
  target.line(point(5, 33), point(8, 33), edge, ink);
  target.line(point(58, 41), point(61, 41), edge, ink);
}
inline void chevron(freeink::ui::DrawTarget& target, freeink::ui::Rect r, bool inverse = false) {
  const auto ink = freeink::ui::Paint::solid(inverse ? freeink::ui::Color::White : freeink::ui::Color::Black);
  target.line({int16_t(r.x + r.width / 3), r.y}, {int16_t(r.right() - r.width / 3), int16_t(r.y + r.height / 2)}, 2,
              ink);
  target.line({int16_t(r.right() - r.width / 3), int16_t(r.y + r.height / 2)},
              {int16_t(r.x + r.width / 3), int16_t(r.bottom())}, 2, ink);
}
inline void swords(freeink::ui::DrawTarget& target, freeink::ui::Rect r, bool inverse = false) {
  namespace fui = freeink::ui;
  const auto ink = freeink::ui::Paint::solid(inverse ? freeink::ui::Color::White : freeink::ui::Color::Black);
  const auto paper = fui::Paint::solid(inverse ? fui::Color::Black : fui::Color::White);
  const auto point = [&](int x, int y) {
    return freeink::ui::Point{int16_t(r.x + x * r.width / 32), int16_t(r.y + y * r.height / 32)};
  };
  const int weight = std::max(2, int(r.width) / 8);
  const auto line = [&](int x, int y, int xx, int yy) { target.line(point(x, y), point(xx, yy), weight, ink); };
  target.line(point(12, 20), point(26, 6), weight + 2, paper);
  line(9, 19, 24, 4);
  line(24, 4, 29, 3);
  line(29, 3, 28, 9);
  line(28, 9, 15, 24);
  line(7, 18, 15, 26);
  line(10, 22, 4, 28);
  line(3, 28, 5, 29);
}
inline void starTriangle(freeink::ui::DrawTarget& target, freeink::ui::Point a, freeink::ui::Point b,
                         freeink::ui::Point c, freeink::ui::Paint ink) {
  const freeink::ui::Point points[] = {a, b, c};
  const auto clip = target.clipRect();
  const int top = std::max(int(clip.y), std::min({int(a.y), int(b.y), int(c.y)}));
  const int bottom = std::min(int(clip.bottom()) - 1, std::max({int(a.y), int(b.y), int(c.y)}));
  for (int y = top; y <= bottom; ++y) {
    int nodes[3], count = 0;
    for (int i = 0, j = 2; i < 3; j = i++) {
      const auto from = points[i], to = points[j];
      if ((from.y < y && to.y >= y) || (to.y < y && from.y >= y))
        nodes[count++] = from.x + (y - from.y) * (to.x - from.x) / (to.y - from.y);
    }
    if (count != 2) continue;
    const int left = std::max(int(clip.x), std::min(nodes[0], nodes[1]));
    const int right = std::min(int(clip.right()) - 1, std::max(nodes[0], nodes[1]));
    if (left <= right) target.fill(freeink::ui::makeRect(left, y, right - left + 1, 1), ink);
  }
}
inline void star(freeink::ui::DrawTarget& target, freeink::ui::Rect box, bool filled = true, bool inverse = false,
                 bool halo = false) {
  namespace fui = freeink::ui;
  const auto ink = fui::Paint::solid(inverse ? fui::Color::White : fui::Color::Black);
  static constexpr int8_t points[10][2] = {{0, -11}, {3, -4},  {11, -3}, {5, 2},    {7, 10},
                                           {0, 6},   {-7, 10}, {-5, 2},  {-11, -3}, {-3, -4}};
  const int side = std::min(box.width, box.height), x = box.x + box.width / 2, y = box.y + box.height / 2;
  const fui::Point center{int16_t(x), int16_t(y)};
  if (halo)
    for (int n = 0; n < 10; ++n) {
      const int next = (n + 1) % 10;
      target.line({int16_t(x + points[n][0] * side / 26), int16_t(y + points[n][1] * side / 26)},
                  {int16_t(x + points[next][0] * side / 26), int16_t(y + points[next][1] * side / 26)}, 4,
                  fui::Paint::solid(fui::Color::White));
    }
  for (int n = 0; n < 10; ++n) {
    const int next = (n + 1) % 10;
    const fui::Point a{int16_t(x + points[n][0] * side / 26), int16_t(y + points[n][1] * side / 26)};
    const fui::Point b{int16_t(x + points[next][0] * side / 26), int16_t(y + points[next][1] * side / 26)};
    if (filled)
      starTriangle(target, center, a, b, ink);
    else
      target.line(a, b, 2, ink);
  }
}
inline void coin(freeink::ui::DrawTarget& target, freeink::ui::Rect r) {
  namespace fui = freeink::ui;
  const auto ink = fui::Paint::solid(fui::Color::Black);
  const int side = std::max(0, std::min(r.width, r.height) - 2);
  const auto ring = fui::makeRect(r.x + (r.width - side) / 2, r.y + (r.height - side) / 2, side, side);
  target.stroke(ring, ink, std::max(2, side / 9), side / 2);
  const auto point = [&](int x, int y) {
    return fui::Point{int16_t(ring.x + x * side / 32), int16_t(ring.y + y * side / 32)};
  };
  constexpr uint8_t STAR[][2] = {{16, 7}, {19, 13}, {25, 16}, {19, 19}, {16, 25}, {13, 19}, {7, 16}, {13, 13}, {16, 7}};
  for (unsigned i = 1; i < sizeof(STAR) / sizeof(STAR[0]); ++i)
    target.line(point(STAR[i - 1][0], STAR[i - 1][1]), point(STAR[i][0], STAR[i][1]), std::max(1, side / 12), ink);
}
inline void book(freeink::ui::DrawTarget& target, freeink::ui::Rect r) {
  namespace fui = freeink::ui;
  const auto ink = fui::Paint::solid(fui::Color::Black);
  const auto point = [&](int x, int y) {
    return fui::Point{int16_t(r.x + x * r.width / 32), int16_t(r.y + y * r.height / 32)};
  };
  const int weight = std::max(2, int(r.width) / 11);
  constexpr uint8_t PAGE[][2] = {{16, 7}, {11, 4}, {6, 4}, {3, 5}, {3, 25}, {8, 24}, {12, 25}, {16, 28}};
  for (unsigned i = 1; i < sizeof(PAGE) / sizeof(PAGE[0]); ++i) {
    target.line(point(PAGE[i - 1][0], PAGE[i - 1][1]), point(PAGE[i][0], PAGE[i][1]), weight, ink);
    target.line(point(32 - PAGE[i - 1][0], PAGE[i - 1][1]), point(32 - PAGE[i][0], PAGE[i][1]), weight, ink);
  }
  target.line(point(16, 7), point(16, 28), weight, ink);
}
inline void flame(freeink::ui::DrawTarget& target, freeink::ui::Rect r) {
  target.bitmap(r, freeink::ui::bitmapFromIcon(extra_reading_flame_64), freeink::ui::BitmapMode::Contain,
                freeink::ui::Paint::solid(freeink::ui::Color::Black));
}
}  // namespace questicons
