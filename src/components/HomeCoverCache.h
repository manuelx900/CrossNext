#pragma once

#include <Bitmap.h>
#include <FreeInkUICore.h>
#include <HalMemory.h>
#include <HalStorage.h>

#include <array>
#include <string>

#include "apps_local/library/PreparedCoverCache.h"

class GfxRenderer;

// Screen-lifetime thumbnail decoding and PSRAM region snapshots.
class HomeCoverCache {
 public:
  static constexpr size_t MAX_COVERS = 7;
  explicit HomeCoverCache(GfxRenderer& renderer) : renderer(renderer) {}
  void begin(bool flat = false);
  void prepare();
  void invalidate();
  void invalidate(size_t index);
  bool paint(freeink::ui::Rect rect, size_t index, const std::string& path, bool flat = false, bool fillSlot = false);
  bool hasImage(size_t index) const { return index < cachedCovers.size() && cachedCovers[index].image; }

 private:
  struct CachedCover {
    freeink::ui::Rect rect{};
    size_t offset = 0;
    size_t bytes = 0;
    bool valid = false;
    bool image = false;
    bool flat = false;
    bool fillSlot = false;
  };
  HalMemory::PsramBuffer coverCache;
  // Native validation needs separate 32 KiB scratch: cached cover pixels must remain intact.
  libraryui::PreparedCoverCache nativeCover;
  bool paintFlatBitmap(freeink::ui::Rect rect, bool fillSlot);
  std::array<CachedCover, MAX_COVERS> cachedCovers{};
  size_t coverCacheCapacity = 0;
  size_t coverCacheUsed = 0;
  int coverCacheOrientation = -1;

  HalFile coverFile;
  Bitmap coverBitmap{coverFile};
  GfxRenderer& renderer;
};
