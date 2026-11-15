#pragma once

#include <FreeInkUI.h>

namespace petui {
// Rasterize a complete absolute gray plane through the ordinary monochrome target.
// Later white surfaces must erase earlier gray pixels in both planes.
class GrayTarget final : public freeink::ui::DrawTarget {
 public:
  GrayTarget(freeink::ui::DrawTarget& target, bool msb) : target(target), msb(msb) {}
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
    target.fill(r, plane(paint), radius, corners);
  }
  void stroke(freeink::ui::Rect r, freeink::ui::Paint paint, uint8_t width, uint8_t radius = 0,
              uint8_t corners = freeink::ui::CornersAll) override {
    target.stroke(r, plane(paint), width, radius, corners);
  }
  void line(freeink::ui::Point a, freeink::ui::Point b, uint8_t width, freeink::ui::Paint paint) override {
    target.line(a, b, width, plane(paint));
  }
  void triangle(freeink::ui::Point a, freeink::ui::Point b, freeink::ui::Point c, freeink::ui::Paint paint) override {
    target.triangle(a, b, c, plane(paint));
  }
  void text(freeink::ui::Rect r, const char* text, freeink::ui::TextStyle style) override {
    style.color = color(style.inverted ? freeink::ui::Color::White : style.color);
    style.inverted = false;
    target.text(r, text, style);
  }
  void bitmap(freeink::ui::Rect r, freeink::ui::BitmapRef ref, freeink::ui::BitmapMode mode,
              freeink::ui::Paint paint = freeink::ui::Paint::solid(freeink::ui::Color::Black),
              freeink::ui::Rotation rotation = freeink::ui::Rotation::None) override {
    // PET's packed gray artwork emits colored runs; native symbols use monochrome masks.
    target.bitmap(r, ref, mode, plane(paint), rotation);
  }

 private:
  freeink::ui::DrawTarget& target;
  bool msb;
  freeink::ui::Color color(freeink::ui::Color value) const {
    using C = freeink::ui::Color;
    if (value == C::Transparent) return value;
    return value == C::White || value == (msb ? C::LightGray : C::DarkGray) ? C::White : C::Black;
  }
  freeink::ui::Paint plane(freeink::ui::Paint paint) const {
    using K = freeink::ui::PaintKind;
    if (paint.kind == K::Solid || paint.kind == K::Dither) return freeink::ui::Paint::solid(color(paint.color));
    return paint;
  }
};
}  // namespace petui
