#pragma once

#include <cstdint>
#include <memory>

class GfxRenderer;
class SdCardFont;

class LanguageUiFonts {
 public:
  LanguageUiFonts();
  ~LanguageUiFonts();
  void begin(GfxRenderer& renderer);
  bool load(GfxRenderer& renderer, int fontId);
  static bool supportsText(const GfxRenderer& renderer, const char* text, uint8_t points);

 private:
  // Only coverage stays resident; the shared font cache owns rebuildable glyphs.
  std::unique_ptr<SdCardFont> fonts[3];
  uint8_t attempted = 0;
};
