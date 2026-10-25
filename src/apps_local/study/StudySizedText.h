#pragma once

#include <GfxRenderer.h>

#include <algorithm>

#include "../flashcards/FlashTextSize.h"

namespace study {
// The wrapper and rasterizer share the same scale for a deck's fixed-size fonts.
class SizedTextTarget {
 public:
  SizedTextTarget(const GfxRenderer& renderer, uint8_t choice, int rubyFont, bool enabled = true)
      : renderer(renderer), choice(flashcards::validTextSize(choice)), rubyFont(rubyFont), enabled(enabled) {}
  int getScreenWidth() const { return renderer.getScreenWidth(); }
  int getTextWidth(int font, const char* text, EpdFontFamily::Style style = EpdFontFamily::REGULAR) const {
    return scaled(renderer.getTextWidth(font, text, style), font);
  }
  int getTextHeight(int font) const { return scaled(renderer.getTextHeight(font), font); }
  int getLineHeight(int font) const { return scaled(renderer.getLineHeight(font), font); }
  void drawText(int font, int x, int y, const char* text, bool black,
                EpdFontFamily::Style style = EpdFontFamily::REGULAR) const {
    const auto factor = scale(font);
    if (factor == 256)
      renderer.drawText(font, x, y, text, black, style);
    else
      renderer.drawTextScaledBw(font, x, y, text, factor, black, style);
  }
  void fillRect(int x, int y, int w, int h, bool black) const { renderer.fillRect(x, y, w, h, black); }

 private:
  const GfxRenderer& renderer;
  uint8_t choice;
  int rubyFont;
  bool enabled;
  uint16_t scale(int font) const {
    if (!enabled) return 256;
    const int source = std::max(1, renderer.getLineHeight(font));
    int height = renderer.getLineHeight(flashcards::textSizeFont(choice));
    if (font == rubyFont) height = std::max(1, height / 2);
    return std::clamp((height * 256 + source / 2) / source, 32, 1024);
  }
  int scaled(int value, int font) const { return (value * int(scale(font)) + 255) / 256; }
};
}  // namespace study
