#include "HomeCoverCache.h"

#include <GfxRenderer.h>
#include <Logging.h>

#include <algorithm>
#include <optional>

#include "../apps_local/home/CoverBoxSampler.h"
#include "../apps_local/home/CoverFit.h"
#include "../apps_local/home/CoverScanline.h"
#include "../apps_local/home/CoverSceneCache.h"
#include "UITheme.h"

namespace fui = freeink::ui;

void HomeCoverCache::begin(bool flat) {
  if (flat && !samplingScratch) {
    // Reused area-sampling rows, bounded to a 4096-pixel source and one screen row.
    // PSRAM avoids placing the approximately 12 KiB worst case on the task stack.
    samplingCapacity = std::max(renderer.getScreenWidth(), renderer.getScreenHeight()) * sizeof(homecover::BoxColumn) +
                       sizeof(homecover::BoxBand) + 1024 + 512 + 4096;
    samplingScratch = HalMemory::allocatePsram(samplingCapacity);
    if (!samplingScratch) LOG_ERR("HOME", "PSRAM cover sampling unavailable (%u bytes)", unsigned(samplingCapacity));
  }
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
  const int width = coverBitmap.getWidth(), height = coverBitmap.getHeight();
  const auto art = fillSlot ? rect : homecover::contain(rect, width, height);
  int cropWidth = width, cropHeight = height;
  if (fillSlot) {
    if (int64_t(width) * rect.height > int64_t(height) * rect.width)
      cropWidth = std::max(1, height * rect.width / rect.height);
    else
      cropHeight = std::max(1, width * rect.height / rect.width);
  }
  const int sourceX = (width - cropWidth) / 2, sourceY = (height - cropHeight) / 2;
  renderer.fillRect(rect.x, rect.y, rect.width, rect.height, false);
  if (art.empty()) return false;
  if (!coverBitmap.is1Bit() || (art.width == width && art.height == height))
    return renderer.drawBitmap(coverBitmap, art.x, art.y, art.width, art.height);
  if (!samplingScratch || width > 4096 || height > 8192) return false;
  const size_t columnsBytes = art.width * sizeof(homecover::BoxColumn), bandBytes = sizeof(homecover::BoxBand);
  const size_t prefixBytes = ((width + 7) / 8) * sizeof(uint16_t);
  const size_t rowBytes = coverBitmap.getRowBytes(), offset = columnsBytes + bandBytes + prefixBytes;
  if (offset + rowBytes + 4096 > samplingCapacity) return false;
  auto* data = samplingScratch.get();
  auto* row = data + offset;
  coverBitmap.setReadAheadBuffer(row + rowBytes, 4096);
  std::optional<homecover::BoxSampler> sampler;
  if (cropWidth >= art.width && cropHeight >= art.height)
    sampler.emplace(
        reinterpret_cast<homecover::BoxColumn*>(data), reinterpret_cast<homecover::BoxBand*>(data + columnsBytes),
        reinterpret_cast<uint16_t*>(data + columnsBytes + bandBytes), width, sourceX, cropWidth, cropHeight, art, 0);
  bool ok = true;
  for (int n = 0; n < height; ++n) {
    if (coverBitmap.readNext1BitRow(row) != BmpReaderError::Ok) {
      ok = false;
      break;
    }
    const int y = coverBitmap.isTopDown() ? n : height - 1 - n;
    if (y < sourceY || y >= sourceY + cropHeight) continue;
    const auto draw = [this](int x, int y, int w, int h) { renderer.fillRect(x, y, w, h, true); };
    if (sampler)
      sampler->paint(row, y - sourceY, coverBitmap.isTopDown(), draw);
    else
      homecover::paint1BitRow(row, sourceX, cropWidth, cropHeight, y - sourceY, art, 0, draw);
  }
  coverBitmap.setReadAheadBuffer(nullptr, 0);
  renderer.preserveImagePolarity(art.x, art.y, art.width, art.height);
  return ok;
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
  if (!path.empty() && Storage.openFileForRead("HOME", path, coverFile)) {
    if (coverBitmap.parseHeaders() == BmpReaderError::Ok && coverBitmap.getWidth() > 0 && coverBitmap.getHeight() > 0) {
      // The art nudges a few px right of center; the spine below hugs its
      // left edge either way.
      constexpr int ART_SHIFT = 3;
      if (flat)
        drawn = paintFlatBitmap(rect, fillSlot);
      else
        drawn = GUI.drawCoverThumbFill(renderer, coverBitmap, Rect{rect.x, rect.y, rect.width, rect.height}, ART_SHIFT);
      if (drawn) {
        renderer.drawRect(rect.x, rect.y, rect.width, rect.height);
        // False spine glued to the art's left edge (centered art means that
        // edge moves with each cover's fit margin): a dark band with a
        // dithered crease makes every cover read as a bound book. Drawn here
        // so the PSRAM snapshot below captures it.
        const int artLeft = std::max<int>(rect.x, rect.x + (rect.width - coverBitmap.getWidth()) / 2 + ART_SHIFT);
        if (!flat) {
          renderer.fillRect(artLeft, rect.y, 3, rect.height);
          renderer.fillRectDither(artLeft + 3, rect.y, 2, rect.height, Color::LightGray);
        }
      }
    }
    homecover::recordSource(path.c_str(), coverFile, drawn);
    coverFile.close();
  } else
    homecover::recordSource(path.c_str(), coverFile, false);
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
