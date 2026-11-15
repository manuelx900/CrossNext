#pragma once
#include <EpdFontFamily.h>
#include <builtinFonts/notosans_8_regular.h>
#include <builtinFonts/ubuntu_10_regular.h>

#include <map>
#include <string>
#include <vector>

enum class Color { Black, White, LightGray, DarkGray, Clear };

// The production SDK adapter is exercised against real font metrics; drawing is recorded.
class GfxRenderer {
 public:
  enum Orientation { Portrait, LandscapeClockwise, PortraitInverted, LandscapeCounterClockwise };
  struct Bounds {
    int left, top, width, height;
  };
  EpdFont face{&ubuntu_10_regular}, tiny{&notosans_8_regular};
  std::map<int, EpdFontFamily> fonts;
  mutable std::vector<std::string> drawn;
  mutable unsigned paragraphWraps = 0;
  GfxRenderer() {
    for (int id = 0; id < 6; ++id) fonts.emplace(id, EpdFontFamily{id == 3 ? &tiny : &face});
  }
  int getScreenWidth() const { return 480; }
  int getScreenHeight() const { return 800; }
  Orientation getOrientation() const { return Portrait; }
  void getOrientedViewableTRBL(int* t, int* r, int* b, int* l) const { *t = *r = *b = *l = 0; }
  const auto& getFontMap() const { return fonts; }
  int getLineHeight(int id) const { return fonts.at(id).getData()->advanceY; }
  int getFontAscenderSize(int id) const { return fonts.at(id).getData()->ascender; }
  int getTextWidth(int id, const char* text, EpdFontFamily::Style style = EpdFontFamily::REGULAR) const {
    int width, height;
    fonts.at(id).getTextDimensions(text, &width, &height, style);
    return width;
  }
  Bounds getTextBounds(int id, const char* text, EpdFontFamily::Style style) const {
    int a, b, c, d;
    fonts.at(id).getTextBounds(text, &a, &b, &c, &d, style);
    return {a, getFontAscenderSize(id) - d, c - a, d - b};
  }
  std::string truncatedText(int id, const char* text, int width, EpdFontFamily::Style style) const {
    return getTextWidth(id, text, style) <= width ? text : "TRUNCATED";
  }
  std::vector<std::string> wrappedText(int id, const char* text, int width, int, EpdFontFamily::Style style) const {
    if (std::string(text).find('\n') != std::string::npos) ++paragraphWraps;
    return {truncatedText(id, text, width, style)};
  }
  void drawText(int, int, int, const char* text, bool, EpdFontFamily::Style) const { drawn.emplace_back(text); }
  template <class... A>
  void drawLine(A...) const {}
  template <class... A>
  void drawPixel(A...) const {}
  template <class... A>
  void drawRect(A...) const {}
  template <class... A>
  void drawRoundedRect(A...) const {}
  template <class... A>
  void drawTextRotated90CW(A...) const {}
  template <class... A>
  void fillPolygon(A...) const {}
  template <class... A>
  void fillRect(A...) const {}
  template <class... A>
  void fillRectDither(A...) const {}
  template <class... A>
  void fillRoundedRect(A...) const {}
};
