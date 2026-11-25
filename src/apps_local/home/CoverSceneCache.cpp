#include "CoverSceneCache.h"

#include <GfxRenderer.h>
#include <HalMemory.h>
#include <HalStorage.h>
#include <LibraryBuilder.h>
#include <Logging.h>
#include <Performance.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <new>

#include "../library/ScenePack.h"

namespace homecover {
namespace {
constexpr size_t SLOT_BYTES = 48 * 1024;
constexpr size_t SLOT_COUNT = 3;
struct Scene {
  freeink::ui::Rect region;
  uint64_t key = 0;
  uint32_t age = 0;
  uint32_t revision = 0;
  uint32_t libraryRevision = 0;
  uint32_t mediaRevision = 0;
  uint32_t resourceRevision = 0;
  SceneKind kind = SceneKind::Home;
  size_t bytes = 0;
  uint8_t count = 0;
  bool valid = false;
};
struct Cache {
  std::array<Scene, SLOT_COUNT> scenes{};
  Scene pending{};
  libraryui::ScenePack pack;
  size_t spillSlot = SLOT_COUNT;
  uint32_t spillAge = 0;
  uint8_t pixels[SLOT_BYTES * SLOT_COUNT];
};
HalMemory::PsramBuffer arena;
Cache* cache = nullptr;
uint32_t age = 0;
bool attempted = false;
std::atomic<uint32_t> revision{0};

bool matches(const Scene& scene, freeink::ui::Rect region, uint64_t key, uint32_t artRevision, uint32_t libraryRevision,
             uint32_t mediaRevision, uint32_t resourceRevision, SceneKind kind) {
  return mediaRevision && resourceRevision && scene.resourceRevision == resourceRevision && scene.valid &&
         scene.mediaRevision == mediaRevision && scene.kind == kind && scene.key == key &&
         scene.revision == artRevision && scene.libraryRevision == libraryRevision && scene.region.x == region.x &&
         scene.region.y == region.y && scene.region.width == region.width && scene.region.height == region.height;
}

size_t replacement(SceneKind kind) {
  auto& scenes = cache->scenes;
  size_t protectedSlot = scenes.size(), replace = scenes.size();
  for (size_t n = 0; n < scenes.size(); ++n)
    if (scenes[n].valid && scenes[n].kind != kind &&
        (protectedSlot == scenes.size() || scenes[n].age > scenes[protectedSlot].age))
      protectedSlot = n;
  for (size_t n = 0; n < scenes.size(); ++n) {
    if (!scenes[n].valid) return n;
    if (n != protectedSlot && (replace == scenes.size() || scenes[n].age < scenes[replace].age)) replace = n;
  }
  return replace;
}

libraryui::ScenePack::Key packKey(const Scene& scene) {
  return {scene.key,
          scene.revision,
          scene.libraryRevision,
          scene.resourceRevision,
          scene.mediaRevision,
          scene.region,
          static_cast<uint32_t>(scene.bytes)};
}
void queuePack(size_t slot) {
  const auto& scene = cache->scenes[slot];
  if (scene.kind != SceneKind::Library || cache->pack.contains(packKey(scene))) return;
  cache->pack.queue(packKey(scene), cache->pixels + slot * SLOT_BYTES);
  cache->spillSlot = slot;
  cache->spillAge = scene.age;
}

}  // namespace

bool restoreScene(GfxRenderer& renderer, freeink::ui::Rect region, uint64_t key, SceneKind kind, bool allowPack) {
  if (!Storage.mediaRevision() || !Storage.resourceRevision()) {
    performance::count(performance::Counter::CoverSceneMiss);
    return false;
  }
  if (!attempted) {
    attempted = true;
    // Three art-only snapshots survive Home in PSRAM.
    // One bounded arena avoids internal RAM and per-return allocations.
    arena = HalMemory::allocatePsram(sizeof(Cache));
    if (!arena) {
      LOG_ERR("HOME", "PSRAM art cache unavailable (%u bytes)", unsigned(sizeof(Cache)));
      performance::count(performance::Counter::CoverSceneMiss);
      return false;
    }
    cache = new (arena.get()) Cache;
  }
  if (!cache) {
    performance::count(performance::Counter::CoverSceneMiss);
    return false;
  }
  auto& pending = cache->pending;
  auto& scenes = cache->scenes;
  pending.valid = false;
  const auto currentRevision = revision.load();
  const auto libraryRevision = library::libraryContentRevision();
  const auto mediaRevision = Storage.mediaRevision();
  const auto resourceRevision = Storage.resourceRevision();
  for (size_t n = 0; n < scenes.size(); ++n) {
    auto& scene = scenes[n];
    if (scene.revision != currentRevision || scene.libraryRevision != libraryRevision ||
        scene.mediaRevision != mediaRevision || scene.resourceRevision != resourceRevision)
      scene.valid = false;
    if (!matches(scene, region, key, currentRevision, libraryRevision, mediaRevision, resourceRevision, kind)) continue;
    if (renderer.copyBufferToRegion(region.x, region.y, region.width, region.height, cache->pixels + n * SLOT_BYTES,
                                    scene.bytes)) {
      scene.age = ++age;
      queuePack(n);
      performance::count(performance::Counter::CoverSceneHit);
      return true;
    }
    scene.valid = false;
  }
  pending.key = key;
  pending.kind = kind;
  pending.mediaRevision = mediaRevision;
  pending.resourceRevision = resourceRevision;
  pending.region = region;
  pending.bytes = renderer.getRegionByteSize(region.x, region.y, region.width, region.height);
  pending.count = 0;
  pending.revision = currentRevision;
  pending.libraryRevision = libraryRevision;
  pending.valid = pending.bytes > 0 && pending.bytes <= SLOT_BYTES;
  if (allowPack && kind == SceneKind::Library && pending.valid && cache->pack.contains(packKey(pending))) {
    cancelScenePack();
    const auto slot = replacement(kind);
    auto& scene = scenes[slot];
    scene.valid = false;
    if (cache->pack.load(packKey(pending), cache->pixels + slot * SLOT_BYTES, SLOT_BYTES) &&
        renderer.copyBufferToRegion(region.x, region.y, region.width, region.height, cache->pixels + slot * SLOT_BYTES,
                                    pending.bytes)) {
      scene = pending;
      scene.age = ++age;
      pending.valid = false;
      performance::count(performance::Counter::CoverPackHit);
      performance::count(performance::Counter::CoverSceneHit);
      return true;
    }
  }
  performance::count(performance::Counter::CoverSceneMiss);
  return false;
}

bool containsScene(freeink::ui::Rect region, uint64_t key, SceneKind kind) {
  if (!cache) return false;
  const auto artRevision = revision.load(), libraryRevision = library::libraryContentRevision();
  const auto mediaRevision = Storage.mediaRevision();
  const auto resourceRevision = Storage.resourceRevision();
  for (const auto& scene : cache->scenes)
    if (matches(scene, region, key, artRevision, libraryRevision, mediaRevision, resourceRevision, kind)) return true;
  return false;
}

bool prefetchLibraryScene(GfxRenderer& renderer, freeink::ui::Rect region, uint64_t key, bool (*continueWork)(void*),
                          void* context) {
  if (!cache || (continueWork && !continueWork(context))) return false;
  if (containsScene(region, key, SceneKind::Library)) return true;
  Scene wanted;
  wanted.key = key;
  wanted.region = region;
  wanted.revision = revision.load();
  wanted.libraryRevision = library::libraryContentRevision();
  wanted.resourceRevision = Storage.resourceRevision();
  wanted.mediaRevision = Storage.mediaRevision();
  wanted.bytes = renderer.getRegionByteSize(region.x, region.y, region.width, region.height);
  wanted.kind = SceneKind::Library;
  if (!cache->pack.contains(packKey(wanted))) return false;
  cancelScenePack();
  const size_t slot = replacement(SceneKind::Library);
  auto& scene = cache->scenes[slot];
  scene.valid = false;
  if (!cache->pack.load(packKey(wanted), cache->pixels + slot * SLOT_BYTES, SLOT_BYTES, continueWork, context))
    return false;
  scene = wanted;
  scene.age = ++age;
  scene.valid = true;
  return true;
}

void invalidateScenes() { revision.fetch_add(1); }
uint32_t sceneRevision() { return revision.load(); }

void recordSource(bool resolved) {
  if (!cache) return;
  auto& pending = cache->pending;
  if (!pending.valid) return;
  if (!resolved || pending.count == 7) {
    pending.valid = false;
    return;
  }
  ++pending.count;
}

void storeScene(GfxRenderer& renderer, bool completeArtwork) {
  if (!cache) return;
  auto& pending = cache->pending;
  auto& scenes = cache->scenes;
  if (!pending.valid || (!pending.count && !completeArtwork) || pending.revision != revision.load() ||
      pending.libraryRevision != library::libraryContentRevision() || !pending.mediaRevision ||
      pending.mediaRevision != Storage.mediaRevision() || !pending.resourceRevision ||
      pending.resourceRevision != Storage.resourceRevision())
    return;
  const size_t replace = replacement(pending.kind);
  auto& scene = scenes[replace];
  scene.valid = false;
  const auto region = pending.region;
  if (renderer.copyRegionToBuffer(region.x, region.y, region.width, region.height, cache->pixels + replace * SLOT_BYTES,
                                  pending.bytes)) {
    scene = pending;
    scene.age = ++age;
    queuePack(replace);
  }
  pending.valid = false;
}
bool scenePackPending() { return cache && cache->pack.pending(); }
void cancelScenePack() {
  if (!cache) return;
  cache->pack.cancel();
  cache->spillSlot = SLOT_COUNT;
}
void stepScenePack() {
  if (!scenePackPending() || cache->spillSlot >= SLOT_COUNT) return;
  const auto& scene = cache->scenes[cache->spillSlot];
  if (!scene.valid || scene.age != cache->spillAge || scene.revision != revision.load() ||
      scene.libraryRevision != library::libraryContentRevision() ||
      scene.resourceRevision != Storage.resourceRevision() || scene.mediaRevision != Storage.mediaRevision()) {
    cancelScenePack();
    return;
  }
  cache->pack.step();
}
}  // namespace homecover
