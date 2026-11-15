#pragma once

#include <FreeInkUI.h>

namespace petui {
// Strengthen dark artwork contours, keeping paper and light fills intact.
class DecorationInk final : public freeink::ui::DrawTarget {
 public:
  explicit DecorationInk(freeink::ui::DrawTarget& target) : target(target) {}
  freeink::ui::Rect clipRect() const override { return target.clipRect(); }
  bool setClipRect(freeink::ui::Rect r) override { return target.setClipRect(r); }
  freeink::ui::Size measureText(freeink::ui::FontId font, const char* text,
                                freeink::ui::TextStyle style) const override {
    return target.measureText(font, text, style);
  }
  int16_t lineHeight(freeink::ui::FontId font) const override { return target.lineHeight(font); }
  freeink::ui::Rect inkBounds(freeink::ui::FontId font, const char* text, freeink::ui::TextStyle style) const override {
    return target.inkBounds(font, text, style);
  }
  void fill(freeink::ui::Rect r, freeink::ui::Paint paint, uint8_t radius = 0,
            uint8_t corners = freeink::ui::CornersAll) override {
    using Kind = freeink::ui::PaintKind;
    if ((paint.kind == Kind::Solid || paint.kind == Kind::Dither) && paint.color == freeink::ui::Color::DarkGray)
      paint.color = freeink::ui::Color::Black;
    target.fill(r, paint, radius, corners);
  }
  void stroke(freeink::ui::Rect r, freeink::ui::Paint paint, uint8_t width, uint8_t radius = 0,
              uint8_t corners = freeink::ui::CornersAll) override {
    target.stroke(r, paint, width, radius, corners);
  }
  void line(freeink::ui::Point a, freeink::ui::Point b, uint8_t width, freeink::ui::Paint paint) override {
    target.line(a, b, width, paint);
  }
  void triangle(freeink::ui::Point a, freeink::ui::Point b, freeink::ui::Point c, freeink::ui::Paint paint) override {
    target.triangle(a, b, c, paint);
  }
  void text(freeink::ui::Rect r, const char* text, freeink::ui::TextStyle style) override {
    target.text(r, text, style);
  }
  void bitmap(freeink::ui::Rect r, freeink::ui::BitmapRef ref, freeink::ui::BitmapMode mode,
              freeink::ui::Paint paint = freeink::ui::Paint::solid(freeink::ui::Color::Black),
              freeink::ui::Rotation rotation = freeink::ui::Rotation::None) override {
    target.bitmap(r, ref, mode, paint, rotation);
  }

 private:
  freeink::ui::DrawTarget& target;
};
}  // namespace petui
