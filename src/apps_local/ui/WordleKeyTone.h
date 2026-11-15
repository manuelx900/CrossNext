#pragma once

#include <FreeInkUI.h>

namespace wordlekeys {
enum class Tone : uint8_t { Normal, Absent, Present, Correct };

inline void fadeAbsent(freeink::ui::DrawTarget& target, freeink::ui::Rect box,
                       freeink::ui::Paint background = freeink::ui::Paint::solid(freeink::ui::Color::White)) {
  // Restore alternating background rows to fade binary text without a new buffer.
  for (int y = box.y + 1; y < box.bottom(); y += 2)
    target.fill(freeink::ui::makeRect(box.x, y, box.width, 1), background);
}
}  // namespace wordlekeys
