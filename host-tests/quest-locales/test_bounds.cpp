#include <EpdFontFamily.h>

#include <cstdio>

int main() {
  // The accent must clear the base glyph by one pixel when it is raised.
  static constexpr EpdGlyph glyphs[] = {
      {5, 7, 96, 0, 7, 0, 0},
      {3, 2, 0, 0, 4, 0, 0},
  };
  static constexpr EpdUnicodeInterval intervals[] = {{0x61, 0x61, 0}, {0x301, 0x301, 1}};
  EpdFontData data{};
  data.glyph = glyphs;
  data.intervals = intervals;
  data.intervalCount = 2;
  EpdFont font(&data);
  EpdFontFamily family(&font);
  int minX, minY, maxX, maxY;
  family.getTextBounds("a\xcc\x81", &minX, &minY, &maxX, &maxY);
  if (minX != 0 || maxX != 5 || minY != 0 || maxY != 10) {
    std::printf("Unexpected combining-mark bounds: %d %d %d %d\n", minX, minY, maxX, maxY);
    return 1;
  }
  std::puts("Combining mark: raised ink is included in the bounds");
  return 0;
}
