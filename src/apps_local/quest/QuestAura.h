#pragma once
#include "QuestAssets.h"
namespace questaura {
inline void draw(freeink::ui::DrawTarget& target, freeink::ui::Rect area, int index, sdart::Pack& assets) {
  if (index < 0 || index >= 50 || area.empty()) return;
  assets.draw(target, area, questassets::AURA_MASKS + index, freeink::ui::BitmapMode::Contain,
              freeink::ui::Color::White);
  assets.draw(target, area, questassets::AURAS + index);
}
}  // namespace questaura
