#pragma once
#include <FreeInkUIIcon.h>

#include "../ui/ExtraIcons.h"
namespace questicons {
inline void book(freeink::ui::DrawTarget& target, freeink::ui::Rect r) {
  target.bitmap(r, freeink::ui::bitmapFromIcon(extra_reading_book_64), freeink::ui::BitmapMode::Contain,
                freeink::ui::Paint::solid(freeink::ui::Color::Black));
}
inline void flame(freeink::ui::DrawTarget& target, freeink::ui::Rect r) {
  target.bitmap(r, freeink::ui::bitmapFromIcon(extra_reading_flame_64), freeink::ui::BitmapMode::Contain,
                freeink::ui::Paint::solid(freeink::ui::Color::Black));
}
}  // namespace questicons
