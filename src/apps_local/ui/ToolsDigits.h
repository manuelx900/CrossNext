#pragma once
#include "NumberTypography.h"
#include "ToyboxScreen.h"

// Tools and statistics use the same Jersey numerals as the firmware clock.
namespace toolsdigits {
inline constexpr auto& glyphs = numerals::glyphs;
inline constexpr auto& bits = numerals::bits;
inline constexpr auto face = numerals::defaultFace;
inline void draw(toybox::Screen& screen, freeink::ui::Rect box, const char* value) {
  numerals::draw(screen.target(), box, value);
}
}  // namespace toolsdigits
