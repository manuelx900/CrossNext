#include <FreeInkUIDisplayTarget.h>

#include <cassert>
#include <cstdio>
#include <vector>

#include "../../src/apps_local/ui/SdBitmapPack.h"

int main(int argc, char** argv) {
  namespace fui = freeink::ui;
  static constexpr struct {
    const char* path;
    unsigned count;
  } packs[] = {{"/apps/pet/graphics.bin", 64},   {"/apps/pet/home.bin", 45},    {"/apps/pet/habitat.bin", 560},
               {"/apps/pet/breathing.bin", 474}, {"/apps/pet/farewell.bin", 6}, {"/apps/pet/collection.bin", 1}};
  std::vector<uint8_t> pixels(128 * 128 / 8, 255);
  fui::DisplayTarget target(pixels.data(), 128, 128, 16);
  for (unsigned mode = 0; mode < 3; ++mode) {
    sdartMemory::available = mode != 1;
    sdartMemory::denyAllocation = mode == 2;
    sdart::Pack pack;
    for (const auto& asset : packs) {
      assert(pack.open(asset.path, asset.count));
      assert(sdartMemory::liveBytes == (mode == 0 ? asset.count * 16 + 4096 : 0));
      for (unsigned id : {0u, asset.count / 2, asset.count - 1}) {
        const auto image = pack.image(id);
        assert(image.width > 1 && image.width <= sdart::MAX_WIDTH && image.height <= sdart::MAX_HEIGHT);
        pack.draw(target, fui::makeRect(0, 0, 128, 128), id);
        assert(pack.ready());
      }
      pack.close();
      assert(!pack.ready() && !sdartMemory::liveBytes);
    }
    assert(!pack.open("/apps/pet/collection.bin", 2) && !sdartMemory::liveBytes);
    assert(pack.open("/apps/pet/collection.bin", 1));
    pack.image(1);
    assert(!pack.ready() && !sdartMemory::liveBytes);
    if (argc > 1) {
      assert(!pack.open(argv[1], 45));
      assert(pack.open(argv[1], 39));
      for (unsigned id : {0u, 38u}) {
        assert(pack.image(id).width > 1);
        pack.draw(target, fui::makeRect(0, 0, 128, 128), id);
        assert(pack.ready());
      }
      pack.close();
      assert(!sdartMemory::liveBytes);
    }
  }
  std::puts("PET real SD loader: six packs, actual dimensions/CRC, PSRAM/small-read/OOM fallback and close PASS");
}
