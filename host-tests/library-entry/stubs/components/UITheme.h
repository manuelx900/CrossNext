#pragma once
#include <FreeInkUIIcon.h>
struct Rect {
  int x, y, width, height;
};
class GfxRenderer {
 public:
  freeink::ui::DrawTarget& target;
  mutable int placeholders = 0;
  explicit GfxRenderer(freeink::ui::DrawTarget& value) : target(value) {}
  void fillRect(int x, int y, int w, int h, bool black) const {
    target.fill(freeink::ui::makeRect(x, y, w, h),
                freeink::ui::Paint::solid(black ? freeink::ui::Color::Black : freeink::ui::Color::White));
  }
  void drawRect(int x, int y, int w, int h, bool) const {
    target.stroke(freeink::ui::makeRect(x, y, w, h), freeink::ui::Paint::solid(freeink::ui::Color::Black), 1);
  }
  void drawIcon(const uint8_t* bits, int x, int y, int side) const {
    target.bitmap(freeink::ui::makeRect(x, y, side, side),
                  {bits, uint16_t(side), uint16_t(side), freeink::ui::BitmapFormat::Mask1},
                  freeink::ui::BitmapMode::Contain, freeink::ui::Paint::solid(freeink::ui::Color::Black),
                  freeink::ui::Rotation::CW90);
  }
};
struct FakeTheme {
  static void drawCoverPlaceholder(const GfxRenderer&, Rect);
};
inline FakeTheme GUI;
