#pragma once

#include <algorithm>

#include "PetScreens.h"

namespace petui {
inline void farewellPicture(toybox::Screen& screen, const Model& m, fui::Rect area, bool inverse = false) {
  if (area.width < 8 || area.height < 8) return;
  const unsigned mask = unsigned(m.profile->species) * 2;
  if (m.farewellArt.draw && m.farewellArt.draw(m.farewellArt.context, screen.target(), area, mask, !inverse) &&
      m.farewellArt.draw(m.farewellArt.context, screen.target(), area, mask + 1, inverse))
    return;
  // A missing optional pack must still distinguish death from sleep.
  const auto ink = fui::Paint::solid(inverse ? fui::Color::White : fui::Color::Black),
             paper = fui::Paint::solid(inverse ? fui::Color::Black : fui::Color::White);
  const int height = std::min(110, int(area.height) - 4), width = std::min(int(area.width) - 4, height * 3 / 4);
  const auto stone =
      fui::makeRect(area.x + (area.width - width) / 2, area.y + (area.height - height) / 2, width, height);
  screen.target().fill(stone, paper);
  screen.target().stroke(stone, ink, 2, std::min(12, width / 4), fui::CornersTop);
  for (int row : {1, 2}) {
    const int y = stone.y + stone.height * row / 4;
    screen.target().line({int16_t(stone.x + width / 4), int16_t(y)}, {int16_t(stone.right() - width / 4), int16_t(y)},
                         2, ink);
  }
  screen.target().line({int16_t(stone.x - 2), stone.bottom()}, {int16_t(stone.right() + 2), stone.bottom()}, 2, ink);
}
}  // namespace petui
