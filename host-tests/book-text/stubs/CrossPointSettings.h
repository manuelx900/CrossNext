#pragma once
#include <cstdint>
#include <cstring>
struct CrossPointSettings {
  static constexpr int BUILTIN_FONT_COUNT = 1, FONT_WEIGHT_COUNT = 2, LINE_COMPRESSION_COUNT = 7,
                       PARAGRAPH_ALIGNMENT_COUNT = 5, SCREEN_MARGIN_MIN = 5, SCREEN_MARGIN_MAX = 40;
  uint8_t fontFamily = 0, fontPointSize = 14, fontWeight = 0, lineSpacing = 2, paragraphAlignment = 0,
          extraParagraphSpacing = 1, paragraphIndentSpaces = 2, wordSpacing = 100, characterSpacing = 2,
          screenMargin = 5, hyphenationEnabled = 0, embeddedStyle = 1, focusReadingEnabled = 0, textAntiAliasing = 1;
  uint8_t perBookTextSettings = 1;
  char sdFontFamilyName[32] = "Noto Serif";
  unsigned saves = 0;
  bool saveOk = true;
  bool saveToFile() {
    ++saves;
    return saveOk;
  }
};
inline CrossPointSettings testSettings;
#define SETTINGS testSettings
