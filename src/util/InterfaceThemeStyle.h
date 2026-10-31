#pragma once

#include "MenuFontStyle.h"

namespace interfacetheme {
enum class Style : uint8_t { Framed, Carousel, Picco, Licorice, Count };
enum class Override : uint8_t { General, Framed, Carousel, Picco, Licorice, Count };
constexpr Override normalized(uint8_t value) {
  return value < uint8_t(Override::Count) ? Override(value) : Override::General;
}
constexpr Style homeStyle(uint8_t value) { return value < uint8_t(Style::Count) ? Style(value) : Style::Framed; }
constexpr Style apply(Style fallback, uint8_t value) {
  const auto choice = normalized(value);
  return choice == Override::General ? fallback : Style(uint8_t(choice) - 1);
}
constexpr Style resolve(uint8_t home, uint8_t general, menufont::Section section, uint8_t overrideValue) {
  const auto base = homeStyle(home);
  if (section == menufont::Section::Home) return base;
  const auto common = apply(base, general);
  return section == menufont::Section::General ? common : apply(common, overrideValue);
}
constexpr bool light(Style style) { return style == Style::Picco || style == Style::Licorice; }
}  // namespace interfacetheme
