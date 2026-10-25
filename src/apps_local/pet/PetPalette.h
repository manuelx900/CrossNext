#pragma once

#include <FreeInkUI.h>

namespace petui {
// Normalize text to explicit ink so the device and host targets agree on inversion.
class Palette final : public freeink::ui::DrawTarget {
 public:
  Palette(freeink::ui::DrawTarget& target, bool inverted)
      : target(target), delegate(target, inverted), inverted(inverted) {}
  bool enabled() const { return inverted; }
  void setInverted(bool value) {
    inverted = value;
    delegate.setEnabled(value);
  }
  freeink::ui::Rect clipRect() const override { return target.clipRect(); }
  bool setClipRect(freeink::ui::Rect r) override { return target.setClipRect(r); }
  freeink::ui::Size measureText(freeink::ui::FontId f, const char* text, freeink::ui::TextStyle style) const override {
    return target.measureText(f, text, style);
  }
  int16_t lineHeight(freeink::ui::FontId f) const override { return target.lineHeight(f); }
  freeink::ui::Rect inkBounds(freeink::ui::FontId f, const char* text, freeink::ui::TextStyle style) const override {
    return target.inkBounds(f, text, style);
  }
  void fill(freeink::ui::Rect r, freeink::ui::Paint paint, uint8_t radius = 0,
            uint8_t corners = freeink::ui::CornersAll) override {
    delegate.fill(r, paint, radius, corners);
  }
  void stroke(freeink::ui::Rect r, freeink::ui::Paint paint, uint8_t width, uint8_t radius = 0,
              uint8_t corners = freeink::ui::CornersAll) override {
    delegate.stroke(r, paint, width, radius, corners);
  }
  void line(freeink::ui::Point a, freeink::ui::Point b, uint8_t width, freeink::ui::Paint paint) override {
    delegate.line(a, b, width, paint);
  }
  void triangle(freeink::ui::Point a, freeink::ui::Point b, freeink::ui::Point c, freeink::ui::Paint paint) override {
    delegate.triangle(a, b, c, paint);
  }
  void bitmap(freeink::ui::Rect r, freeink::ui::BitmapRef ref, freeink::ui::BitmapMode mode,
              freeink::ui::Paint paint = freeink::ui::Paint::solid(freeink::ui::Color::Black),
              freeink::ui::Rotation rotation = freeink::ui::Rotation::None) override {
    delegate.bitmap(r, ref, mode, paint, rotation);
  }
  void text(freeink::ui::Rect r, const char* text, freeink::ui::TextStyle style) override {
    if (inverted) {
      style.color = freeink::ui::invertedColor(style.inverted ? freeink::ui::Color::White : style.color);
      style.inverted = false;
    }
    target.text(r, text, style);
  }

 private:
  freeink::ui::DrawTarget& target;
  freeink::ui::InvertedDrawTarget delegate;
  bool inverted;
};
class PaletteScope {
 public:
  PaletteScope(Palette* palette, bool inverted) : palette(palette), previous(palette && palette->enabled()) {
    if (palette) palette->setInverted(inverted);
  }
  ~PaletteScope() {
    if (palette) palette->setInverted(previous);
  }
  bool inverted() const { return palette && palette->enabled(); }

 private:
  Palette* palette;
  bool previous;
};
}  // namespace petui
