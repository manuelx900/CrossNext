#pragma once

#include "CoverScanline.h"

namespace homecover {
struct BoxColumn {
  uint16_t begin, end;
  uint32_t ink;
};
struct BoxBand {
  uint16_t first, last, rows, inset;
};

// Borrowed row scratch; source pixels are averaged before monochrome reduction.
class BoxSampler {
 public:
  static int bandCapacity(freeink::ui::Rect slot, int taper) {
    return taper ? std::max(3, int(slot.height) / 16) + 1 : 1;
  }
  BoxSampler(BoxColumn* columns, BoxBand* bands, uint16_t* prefix, int width, int sourceX, int cropWidth,
             int cropHeight, freeink::ui::Rect slot, int taper)
      : columns(columns), bands(bands), prefix(prefix), width(width), cropHeight(cropHeight), slot(slot) {
    int first = 0;
    while (first < slot.width) {
      const int inset = edgeInset(first, slot, taper);
      int last = first + 1;
      while (last < slot.width && edgeInset(last, slot, taper) == inset) ++last;
      bands[bandCount++] = {uint16_t(first), uint16_t(last), 0, uint16_t(inset)};
      first = last;
    }
    for (int x = 0; x < slot.width; ++x)
      columns[x] = {uint16_t(sourceX + x * cropWidth / slot.width),
                    uint16_t(sourceX + (x + 1) * cropWidth / slot.width), 0};
  }

  template <typename Draw>
  void paint(const uint8_t* row, int sourceLine, bool topDown, Draw draw) {
    uint16_t sum = 0;
    for (int n = 0; n < (width + 7) / 8; ++n) prefix[n] = sum += popcount(uint8_t(~row[n]));
    const int nextLine = sourceLine + (topDown ? 1 : -1);
    for (int b = 0; b < bandCount; ++b) {
      auto& band = bands[b];
      const int height = slot.height - 2 * band.inset;
      const int y = sourceLine * height / cropHeight;
      const bool flush = nextLine < 0 || nextLine >= cropHeight || nextLine * height / cropHeight != y;
      ++band.rows;
      int run = -1;
      for (int x = band.first; x < band.last; ++x) {
        auto& column = columns[x];
        column.ink += countTo(row, column.end) - countTo(row, column.begin);
        if (!flush) continue;
        const uint32_t area = uint32_t(column.end - column.begin) * band.rows;
        const bool black = column.ink * 32 > uint32_t(2 * thresholds[y & 3][x & 3] + 1) * area;
        column.ink = 0;
        if (black && run < 0) run = x;
        if (!black && run >= 0) {
          draw(slot.x + run, slot.y + band.inset + y, x - run, 1);
          run = -1;
        }
      }
      if (flush) {
        if (run >= 0) draw(slot.x + run, slot.y + band.inset + y, band.last - run, 1);
        band.rows = 0;
      }
    }
  }

 private:
  static constexpr uint8_t bits[16] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};
  static constexpr uint8_t thresholds[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
  static int popcount(uint8_t value) { return bits[value & 15] + bits[value >> 4]; }
  int countTo(const uint8_t* row, int end) const {
    const int bytes = end / 8, tail = end % 8;
    return (bytes ? prefix[bytes - 1] : 0) + (tail ? popcount(uint8_t(~row[bytes]) & (0xff << (8 - tail))) : 0);
  }
  BoxColumn* columns;
  BoxBand* bands;
  uint16_t* prefix;
  int width, cropHeight, bandCount = 0;
  freeink::ui::Rect slot;
};
}  // namespace homecover
