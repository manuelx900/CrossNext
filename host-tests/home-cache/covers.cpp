#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalMemory.h>
#include <HalStorage.h>

#include <cstdio>
#include <cstring>
#include <vector>

#include "../../src/apps_local/home/CoverBitmapPainter.h"
std::vector<uint8_t> bmp(int w, int h, unsigned bpp, bool topDown) {
  const unsigned offset = 54 + (1u << bpp) * 4, row = (w * bpp + 31) / 32 * 4;
  std::vector<uint8_t> bytes(offset + row * h);
  const auto put = [&](unsigned at, uint32_t value, unsigned n) {
    for (unsigned i = 0; i < n; ++i) bytes[at + i] = value >> (8 * i);
  };
  put(0, 0x4D42, 2);
  put(2, bytes.size(), 4);
  put(10, offset, 4);
  put(14, 40, 4);
  put(18, w, 4);
  put(22, topDown ? -h : h, 4);
  put(26, 1, 2);
  put(28, bpp, 2);
  for (unsigned n = 0; n < (1u << bpp); ++n) put(54 + n * 4, (n * 255 / ((1u << bpp) - 1)) * 0x010101, 4);
  return bytes;
}
int main(int argc, char**) {
  failPsram = argc > 1;
  unsigned checks = 0, failures = 0;
  const auto check = [&](bool ok) {
    ++checks;
    if (!ok) ++failures;
  };
  GfxRenderer renderer;
  for (int width : {16, 64, 240, 800})
    for (int height : {24, 96, 400})
      for (unsigned depth : {1u, 2u})
        for (bool topDown : {false, true})
          for (const auto slot : {freeink::ui::makeRect(10, 10, 80, 120), freeink::ui::makeRect(20, 20, 200, 100)}) {
            testStorage.files["/cover.bmp"] = bmp(width, height, depth, topDown);
            HalFile file;
            check(Storage.openFileForRead("TEST", "/cover.bmp", file));
            Bitmap bitmap(file);
            check(bitmap.parseHeaders() == BmpReaderError::Ok);
            std::fill(renderer.frame.begin(), renderer.frame.end(), 0);
            const bool ok = homecover::paintBitmap(renderer, bitmap, slot);
            check(ok != failPsram);
            if (failPsram) continue;
            for (int y = 0; y < renderer.height; ++y)
              for (int x = 0; x < renderer.width; ++x)
                check(bool(renderer.frame[y * renderer.width + x]) ==
                      (x >= slot.x && x < slot.right() && y >= slot.y && y < slot.bottom()));
          }
  std::printf("Production cover renderer: %u checks, %u failures (%s)\n", checks, failures,
              failPsram ? "PSRAM unavailable" : "portrait/landscape, 1/2 bit, both row orders");
  return failures ? 1 : 0;
}
