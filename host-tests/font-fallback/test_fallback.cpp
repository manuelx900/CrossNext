#include <FontFallback.h>

#include <cassert>
#include <cstdio>

#include "builtinFonts/ubuntu_12_regular.h"
#include "fonts/toybox_30.h"

int main() {
  const EpdFont pixel(&toybox_30), ubuntu(&ubuntu_12_regular);
  for (const auto* face : {&pixel, &ubuntu}) {
    for (const uint32_t cp : {0x2641u, 0x1F728u}) {
      EpdGlyph fallback;
      const auto* glyph = face->getGlyphMetrics(cp, fallback);
      assert(glyph == &fallback && glyph->width == glyph->height && glyph->advanceX > 0);
      int ink = 0;
      for (int y = 0; y < glyph->height; ++y)
        for (int x = 0; x < glyph->width; ++x) ink += syntheticGlyph::ink(cp, x, y, glyph->width, glyph->height);
      assert(ink > 0 && ink < glyph->width * glyph->height);
    }
  }
  assert(!syntheticGlyph::ink(0x2641, 7, 10, 16, 16));
  assert(syntheticGlyph::ink(0x1F728, 7, 7, 16, 16));
  assert(!syntheticGlyph::ink(0x2641, -1, 0, 16, 16));
  assert(!syntheticGlyph::ink(0x1F728, 0, 0, 0, 0));
  assert(syntheticGlyph::isSupported(syntheticGlyph::BLACK_SQUARE));
  assert(!syntheticGlyph::isSupported('A'));
  // Model the complete character coverage advertised by an optional SD face.
  static constexpr EpdUnicodeInterval unicode[] = {{32, 0xffff, 0}};
  EpdFontData sdData = toybox_30;
  sdData.intervals = unicode;
  sdData.intervalCount = 1;
  const EpdFont sd(&sdData);
  std::map<int, EpdFontFamily> fonts;
  fonts.emplace(1, EpdFontFamily(&pixel));
  fonts.emplace(2, EpdFontFamily(&ubuntu));
  fonts.emplace(3, EpdFontFamily(&sd));
  std::map<int, int> fallbacks{{1, 2}, {2, 3}};
  const auto resolve = [&](const char* text) {
    return fontfallback::resolve(fonts, fallbacks, 1, text, EpdFontFamily::REGULAR);
  };
  assert(resolve(nullptr) == 1 && resolve("") == 1);
  assert(resolve("MISIÓN DIARIA: ESPAÑOL ¡SÍ!") == 1);
  assert(resolve("Français") == 2);
  assert(resolve("Ève") == 2);
  assert(resolve("Привет") == 2);
  // Traverse the Ubuntu edge even when Ubuntu cannot cover this character.
  assert(resolve("中文") == 3);
  assert(resolve("Français 中文") == 3);
  assert(resolve("中文 Français") == 3);
  fallbacks.clear();
  assert(resolve("Français") == 1);
  fallbacks.emplace(1, 2);
  assert(resolve("Français") == 2);
  fallbacks.emplace(2, 1);
  assert(resolve("中文") == 1);
  assert(resolve("Français") == 2);
  fonts.erase(2);
  assert(resolve("Français") == 1);
  std::puts("Real pixel/Ubuntu coverage, Spanish identity, SD chaining, map reset and cycles passed.");
}
