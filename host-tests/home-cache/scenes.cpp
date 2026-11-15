#include <GfxRenderer.h>
#include <HalMemory.h>
#include <HalStorage.h>

#include <cstdio>

#include "../../src/apps_local/home/CoverSceneCache.h"

int main(int argc, char**) {
  failPsram = argc > 1;
  unsigned checks = 0, failures = 0;
  const auto check = [&](bool ok) {
    ++checks;
    failures += !ok;
  };
  GfxRenderer renderer;
  const auto region = freeink::ui::makeRect(8, 80, 80, 100);
  testStorage.modified = 0;
  auto& source = testStorage.files["/thumb.bmp"];
  source.assign(20000, 0x35);
  const auto store = [&](uint64_t key) {
    check(!homecover::restoreScene(renderer, region, key));
    HalFile file;
    check(Storage.openFileForRead("TEST", "/thumb.bmp", file));
    renderer.fillRect(region.x, region.y, region.width, region.height);
    homecover::recordSource("/thumb.bmp", file, true);
    homecover::storeScene(renderer);
  };
  store(1);
  renderer.fillRect(region.x, region.y, region.width, region.height, false);
  const auto before = testStorage.readBytes;
  check(homecover::restoreScene(renderer, region, 1) != failPsram);
  if (!failPsram) {
    check(testStorage.readBytes == before);
    check(renderer.frame[region.y * renderer.width + region.x] == 1);
    source[10000] ^= 1;             // Same timestamp, size and header, changed middle of the file.
    homecover::invalidateScenes();  // Transfer/cache mutation boundary.
    check(!homecover::restoreScene(renderer, region, 1));
    store(2);
    source.push_back(0);
    homecover::invalidateScenes();  // Every file mutation invalidates the generation.
    check(!homecover::restoreScene(renderer, region, 2));
    store(3);
    homecover::invalidateScenes();
    check(!homecover::restoreScene(renderer, region, 3));
    store(4);
    testStorage.files.erase("/thumb.bmp");
    homecover::invalidateScenes();
    check(!homecover::restoreScene(renderer, region, 4));
    testStorage.files["/thumb.bmp"].assign(20000, 0x35);
    homecover::invalidateScenes();
    for (uint64_t key = 10; key < 13; ++key) store(key);
    // Cycling the three recent books must keep every art snapshot resident.
    for (unsigned visit = 0; visit < 30; ++visit) {
      renderer.fillRect(region.x, region.y, region.width, region.height, false);
      check(homecover::restoreScene(renderer, region, 10 + visit % 3));
      check(testStorage.readBytes == before);
      check(renderer.frame[region.y * renderer.width + region.x] == 1);
    }
  }
  std::printf("Home scene cache: %u checks, %u failures (%s)\n", checks, failures,
              failPsram ? "OOM" : "undated/warm metadata/mutations");
  return failures ? 1 : 0;
}
