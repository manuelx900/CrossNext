#pragma once

#include "PetReferenceIcons.h"
#include "PetWidgets.h"

namespace petui::widget {
inline void sparkle(toybox::Screen& s, int x, int y, int size, bool inverse = false) {
  const int half = size / 2, neck = std::max(1, size / 6);
  const fui::Point points[] = {{int16_t(x), int16_t(y - half)}, {int16_t(x + neck), int16_t(y - neck)},
                               {int16_t(x + half), int16_t(y)}, {int16_t(x + neck), int16_t(y + neck)},
                               {int16_t(x), int16_t(y + half)}, {int16_t(x - neck), int16_t(y + neck)},
                               {int16_t(x - half), int16_t(y)}, {int16_t(x - neck), int16_t(y - neck)}};
  for (unsigned i = 0; i < 8; ++i) s.target().line(points[i], points[(i + 1) % 8], 2, inverse ? paper : ink);
}
inline void celebrationBanner(toybox::Screen& s, fui::Rect r, const char* title, const freeink::Icon& mark,
                              bool inverse = false, bool checked = true) {
  if (inverse) s.target().fill(r, ink);
  const int side = std::min(38, int(r.height) - 12);
  icon(s, rect(r.x + 10, r.y + (r.height - side) / 2, side, side), mark, inverse);
  text(s, rect(r.x + side + 24, r.y + 4, r.width - side - (checked ? 76 : 34), r.height - 8), title, 1, inverse,
       fui::TextAlign::Center);
  if (checked) icon(s, rect(r.right() - 42, r.y + (r.height - 28) / 2, 28, 28), extra_checked_32, inverse);
  if (!inverse) s.target().line({r.x, r.bottom()}, {r.right(), r.bottom()}, 2, ink);
}
enum class Celebration { Joy, Clean, Fed };
inline void celebrationPicture(toybox::Screen& s, const Model& m, fui::Rect r, bool joyful = true,
                               Celebration scene = Celebration::Joy) {
  const auto& p = *m.profile;
  if (!joyful || p.illness != pet::Illness::Healthy || p.resting || p.lifeStatus != pet::LifeStatus::Active ||
      pet::stage(p.readingSeconds) == pet::Stage::Egg) {
    creature(s, m, r);
    return;
  }
  if (pet::stage(p.readingSeconds) == pet::Stage::Adult) {
    if (p.species == pet::Species::Silva) {
      icon(s, r, scene == Celebration::Fed ? referenceicons::SILVA_FED : referenceicons::SILVA_CELEBRATION);
      return;
    }
    if (scene == Celebration::Clean && p.species == pet::Species::Ember) {
      icon(s, r, referenceicons::EMBER_CLEAN);
      return;
    }
  }
  static constexpr fui::Point stars[] = {{9, 27}, {18, 77}, {28, 6}, {80, 16}, {94, 48}, {85, 82}};
  for (unsigned i = 0; i < 6; ++i)
    sparkle(s, r.x + stars[i].x * r.width / 100, r.y + stars[i].y * r.height / 100, i % 2 ? 13 : 19);
  const auto image = r.inset({18, 27, 5, 27});
  const auto age = pet::stage(p.readingSeconds);
  const unsigned body = petsleep::bodyIndex(p.species, age, petsleep::Pose::Happy, 0);
  if (!m.sleepArt.draw || !m.sleepArt.draw(m.sleepArt.context, s.target(), image, body + 1, false))
    art(s, m, image, 3 + unsigned(p.species) * 16 + (unsigned(age) - 1) * 4 + 1, speciesIcon(p.species), false);
  icon(s, rect(r.right() - 56, r.y + 4, 32, 32), icon_hearts_32);
}
}  // namespace petui::widget
