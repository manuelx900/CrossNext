#include <FreeInkUIDisplayTarget.h>

#include <cassert>
#include <cstdio>

#include "../../src/apps_local/intercept/InterceptArt.h"
#include "../../src/apps_local/ui/SdBitmapPack.h"

int main(int argc, char** argv) {
  namespace fui = freeink::ui;
  assert(argc == 5);
  uint8_t pixels[32 * 32 / 8]{};
  fui::DisplayTarget target(pixels, 32, 32, 4);
  for (unsigned mode = 0; mode < 3; ++mode) {
    sdartMemory::available = mode != 1;
    sdartMemory::denyAllocation = mode == 2;
    sdart::Pack pack;
    assert(pack.open(interceptart::PATH, interceptart::COUNT));
    assert(sdartMemory::liveBytes == (mode == 0 ? 4880u : 0u));
    for (unsigned id = 0; id < interceptart::COUNT; ++id) {
      pack.draw(target, fui::makeRect(0, 0, 32, 32), id);
      assert(pack.ready());
    }
    pack.close();
    assert(!pack.ready() && sdartMemory::liveBytes == 0);
    assert(!pack.open(interceptart::PATH, interceptart::COUNT - 1));
    for (unsigned bad = 1; bad < 4; ++bad) {
      assert(!pack.open(argv[bad], interceptart::COUNT));
      assert(!pack.ready() && sdartMemory::liveBytes == 0);
    }
    assert(pack.open(argv[4], interceptart::COUNT));
    pack.draw(target, fui::makeRect(0, 0, 32, 32), 0);
    assert(!pack.ready() && sdartMemory::liveBytes == 0);
  }
  std::puts("Intercept: all 49 sprites, CRC/read failures, missing/truncated/index errors, PSRAM/OOM fallback PASS");
}
