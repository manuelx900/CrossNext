#pragma once
#include <FreeInkUIIcon.h>

#include "QuestKeepsakeBitmaps.h"
namespace questkeepsake {
inline void draw(freeink::ui::DrawTarget& target, freeink::ui::Rect r, int id) {
  if (id < 0 || unsigned(id) >= sizeof(questkeepsakeart::REQUEST_ITEM)) return;
  target.bitmap(r, freeink::ui::bitmapFromIcon(*questkeepsakeart::ITEMS[questkeepsakeart::REQUEST_ITEM[id]]),
                freeink::ui::BitmapMode::Contain, freeink::ui::Paint::solid(freeink::ui::Color::Black));
}
}  // namespace questkeepsake
