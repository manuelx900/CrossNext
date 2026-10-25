#pragma once

#include <FreeInkUI.h>

#include "StudySizedText.h"

namespace study {
// Only content slots use the local size policy; UI chrome delegates.
class SizedDrawTarget final : public freeink::ui::DrawTarget {
  using FontId = freeink::ui::FontId;
  using Rect = freeink::ui::Rect;
  using Point = freeink::ui::Point;
  using Paint = freeink::ui::Paint;
  using TextStyle = freeink::ui::TextStyle;
  const GfxRenderer& renderer;
  freeink::ui::DrawTarget& base;
  int questionFont = 0, answerFont = 0;
  uint8_t questionSize = 1, answerSize = 0;
  int font(FontId slot) const { return slot == 2 ? questionFont : answerFont; }
  bool scaled(FontId slot) const { return slot == 2 || slot == 4; }
  SizedTextTarget adapter(FontId slot) const {
    return SizedTextTarget(renderer, slot == 2 ? questionSize : answerSize, 0);
  }
  static EpdFontFamily::Style face(TextStyle style) {
    return static_cast<EpdFontFamily::Style>((style.bold ? EpdFontFamily::BOLD : 0) |
                                             (style.italic ? EpdFontFamily::ITALIC : 0));
  }

 public:
  SizedDrawTarget(const GfxRenderer& renderer, freeink::ui::DrawTarget& base) : renderer(renderer), base(base) {}
  void bind(int questionFont, int answerFont, uint8_t questionSize, uint8_t answerSize) {
    this->questionFont = questionFont;
    this->answerFont = answerFont;
    this->questionSize = questionSize;
    this->answerSize = answerSize;
  }
  Rect clipRect() const override { return base.clipRect(); }
  bool setClipRect(Rect r) override { return base.setClipRect(r); }
  freeink::ui::Size measureText(FontId slot, const char* text, TextStyle style) const override {
    if (!scaled(slot)) return base.measureText(slot, text, style);
    const auto target = adapter(slot);
    return {int16_t(target.getTextWidth(font(slot), text, face(style))), int16_t(target.getLineHeight(font(slot)))};
  }
  int16_t lineHeight(FontId slot) const override {
    return scaled(slot) ? adapter(slot).getLineHeight(font(slot)) : base.lineHeight(slot);
  }
  void fill(Rect r, Paint p, uint8_t radius = 0, uint8_t corners = freeink::ui::CornersAll) override {
    base.fill(r, p, radius, corners);
  }
  void stroke(Rect r, Paint p, uint8_t w, uint8_t radius = 0, uint8_t corners = freeink::ui::CornersAll) override {
    base.stroke(r, p, w, radius, corners);
  }
  void line(Point a, Point b, uint8_t w, Paint p) override { base.line(a, b, w, p); }
  void triangle(Point a, Point b, Point c, Paint p) override { base.triangle(a, b, c, p); }
  void bitmap(Rect r, freeink::ui::BitmapRef image, freeink::ui::BitmapMode mode, Paint p,
              freeink::ui::Rotation rotation = freeink::ui::Rotation::None) override {
    base.bitmap(r, image, mode, p, rotation);
  }
  void text(Rect r, const char* text, TextStyle style) override {
    if (!scaled(style.font)) {
      base.text(r, text, style);
      return;
    }
    const auto target = adapter(style.font);
    freeink::ui::layoutText(*this, r, text, style, [&](const char* run, Rect line) {
      int x = line.x;
      const int width = target.getTextWidth(font(style.font), run, face(style));
      if (style.align == freeink::ui::TextAlign::Center)
        x += (line.width - width) / 2;
      else if (style.align == freeink::ui::TextAlign::Right)
        x += line.width - width;
      target.drawText(font(style.font), x, line.y, run, style.color != freeink::ui::Color::White, face(style));
    });
  }
};
}  // namespace study
