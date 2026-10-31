#include <GfxRenderer.h>
#include <HalMemory.h>
#include <HalStorage.h>

#include <cstdio>

#include "../../src/apps_local/home/CoverSceneCache.h"
int main(int argc, char**) {
  int checks = 0, failures = 0;
  const auto check = [&](bool value) {
    ++checks;
    if (!value) ++failures;
  };
  const auto region = freeink::ui::makeRect(0, 80, 480, 640);
  GfxRenderer renderer;
  if (argc > 1) {
    HalMemory::fail = true;
    check(!homecover::restoreScene(renderer, region, 1));
    homecover::storeScene(renderer);
    check(!homecover::restoreScene(renderer, region, 1));
    check(HalMemory::active == 0);
  } else {
    const auto save = [&](uint64_t key, int sources = 1, bool dated = true) {
      std::fill(renderer.frame.begin(), renderer.frame.end(), uint8_t(key));
      check(!homecover::restoreScene(renderer, region, key));
      for (int n = 0; n < sources; ++n) {
        const std::string path = "/cover" + std::to_string(n);
        Storage.files[path].modified = dated ? 100 : 0;
        HalFile file;
        Storage.openFileForRead("test", path.c_str(), file);
        homecover::recordSource(path.c_str(), file, true);
      }
      homecover::storeScene(renderer);
    };
    save(1, 7);
    check(HalMemory::active == 1);
    const int opens = Storage.opens;
    renderer.frame[0] = 0;
    check(homecover::restoreScene(renderer, region, 1));
    check(renderer.frame[0] == 1);
    check(Storage.opens - opens == 7);
    check(!homecover::restoreScene(renderer, freeink::ui::makeRect(0, 80, 400, 640), 1));
    Storage.files["/cover0"].size++;
    check(!homecover::restoreScene(renderer, region, 1));
    save(2);
    Storage.files["/cover0"].modified++;
    check(!homecover::restoreScene(renderer, region, 2));
    save(3);
    homecover::invalidateScenes();
    check(!homecover::restoreScene(renderer, region, 3));
    save(4, 1, false);
    check(!homecover::restoreScene(renderer, region, 4));
    save(5);
    save(6);
    save(7);
    check(homecover::restoreScene(renderer, region, 5));
    save(8);
    check(!homecover::restoreScene(renderer, region, 6));
    check(homecover::restoreScene(renderer, region, 5));
    check(HalMemory::active == 1);
    Storage.files.erase("/cover0");
    check(!homecover::restoreScene(renderer, region, 5));
  }
  std::printf("Cover scenes: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
