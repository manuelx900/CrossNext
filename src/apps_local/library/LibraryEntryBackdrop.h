#pragma once
#include <FreeInkUI.h>

class GfxRenderer;
namespace libraryui {
inline void coverFrame(freeink::ui::DrawTarget& target, freeink::ui::Rect card) {
  target.stroke(card, freeink::ui::Paint::solid(freeink::ui::Color::Black), 1);
}
freeink::ui::Rect entryBackdrop(const GfxRenderer& renderer, freeink::ui::DrawTarget& target,
                                const freeink::ui::DeviceContext& device, freeink::ui::Rect body, bool covers,
                                int bookCount = -1, int selected = 0);
}  // namespace libraryui
