#include <cassert>
#include <cstdio>

#include "../../lib/Epub/Epub/converters/PixelCache.h"

int main() {
  unsigned cases = 0;
  for (int width : {1, 15, 31, 120, 480}) {
    for (int height : {1, 7, 19, 800}) {
      for (int block : {1, 4, 16}) {
        if (block > height) continue;
        for (bool clipped : {false, true}) {
          PixelCache cache;
          assert(cache.begin("test.pxc", width, height, 0, 0, block));
          const int stride = (width + 3) / 4;
          const int first = clipped ? height / 4 : 0;
          const int end = clipped ? height * 3 / 4 : height;
          for (int y = first; y < end; y += block) {
            assert(cache.advanceTo(y));
            for (int r = y; r < std::min(end, y + block); ++r) {
              for (int x = 0; x < stride; ++x)
                cache.buffer[(r - cache.bandStart) * stride + x] = uint8_t(r * 131 + x * 17);
            }
          }
          assert(cache.finalize());
          assert(written.size() == size_t(4 + stride * height));
          assert(written[0] == uint8_t(width) && written[1] == uint8_t(width >> 8));
          assert(written[2] == uint8_t(height) && written[3] == uint8_t(height >> 8));
          for (int y = 0; y < height; ++y) {
            for (int x = 0; x < stride; ++x)
              assert(written[4 + y * stride + x] == (y >= first && y < end ? uint8_t(y * 131 + x * 17) : 0));
          }
          if (width == 480 && height == 800 && !clipped && (block == 1 || block == 16)) {
            assert(writes == 52);
            std::printf("480x800 block=%d: %zu writes, 96004 identical bytes\n", block, writes);
          }
          ++cases;
        }
      }
    }
  }
  for (size_t failure : {1U, 2U, 3U, 4U}) {
    failAt = failure;
    {
      PixelCache cache;
      if (cache.begin("fail.pxc", 480, 800, 0, 0, 1)) {
        bool ok = true;
        for (int y = 0; y < 800 && ok; ++y) ok = cache.advanceTo(y);
        cache.finalize();
      }
    }
    assert(!exists);
  }
  failAt = 0;
  {
    PixelCache cache;
    assert(cache.begin("cancel.pxc", 480, 800, 0, 0, 1));
  }
  assert(!exists);
  {
    PixelCache cache;
    assert(cache.begin("close.pxc", 480, 800, 0, 0, 1));
    failClose = true;
    assert(!cache.finalize());
    failClose = false;
  }
  assert(!exists);
  std::printf("Pixel bands: %u geometry/gap cases; short writes, cancel and close failure passed\n", cases);
}
