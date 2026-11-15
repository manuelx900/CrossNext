#pragma once

#include <FreeInkUIIcon.h>

#include "../ui/ToyboxIcons.h"
#include "PetActionIcons.h"
#include "PetScreens.h"

namespace petui::symbols {
enum class Kind : uint8_t {
  Care,
  Food,
  Play,
  Rest,
  Decorate,
  Clean,
  Read,
  Health,
  Energy,
  Info,
  Settings,
  Album,
  Sparkles
};
inline void reference(toybox::Screen& screen, fui::Rect area, const uint8_t* pixels, bool inverse,
                      int sourceSize = actionicons::SIZE) {
  const int side = std::min(area.width, area.height);
  const int left = area.x + (area.width - side) / 2, top = area.y + (area.height - side) / 2;
  static constexpr fui::Color colors[] = {fui::Color::Black, fui::Color::DarkGray, fui::Color::LightGray,
                                          fui::Color::White};
  const auto sample = [&](int x, int y) {
    const int source = (y * sourceSize / side) * sourceSize + x * sourceSize / side;
    return (pixels[source / 4] >> (6 - (source % 4) * 2)) & 3;
  };
  for (int y = 0; y < side; ++y) {
    for (int x = 0; x < side;) {
      const int value = sample(x, y), begin = x++;
      while (x < side && sample(x, y) == value) ++x;
      if (value != 3)
        screen.target().fill(fui::makeRect(left + begin, top + y, x - begin, 1),
                             fui::Paint::dither(colors[inverse ? 3 - value : value]));
    }
  }
}
inline void draw(toybox::Screen& screen, fui::Rect area, Kind kind, bool inverse = false) {
  auto& target = screen.target();
  const int side = std::min(area.width, area.height);
  const int left = area.x + (area.width - side) / 2, top = area.y + (area.height - side) / 2;
  const auto ink = fui::Paint::solid(inverse ? fui::Color::White : fui::Color::Black);
  const auto fill = fui::Paint::dither(inverse ? fui::Color::DarkGray : fui::Color::LightGray);
  const uint8_t weight = side >= 26 ? 2 : 1;
  const auto box = [&](int x, int y, int w, int h) {
    return fui::makeRect(left + x * side / 32, top + y * side / 32, std::max(1, w * side / 32),
                         std::max(1, h * side / 32));
  };
  const auto line = [&](int x1, int y1, int x2, int y2) {
    target.line({int16_t(left + x1 * side / 32), int16_t(top + y1 * side / 32)},
                {int16_t(left + x2 * side / 32), int16_t(top + y2 * side / 32)}, weight, ink);
  };
  const auto outline = [&](const uint8_t (*points)[2], unsigned count) {
    for (unsigned n = 0; n < count; ++n) {
      const auto& a = points[n];
      const auto& b = points[(n + 1) % count];
      line(a[0], a[1], b[0], b[1]);
    }
  };
  const auto rounded = [&](int x, int y, int w, int h, int radius) {
    const auto r = box(x, y, w, h);
    const auto curve = uint8_t(std::max(1, radius * side / 32));
    target.fill(r, fill, curve);
    target.stroke(r, ink, weight, curve);
  };
  switch (kind) {
    case Kind::Care:
      reference(screen, area, actionicons::CARE, inverse);
      break;
    case Kind::Food:
      rounded(5, 17, 23, 12, 5);
      rounded(9, 10, 6, 7, 3);
      rounded(15, 8, 7, 9, 3);
      rounded(21, 12, 5, 5, 2);
      line(3, 17, 29, 17);
      break;
    case Kind::Play:
      reference(screen, area, actionicons::PLAY, inverse);
      break;
    case Kind::Rest:
      reference(screen, area, actionicons::REST, inverse);
      break;
    case Kind::Decorate:
      reference(screen, area, actionicons::DECORATE, inverse);
      break;
    case Kind::Clean:
      rounded(5, 14, 23, 15, 6);
      rounded(4, 4, 7, 7, 3);
      rounded(16, 3, 5, 5, 2);
      rounded(25, 8, 4, 4, 2);
      line(12, 20, 21, 20);
      break;
    case Kind::Read:
      rounded(2, 5, 14, 23, 2);
      rounded(17, 5, 14, 23, 2);
      line(16, 6, 16, 29);
      line(6, 11, 12, 11);
      line(21, 11, 27, 11);
      line(6, 16, 12, 16);
      line(21, 16, 27, 16);
      break;
    case Kind::Health: {
      static constexpr uint8_t heart[][2] = {{16, 9},  {12, 5},  {8, 4},   {4, 5},  {1, 9},  {1, 14}, {3, 18},
                                             {16, 30}, {29, 18}, {31, 14}, {31, 9}, {28, 5}, {24, 4}, {20, 5}};
      outline(heart, 14);
      break;
    }
    case Kind::Energy: {
      static constexpr uint8_t bolt[][2] = {{20, 1}, {5, 18}, {15, 18}, {11, 31}, {28, 12}, {18, 12}};
      outline(bolt, 6);
      break;
    }
    case Kind::Sparkles: {
      static constexpr uint8_t large[][2] = {{10, 3},  {13, 12}, {21, 16}, {13, 19},
                                             {10, 29}, {7, 19},  {0, 16},  {7, 12}};
      static constexpr uint8_t small[][2] = {{25, 0}, {27, 5}, {31, 7}, {27, 9}, {25, 14}, {23, 9}, {19, 7}, {23, 5}};
      outline(large, 8);
      outline(small, 8);
      break;
    }
    case Kind::Info:
      target.stroke(box(3, 3, 26, 26), ink, weight, uint8_t(std::max(1, 13 * side / 32)));
      target.fill(box(15, 8, 3, 3), ink, uint8_t(std::max(1, side / 32)));
      target.fill(box(15, 14, 3, 10), ink, uint8_t(std::max(1, side / 32)));
      break;
    case Kind::Settings:
      target.bitmap(area, fui::bitmapFromIcon(icon_go_settings_32), fui::BitmapMode::Contain, ink);
      break;
    case Kind::Album:
      rounded(3, 3, 27, 27, 2);
      line(7, 24, 15, 15);
      line(15, 15, 25, 24);
      rounded(21, 8, 4, 4, 2);
      break;
  }
}
}  // namespace petui::symbols
