#pragma once

#include <algorithm>

#include "PetHabitatData.h"
#include "PetScreens.h"
#include "PetSleepAnimation.h"

namespace petui {
// Both live views use the same full-body pose. Home alone may substitute a shelter head.
inline bool fullCreature(const Model& m, fui::DrawTarget& target, fui::Rect area, bool inverse = false,
                         bool opaqueFallback = false) {
  const auto& p = *m.profile;
  const unsigned step = petsleep::breathing(p, m.view) ? m.sleepStep : 0;
  const auto mask = petsleep::fullBodyIndex(p, step);
  if (m.sleepArt.draw && m.sleepArt.draw(m.sleepArt.context, target, area, mask, !inverse) &&
      m.sleepArt.draw(m.sleepArt.context, target, area, mask + 1, inverse))
    return true;
  const unsigned scene = pet::homeScene(p);
  if (m.habitat.draw && m.habitat.draw(m.habitat.context, target, area, 482 + scene * 2, !inverse) &&
      m.habitat.draw(m.habitat.context, target, area, 483 + scene * 2, inverse))
    return true;
  if (opaqueFallback) {
    const auto& source = PET_ART[scene];
    const int width = std::min(int(area.width), int(area.height) * source.width / source.height);
    const int height = width * source.height / source.width;
    // Cover the one-pixel difference between row-loader and bitmap contain rounding.
    const int x = area.x + (area.width - width) / 2, y = area.y + (area.height - height) / 2;
    const int left = std::max(int(area.x), x - 1), top = std::max(int(area.y), y - 1);
    const int right = std::min(int(area.right()), x + width + 1), bottom = std::min(int(area.bottom()), y + height + 1);
    target.fill(fui::makeRect(left, top, right - left, bottom - top),
                fui::Paint::solid(inverse ? fui::Color::Black : fui::Color::White));
  }
  if (m.scenes.draw && m.scenes.draw(m.scenes.context, target, area, scene, inverse)) return true;
  const unsigned age = unsigned(pet::stage(p.readingSeconds));
  const unsigned pose = unsigned(petsleep::bodyPose(p));
  const unsigned graphic =
      age ? 3 + unsigned(p.species) * 16 + (age - 1) * 4 + std::min(pose, 3u) : unsigned(p.species);
  return m.art.draw && m.art.draw(m.art.context, target, area, graphic, inverse);
}
}  // namespace petui
