#pragma once

#include <FreeInkUICore.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>

namespace petinput {
struct PuzzleLayout {
  freeink::ui::Rect frame{};
  int16_t side = 0;
  static constexpr int GAP = 8, INSET = 10;
  freeink::ui::Rect tile(unsigned index) const {
    return freeink::ui::makeRect(frame.x + INSET + int(index % 3) * (side + GAP),
                                 frame.y + INSET + int(index / 3) * (side + GAP), side, side);
  }
  int at(int x, int y) const {
    if (side <= 0) return -1;
    for (unsigned i = 0; i < 9; ++i) {
      const auto box = tile(i);
      if (x >= box.x && x < box.right() && y >= box.y && y < box.bottom()) return int(i);
    }
    return -1;
  }
  int slide(const uint8_t* board, int x, int y, int dx, int dy) const {
    const int from = at(x, y);
    if (from < 0 || !board[from] || std::abs(dx) + std::abs(dy) != 1) return -1;
    const int row = from / 3 + dy, column = from % 3 + dx;
    if (row < 0 || row >= 3 || column < 0 || column >= 3 || board[row * 3 + column]) return -1;
    return from;
  }
};

inline PuzzleLayout puzzleLayout(freeink::ui::Rect area, bool wide) {
  const int room = area.height - (wide ? 0 : 76);
  const int side = std::max(0, (std::min(int(area.width), room) - PuzzleLayout::INSET * 2 - PuzzleLayout::GAP * 2) / 3);
  const int extent = side * 3 + PuzzleLayout::INSET * 2 + PuzzleLayout::GAP * 2;
  return {freeink::ui::makeRect(area.x + (area.width - extent) / 2, area.y + (room - extent) / 2, extent, extent),
          int16_t(side)};
}

// Main-loop-owned gesture, retained while the renderer holds RenderLock.
class PendingSlide {
 public:
  void begin(int x, int y, uint32_t revision) {
    if (pending) return;
    contact = true;
    startX = int16_t(x);
    startY = int16_t(y);
    view = revision;
  }
  void end(int dx, int dy, uint32_t revision) {
    if (contact && view == revision && (dx || dy) && !pending) {
      stepX = int8_t(dx);
      stepY = int8_t(dy);
      pending = true;
    }
    contact = false;
  }
  bool take(uint32_t revision, int& x, int& y, int& dx, int& dy) {
    if (!pending) return false;
    pending = false;
    if (view != revision) return false;
    x = startX;
    y = startY;
    dx = stepX;
    dy = stepY;
    return true;
  }
  void clear() { contact = pending = false; }

 private:
  uint32_t view = 0;
  int16_t startX = 0, startY = 0;
  int8_t stepX = 0, stepY = 0;
  bool contact = false, pending = false;
};
}  // namespace petinput
