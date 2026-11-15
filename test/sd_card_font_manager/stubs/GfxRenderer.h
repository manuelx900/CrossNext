#pragma once
#include <EpdFontFamily.h>

#include <map>
class SdCardFont;
class GfxRenderer {
 public:
  std::map<int, EpdFontFamily> fonts;
  std::map<int, SdCardFont*> sd;
  const auto& getFontMap() const { return fonts; }
  void insertFont(int id, const EpdFontFamily& family) { fonts[id] = family; }
  void registerSdCardFont(int id, SdCardFont* font) { sd[id] = font; }
  void removeFont(int id) {
    fonts.erase(id);
    sd.erase(id);
  }
  void clearSdCardFonts() { sd.clear(); }
  void clearFallbackFonts() {}
};
