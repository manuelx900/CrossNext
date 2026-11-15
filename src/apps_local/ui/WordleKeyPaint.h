#pragma once

#include <algorithm>

#include "PixelMenu.h"
#include "WordleKeyTone.h"

namespace wordlekeys {
inline bool paintKey(freeink::ui::DrawTarget& target, freeink::ui::Rect box, Tone tone, bool enter = false) {
  namespace fui = freeink::ui;
  const auto ink = fui::Paint::solid(fui::Color::Black);
  const bool black = enter || tone == Tone::Correct;
  if (black)
    target.fill(box, ink);
  else if (tone == Tone::Present)
    target.fill(box, fui::Paint::dither(fui::Color::LightGray));
  target.stroke(box, ink, pixelui::kBorder);
  return black;
}

inline void paintErase(freeink::ui::DrawTarget& target, freeink::ui::Rect box) {
  namespace fui = freeink::ui;
  const auto ink = fui::Paint::solid(fui::Color::Black);
  const auto icon = fui::lucideDeleteIcon28();
  const int16_t side = std::min({int16_t(icon.width), int16_t(icon.height), box.width, box.height});
  target.bitmap(fui::centeredRect(box, {side, side}), icon, fui::BitmapMode::Contain, ink);
}
}  // namespace wordlekeys
