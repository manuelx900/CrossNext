#include <algorithm>
#include <cstdio>
#include <vector>

#include "../../src/apps_local/home/CoverBoxSampler.h"
#include "../../src/apps_local/home/CoverScanline.h"

namespace {
int checks = 0, failures = 0;
void check(bool ok) {
  ++checks;
  if (!ok) ++failures;
}
std::vector<uint8_t> filtered(const std::vector<uint8_t>& source, int width, int height, int outWidth, int outHeight,
                              int taper, bool topDown) {
  const auto slot = freeink::ui::makeRect(13, 17, outWidth, outHeight);
  std::vector<homecover::BoxColumn> columns(outWidth);
  std::vector<homecover::BoxBand> bands(homecover::BoxSampler::bandCapacity(slot, taper));
  std::vector<uint16_t> prefix((width + 7) / 8);
  std::vector<uint8_t> pixels(outWidth * outHeight);
  homecover::BoxSampler sampler(columns.data(), bands.data(), prefix.data(), width, 0, width, height, slot, taper);
  for (int n = 0; n < height; ++n) {
    const int y = topDown ? n : height - 1 - n;
    sampler.paint(source.data() + y * ((width + 7) / 8), y, topDown, [&](int x, int yy, int w, int h) {
      check(x >= slot.x && yy >= slot.y && x + w <= slot.right() && yy + h <= slot.bottom());
      for (int dy = 0; dy < h; ++dy)
        for (int dx = 0; dx < w; ++dx) pixels[(yy - slot.y + dy) * outWidth + x - slot.x + dx] = 1;
    });
  }
  return pixels;
}
}  // namespace

int main() {
  for (int width : {1, 7, 31, 149, 257})
    for (int height : {1, 33, 224})
      for (int divisor : {1, 2, 3}) {
        const int outWidth = std::max(1, width / divisor), outHeight = std::max(1, height / divisor);
        std::vector<uint8_t> source(((width + 7) / 8) * height);
        for (size_t n = 0; n < source.size(); ++n) source[n] = uint8_t(n * 37 + n / 19);
        for (int taper : {-1, 0, 1}) {
          if (outHeight < 8 && taper) continue;
          const auto forward = filtered(source, width, height, outWidth, outHeight, taper, true);
          check(forward == filtered(source, width, height, outWidth, outHeight, taper, false));
        }
        for (int color : {0, 255}) {
          std::fill(source.begin(), source.end(), color);
          const auto pixels = filtered(source, width, height, outWidth, outHeight, 0, true);
          check(std::all_of(pixels.begin(), pixels.end(), [color](uint8_t pixel) { return pixel == !color; }));
        }
      }
  // A half-tone checkerboard must retain its mean when reduced by an even factor.
  // Point sampling aliases this source to a single flat color.
  std::vector<uint8_t> checker(64 * 64 / 8);
  for (int y = 0; y < 64; ++y) std::fill(checker.begin() + y * 8, checker.begin() + (y + 1) * 8, y & 1 ? 0x55 : 0xaa);
  const auto reduced = filtered(checker, 64, 64, 32, 32, 0, true);
  check(std::count(reduced.begin(), reduced.end(), 1) == 512);
  std::vector<uint8_t> cover(((145 + 7) / 8) * 218);
  for (int y = 0; y < 218; ++y) std::fill(cover.begin() + y * 19, cover.begin() + (y + 1) * 19, y & 1 ? 0x55 : 0xaa);
  const auto fitted = filtered(cover, 145, 218, 98, 147, 0, true);
  const auto ink = std::count(fitted.begin(), fitted.end(), 1);
  check(ink > int(fitted.size()) * 40 / 100 && ink < int(fitted.size()) * 60 / 100);
  for (bool topDown : {false, true}) {
    const auto slot = freeink::ui::makeRect(13, 17, 120, 180);
    std::vector<uint8_t> pixels(120 * 180), row((108 + 7) / 8, 0);
    for (int n = 0; n < 180; ++n) {
      const int y = (topDown ? n : 179 - n) - 9;
      if (y < 0 || y >= 162) continue;
      homecover::paint1BitRow(row.data(), 0, 108, 162, y, slot, 0, [&](int x, int yy, int w, int h) {
        check(x >= slot.x && yy >= slot.y && x + w <= slot.right() && yy + h <= slot.bottom());
        for (int dy = 0; dy < h; ++dy)
          for (int dx = 0; dx < w; ++dx) pixels[(yy - slot.y + dy) * 120 + x - slot.x + dx] = 1;
      });
    }
    check(std::all_of(pixels.begin(), pixels.end(), [](uint8_t p) { return p == 1; }));
  }
  std::printf("cover-sampling: %d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}
