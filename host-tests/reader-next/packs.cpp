#include <GfxRenderer.h>
#include <LibraryBuilder.h>

#include <cstdio>
#include <vector>

#include "../../src/apps_local/home/CoverSceneCache.h"
#include "../../src/apps_local/library/ScenePack.h"
int main() {
  unsigned checks = 0, failed = 0;
#define CHECK(x)                                                       \
  do {                                                                 \
    ++checks;                                                          \
    if (!(x)) {                                                        \
      ++failed;                                                        \
      std::printf("Pack check %u line %d failed\n", checks, __LINE__); \
    }                                                                  \
  } while (0)
  using Pack = libraryui::ScenePack;
  Pack pack;
  Pack::Key key{101,
                0,
                library::libraryContentRevision(),
                Storage.resourceRevision(),
                Storage.mediaRevision(),
                freeink::ui::makeRect(4, 80, 320, 200),
                8000};
  std::vector<uint8_t> bytes(key.bytes), out(key.bytes);
  for (size_t n = 0; n < bytes.size(); ++n) bytes[n] = uint8_t(n * 31);
  const auto finish = [&]() {
    unsigned steps = 0;
    while (pack.pending() && steps++ < 20) pack.step();
    CHECK(!pack.pending());
    CHECK(sceneOpenHandles == 0);
  };
  CHECK(!pack.load(key, out.data(), out.size()));
  pack.queue(key, bytes.data());
  pack.step();
  CHECK(pack.pending());
  pack.cancel();
  CHECK(!pack.contains(key));
  CHECK(sceneOpenHandles == 0);
  pack.queue(key, bytes.data());
  finish();
  CHECK(pack.contains(key));
  auto opens = Storage.opens;
  auto reads = sceneReads;
  CHECK(pack.load(key, out.data(), out.size()));
  CHECK(out == bytes);
  CHECK(Storage.opens == opens + 1 && sceneReads == reads + 2);
  int remaining = 1;
  const auto beforeCancel = Storage.opens;
  CHECK(!pack.load(key, out.data(), out.size(), [](void* p) { return --*static_cast<int*>(p) >= 0; }, &remaining));
  CHECK(pack.contains(key));
  CHECK(Storage.opens == beforeCancel + 1);
  CHECK(pack.load(key, out.data(), out.size()));
  CHECK(out == bytes);
  Pack otherBoot;
  opens = Storage.opens;
  CHECK(!otherBoot.load(key, out.data(), out.size()));
  CHECK(Storage.opens == opens);
  ++testSceneResource;
  CHECK(!pack.contains(key));
  CHECK(!pack.load(key, out.data(), out.size()));
  key.resource = Storage.resourceRevision();
  packWritesLeft = 1;
  pack.queue(key, bytes.data());
  finish();
  CHECK(!pack.contains(key));
  packWritesLeft = -1;
  pack.queue(key, bytes.data());
  finish();
  CHECK(pack.contains(key));
  packReadsLeft = 1;
  CHECK(!pack.load(key, out.data(), out.size()));
  CHECK(!pack.contains(key));
  packReadsLeft = -1;
  pack.queue(key, bytes.data());
  finish();
  for (auto& [name, file] : Storage.files)
    if (file.bytes.size() > 64) file.bytes.back() ^= 1;
  CHECK(!pack.load(key, out.data(), out.size()));
  CHECK(!pack.contains(key));
  pack.queue(key, bytes.data());
  finish();
  sceneOnRead = []() { ++testSceneResource; };
  CHECK(!pack.load(key, out.data(), out.size()));
  key.resource = Storage.resourceRevision();
  sceneFailClose = true;
  pack.queue(key, bytes.data());
  finish();
  CHECK(!pack.contains(key));
  sceneFailClose = false;
  pack.queue(key, bytes.data());
  finish();
  CHECK(pack.contains(key));
  ++Storage.media;
  CHECK(!pack.contains(key));
  key.media = Storage.mediaRevision();
  packWritesLeft = 3;
  pack.queue(key, bytes.data());
  finish();
  CHECK(!pack.contains(key));
  packWritesLeft = -1;
  CHECK(!pack.load(Pack::Key{}, out.data(), out.size()));
  // The production scene owner spills after presentation and paints only verified loads.
  GfxRenderer renderer;
  const auto region = freeink::ui::makeRect(0, 80, 480, 640);
  for (uint64_t n = 200; n < 207; ++n) {
    CHECK(!homecover::restoreScene(renderer, region, n, homecover::SceneKind::Library));
    std::fill(renderer.frame.begin(), renderer.frame.end(), uint8_t(n));
    homecover::storeScene(renderer, true);
    unsigned steps = 0;
    while (homecover::scenePackPending() && steps++ < 20) homecover::stepScenePack();
    CHECK(!homecover::scenePackPending());
  }
  std::fill(renderer.frame.begin(), renderer.frame.end(), 0x55);
  const auto untouched = renderer.frame;
  opens = Storage.opens;
  CHECK(!homecover::restoreScene(renderer, region, 200, homecover::SceneKind::Library, false));
  CHECK(renderer.frame == untouched && Storage.opens == opens);
  CHECK(homecover::restoreScene(renderer, region, 200, homecover::SceneKind::Library, true));
  CHECK(renderer.frame[0] == 200);
  CHECK(Storage.opens == opens + 1);
  CHECK(sceneOpenHandles == 0);
  const auto beforePrefetch = renderer.frame;
  CHECK(homecover::prefetchLibraryScene(renderer, region, 201, [](void*) { return true; }, nullptr));
  CHECK(renderer.frame == beforePrefetch);
  opens = Storage.opens;
  CHECK(homecover::restoreScene(renderer, region, 201, homecover::SceneKind::Library, false));
  CHECK(Storage.opens == opens && renderer.frame[0] == 201);
  for (uint64_t n = 210; n < 215; ++n) {
    CHECK(!homecover::restoreScene(renderer, region, n, homecover::SceneKind::Library));
    std::fill(renderer.frame.begin(), renderer.frame.end(), uint8_t(n));
    homecover::storeScene(renderer, true);
    while (homecover::scenePackPending()) homecover::stepScenePack();
  }
  for (auto& [path, file] : Storage.files) {
    if (file.bytes.size() > 64 && file.bytes[8] == 200) file.bytes.back() ^= 1;
  }
  std::fill(renderer.frame.begin(), renderer.frame.end(), 0x33);
  const auto beforeCorrupt = renderer.frame;
  CHECK(!homecover::restoreScene(renderer, region, 200, homecover::SceneKind::Library, true));
  CHECK(renderer.frame == beforeCorrupt);
  ++testSceneResource;
  opens = Storage.opens;
  CHECK(!homecover::restoreScene(renderer, region, 214, homecover::SceneKind::Library, true));
  CHECK(Storage.opens == opens);
  homecover::cancelScenePack();
  CHECK(sceneOpenHandles == 0);
  std::printf("Library scene packs: %u checks, %u failures; one open/two reads per verified grid, no pre-notice I/O\n",
              checks, failed);
  return failed ? 1 : 0;
}
