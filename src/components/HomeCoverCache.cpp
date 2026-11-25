#include "HomeCoverCache.h"

#include <GfxRenderer.h>
#include <Logging.h>

#include <algorithm>
#include <optional>

#include "../apps_local/home/CoverBitmapPainter.h"
#include "../apps_local/home/CoverFit.h"
#include "../apps_local/home/CoverScanline.h"
#include "UITheme.h"

namespace fui = freeink::ui;

void HomeCoverCache::begin(bool /*flat*/) {
  // Cover regions do not overlap. Each may widen by one physical byte per row.
  if (coverCache) {
    invalidate();
    return;
  }
  coverCacheCapacity = renderer.getRegionByteSize(0, 0, renderer.getScreenWidth(), renderer.getScreenHeight()) +
                       MAX_COVERS * std::max(renderer.getScreenWidth(), renderer.getScreenHeight());
  coverCache = HalMemory::allocatePsram(coverCacheCapacity);
  if (!coverCache) {
    LOG_ERR("HOME", "PSRAM cover cache unavailable (%u bytes); rendering uncached", unsigned(coverCacheCapacity));
    coverCacheCapacity = 0;
  }
}

void HomeCoverCache::invalidate() {
  coverCacheUsed = 0;
  cachedCovers.fill(CachedCover{});
}

void HomeCoverCache::invalidate(size_t index) {
  if (index < cachedCovers.size()) cachedCovers[index].valid = false;
}

void HomeCoverCache::prepare() {
  const int orientation = static_cast<int>(renderer.getOrientation());
  if (coverCacheOrientation != orientation) {
    invalidate();
    coverCacheOrientation = orientation;
  }
}

bool HomeCoverCache::paintFlatBitmap(fui::Rect rect, bool fillSlot) {
  return homecover::paintBitmap(renderer, coverBitmap, rect, fillSlot);
}

bool HomeCoverCache::paint(fui::Rect rect, size_t index, const std::string& path, bool flat, bool fillSlot) {
  if (index >= cachedCovers.size()) return false;
  auto& cached = cachedCovers[index];
  uint8_t* const cacheData = coverCache.get();
  if (coverCache && cached.valid && cached.flat == flat && cached.fillSlot == fillSlot && cached.rect.x == rect.x &&
      cached.rect.y == rect.y && cached.rect.width == rect.width && cached.rect.height == rect.height &&
      renderer.copyBufferToRegion(rect.x, rect.y, rect.width, rect.height, cacheData + cached.offset, cached.bytes)) {
    return true;
  }
  cached.valid = false;
  bool drawn = false;
  const auto before = libraryui::PreparedCoverCache::sourceStamp();
  if (!path.empty() && Storage.openFileForRead("HOME", path, coverFile)) {
    if (flat && fillSlot)
      drawn = nativeCover.load(coverFile, renderer, rect, libraryui::PreparedCoverCache::key(renderer, rect), true,
                               path.c_str(), &before);
    if (!drawn && coverBitmap.parseHeaders() == BmpReaderError::Ok && coverBitmap.getWidth() > 0 &&
        coverBitmap.getHeight() > 0) {
      constexpr int ART_SHIFT = 3;
      if (flat)
        drawn = paintFlatBitmap(rect, fillSlot);
      else
        drawn = GUI.drawCoverThumbFill(renderer, coverBitmap, Rect{rect.x, rect.y, rect.width, rect.height}, ART_SHIFT);
    }
    if (drawn) {
      renderer.drawRect(rect.x, rect.y, rect.width, rect.height);
      if (!flat) {
        constexpr int ART_SHIFT = 3;
        const int artLeft = std::max<int>(rect.x, rect.x + (rect.width - coverBitmap.getWidth()) / 2 + ART_SHIFT);
        renderer.fillRect(artLeft, rect.y, 3, rect.height);
        renderer.fillRectDither(artLeft + 3, rect.y, 2, rect.height, Color::LightGray);
      }
    }
    coverFile.close();
  }
  if (!drawn) {
    if (flat)
      renderer.fillRect(rect.x, rect.y, rect.width, rect.height, false);
    else
      GUI.drawCoverPlaceholder(renderer, Rect{rect.x, rect.y, rect.width, rect.height});
    renderer.drawRect(rect.x, rect.y, rect.width, rect.height);
  }
  cached.image = drawn;
  if (coverCache) {
    const size_t needed = renderer.getRegionByteSize(rect.x, rect.y, rect.width, rect.height);
    if (needed > cached.bytes && needed <= coverCacheCapacity - coverCacheUsed) {
      cached.offset = coverCacheUsed;
      cached.bytes = needed;
      coverCacheUsed += needed;
    }
    if (needed > 0 && needed <= cached.bytes) {
      cached.rect = rect;
      cached.flat = flat;
      cached.fillSlot = fillSlot;
      cached.valid =
          renderer.copyRegionToBuffer(rect.x, rect.y, rect.width, rect.height, cacheData + cached.offset, cached.bytes);
    }
  }
  return true;  // The cover slot is painted, including fallback art.
}
