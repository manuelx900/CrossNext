#pragma once
#include <FreeInkUIDisplayTarget.h>

#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
namespace fui = freeink::ui;
class GfxRenderer {
 public:
  int width, height;
  mutable int measures = 0;
  int texts = 0;
  std::array<uint8_t, 48000> pixels;
  fui::DisplayTarget target;
  GfxRenderer(int w, int h)
      : width(w), height(h), target(pixels.data(), w, h, (w + 7) / 8, fui::Orientation::LandscapeCounterClockwise) {
    pixels.fill(255);
  }
  int getScreenWidth() const { return width; }
  int getScreenHeight() const { return height; }
  void getOrientedViewableTRBL(int* t, int* r, int* b, int* l) const { *t = *r = *b = *l = 4; }
  int getTextWidth(int, const char* s) const {
    ++measures;
    return target.measureText(0, s, {}).width;
  }
  void drawText(int, int x, int y, const char* s) {
    if (!*s) return;
    ++texts;
    if (!(x >= 0 && x + getTextWidth(0, s) <= width && y >= 0 && y + 24 <= height))
      std::fprintf(stderr, "overflow x=%d y=%d width=%d screen=%dx%d text=%s\n", x, y, getTextWidth(0, s), width,
                   height, s);
    assert(x >= 0 && x + getTextWidth(0, s) <= width && y >= 0 && y + 24 <= height);
    target.text(fui::makeRect(x, y, width - x, 24), s, {});
  }
  void fillRect(int x, int y, int w, int h, bool black) {
    assert(x >= 0 && y >= 0 && x + w <= width && y + h <= height);
    target.fill(fui::makeRect(x, y, w, h), fui::Paint::solid(black ? fui::Color::Black : fui::Color::White));
  }
  void write(const char* name) {
    FILE* f = std::fopen(name, "wb");
    assert(f);
    std::fprintf(f, "P4\n%d %d\n", width, height);
    for (int i = 0; i < (width + 7) / 8 * height; ++i) std::fputc(pixels[i] ^ 255, f);
    std::fclose(f);
  }
};
