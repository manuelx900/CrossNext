#include <GfxRenderer.h>
#include <HalMemory.h>
#include <HalStorage.h>
#include <LibraryBuilder.h>

#include <cstdio>

#include "../../src/apps_local/home/CoverSceneCache.h"
int main(int argc, char**) {
  int checks = 0, failures = 0;
  const auto check = [&](bool value) {
    ++checks;
    if (!value) {
      ++failures;
      std::printf("scene check %d failed\n", checks);
    }
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
    const auto original = renderer.frame;
    check(homecover::containsScene(region, 1));
    check(!homecover::containsScene(region, 2));
    check(!homecover::containsScene(freeink::ui::makeRect(0, 80, 400, 640), 1));
    check(renderer.frame == original);
    check(HalMemory::active == 1);
    const int opens = Storage.opens;
    renderer.frame[0] = 0;
    check(homecover::restoreScene(renderer, region, 1));
    check(renderer.frame[0] == 1);
    check(Storage.opens == opens);
    check(!homecover::restoreScene(renderer, freeink::ui::makeRect(0, 80, 400, 640), 1));
    Storage.files["/cover0"].size++;
    homecover::invalidateScenes();
    check(!homecover::containsScene(region, 1));
    check(!homecover::restoreScene(renderer, region, 1));
    save(2);
    Storage.files["/cover0"].modified++;
    homecover::invalidateScenes();
    check(!homecover::restoreScene(renderer, region, 2));
    save(3);
    homecover::invalidateScenes();
    check(!homecover::restoreScene(renderer, region, 3));
    save(4, 1, false);
    check(homecover::restoreScene(renderer, region, 4));
    ++library::testRevision;
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
    homecover::invalidateScenes();
    check(!homecover::restoreScene(renderer, region, 5));
    check(!homecover::restoreScene(renderer, region, 90));
    std::fill(renderer.frame.begin(), renderer.frame.end(), 0x90);
    homecover::storeScene(renderer, true);
    const auto libraryOpens = Storage.opens;
    std::fill(renderer.frame.begin(), renderer.frame.end(), 0);
    check(homecover::restoreScene(renderer, region, 90));
    check(renderer.frame[0] == 0x90);
    check(Storage.opens == libraryOpens);
    // Home churn preserves the last Library view without another allocation or source open.
    using Kind = homecover::SceneKind;
    homecover::invalidateScenes();
    check(!homecover::restoreScene(renderer, region, 900, Kind::Library));
    std::fill(renderer.frame.begin(), renderer.frame.end(), 0x5a);
    homecover::storeScene(renderer, true);
    const auto expectedLibrary = renderer.frame;
    for (uint64_t key = 901; key < 909; ++key) save(key);
    const auto beforeReturn = Storage.opens;
    renderer.frame = expectedLibrary;
    renderer.frame[renderer.frame.size() / 2] ^= 0xff;
    check(homecover::restoreScene(renderer, region, 900, Kind::Library));
    check(renderer.frame == expectedLibrary && Storage.opens == beforeReturn);
    check(HalMemory::active == 1);
    // Library paging likewise preserves the latest Home view.
    check(homecover::restoreScene(renderer, region, 908));
    const auto home = renderer.frame;
    for (uint64_t key = 910; key < 918; ++key) {
      check(!homecover::restoreScene(renderer, region, key, Kind::Library));
      std::fill(renderer.frame.begin(), renderer.frame.end(), uint8_t(key));
      homecover::storeScene(renderer, true);
    }
    renderer.frame = home;
    renderer.frame[renderer.frame.size() / 2] ^= 0xff;
    check(homecover::restoreScene(renderer, region, 908));
    check(renderer.frame == home);
    ++Storage.media;
    check(!homecover::containsScene(region, 908));
    check(!homecover::restoreScene(renderer, region, 917, Kind::Library));
    std::printf("Library return: identical pixels, zero cover opens; media replacement invalidates views\n");
    ++library::testRevision;
    check(!homecover::restoreScene(renderer, region, 90));
  }
  std::printf("Cover scenes: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
