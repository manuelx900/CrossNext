#pragma once
#include <FreeInkUI.h>

#include "PieceDesign.h"
#include "ReferencePieces.h"

namespace chessstyle {
inline void draw(freeink::ui::DrawTarget& target, freeink::ui::Rect r, int type, bool dark, Design design = kDefault) {
  (void)design;
  if (type < 1 || type > 6) return;
  const auto& piece = chessreference::pieces[0][dark ? 1 : 0][type - 1];
  freeink::ui::BitmapRef bitmap{piece.mask, 64, 64, freeink::ui::BitmapFormat::BW1};
  target.bitmap(r, bitmap, freeink::ui::BitmapMode::Contain, freeink::ui::Paint::solid(freeink::ui::Color::White));
  bitmap.data = piece.ink;
  target.bitmap(r, bitmap, freeink::ui::BitmapMode::Contain, freeink::ui::Paint::solid(freeink::ui::Color::Black));
}
}  // namespace chessstyle
