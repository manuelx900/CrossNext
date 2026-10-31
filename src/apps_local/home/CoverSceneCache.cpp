#include "CoverSceneCache.h"

#include <GfxRenderer.h>
#include <HalMemory.h>
#include <HalStorage.h>
#include <Logging.h>

#include <array>
#include <atomic>
#include <cstring>
#include <new>

namespace homecover {
namespace {
constexpr size_t SLOT_BYTES = 48 * 1024;
constexpr size_t SLOT_COUNT = 3;
struct Source {
  char path[256]{};
  uint64_t size = 0;
  uint32_t modified = 0;
};
struct Scene {
  std::array<Source, 7> sources;
  freeink::ui::Rect region;
  uint64_t key = 0;
  uint32_t age = 0;
  uint32_t revision = 0;
  size_t bytes = 0;
  uint8_t count = 0;
  bool valid = false;
};
struct Cache {
  std::array<Scene, SLOT_COUNT> scenes{};
  Scene pending{};
  uint8_t pixels[SLOT_BYTES * SLOT_COUNT];
};
HalMemory::PsramBuffer arena;
Cache* cache = nullptr;
uint32_t age = 0;
bool attempted = false;
std::atomic<uint32_t> revision{0};

bool current(const Source& source) {
  HalFile file;
  return Storage.openFileForRead("HOME", source.path, file) && file.fileSize64() == source.size &&
         file.modificationTime() == source.modified;
}
}  // namespace

bool restoreScene(GfxRenderer& renderer, freeink::ui::Rect region, uint64_t key) {
  if (!attempted) {
    attempted = true;
    // Three art-only snapshots and their source metadata survive Home in PSRAM.
    // One bounded arena (~152 KiB) avoids internal RAM and per-return allocations.
    arena = HalMemory::allocatePsram(sizeof(Cache));
    if (!arena) {
      LOG_ERR("HOME", "PSRAM art cache unavailable (%u bytes)", unsigned(sizeof(Cache)));
      return false;
    }
    cache = new (arena.get()) Cache;
  }
  if (!cache) return false;
  auto& pending = cache->pending;
  auto& scenes = cache->scenes;
  pending.valid = false;
  const auto currentRevision = revision.load();
  for (size_t n = 0; n < scenes.size(); ++n) {
    auto& scene = scenes[n];
    if (scene.revision != currentRevision) scene.valid = false;
    if (!scene.valid || scene.key != key || scene.region.x != region.x || scene.region.y != region.y ||
        scene.region.width != region.width || scene.region.height != region.height)
      continue;
    bool valid = true;
    for (int i = 0; i < scene.count && valid; ++i) valid = current(scene.sources[i]);
    if (valid && renderer.copyBufferToRegion(region.x, region.y, region.width, region.height,
                                             cache->pixels + n * SLOT_BYTES, scene.bytes)) {
      scene.age = ++age;
      return true;
    }
    scene.valid = false;
  }
  pending.key = key;
  pending.region = region;
  pending.bytes = renderer.getRegionByteSize(region.x, region.y, region.width, region.height);
  pending.count = 0;
  pending.revision = currentRevision;
  pending.valid = pending.bytes > 0 && pending.bytes <= SLOT_BYTES;
  return false;
}

void invalidateScenes() { revision.fetch_add(1); }

void recordSource(const char* path, HalFile& file, bool valid) {
  if (!cache) return;
  auto& pending = cache->pending;
  if (!pending.valid) return;
  if (!valid || pending.count == pending.sources.size() || std::strlen(path) >= sizeof(Source::path)) {
    pending.valid = false;
    return;
  }
  auto& source = pending.sources[pending.count++];
  source.modified = file.modificationTime();
  // An undated file cannot be validated reliably across activity lifetimes.
  if (!source.modified) {
    pending.valid = false;
    return;
  }
  source.size = file.fileSize64();
  std::strcpy(source.path, path);
}

void storeScene(GfxRenderer& renderer) {
  if (!cache) return;
  auto& pending = cache->pending;
  auto& scenes = cache->scenes;
  if (!pending.valid || !pending.count || pending.revision != revision.load()) return;
  size_t replace = 0;
  for (size_t n = 0; n < scenes.size(); ++n) {
    if (!scenes[n].valid) {
      replace = n;
      break;
    }
    if (scenes[n].age < scenes[replace].age) replace = n;
  }
  auto& scene = scenes[replace];
  scene.valid = false;
  const auto region = pending.region;
  if (renderer.copyRegionToBuffer(region.x, region.y, region.width, region.height, cache->pixels + replace * SLOT_BYTES,
                                  pending.bytes)) {
    scene = pending;
    scene.age = ++age;
  }
  pending.valid = false;
}
}  // namespace homecover
