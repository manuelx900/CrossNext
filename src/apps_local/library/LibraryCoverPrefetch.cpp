#include "LibraryCoverPrefetch.h"

#include <Logging.h>
#include <Performance.h>

#include <algorithm>
#include <cstring>

#include "PreparedCoverCache.h"

namespace libraryui {
void CoverPrefetch::begin(uint64_t nextKey, uint32_t nextContentRevision, uint32_t nextArtRevision) {
  const auto media = Storage.mediaRevision();
  if (key == nextKey && contentRevision == nextContentRevision && artRevision == nextArtRevision && media &&
      mediaRevision == media)
    return;
  mediaRevision = media;
  cancelLoad();
  key = nextKey;
  contentRevision = nextContentRevision;
  artRevision = nextArtRevision;
  entries = {};
  used = 0;
}

CoverPrefetch::Result CoverPrefetch::load(size_t slot, int height, const char* path, uint64_t nativeKey,
                                          size_t nativeBytes) {
  if (file.isOpen() && !(loadingStamp == PreparedCoverCache::sourceStamp())) {
    cancelLoad();
    return Result::Unavailable;
  }
  if (!Storage.mediaRevision()) return Result::Unavailable;
  if (mediaRevision != Storage.mediaRevision()) begin(key, contentRevision, artRevision);
  if (!key || slot >= entries.size() || height <= 0) return Result::Unavailable;
  if (entries[slot].size && entries[slot].height == height &&
      (!entries[slot].nativeKey || entries[slot].nativeKey == nativeKey))
    return Result::Ready;
  if (!attempted) {
    attempted = true;
    const auto heap = HalMemory::getPsramHeap();
    constexpr size_t HEADROOM = 512 * 1024;
    if (heap.freeBytes < CAPACITY + HEADROOM || heap.largestBlockBytes < CAPACITY) return Result::Unavailable;
    // One next-page byte arena, at most 128 KiB; allocated once on idle, never on navigation.
    // PSRAM avoids a second framebuffer and leaves the internal heap untouched.
    buffer = HalMemory::allocatePsram(CAPACITY);
    if (!buffer) LOG_ERR("LIB", "PSRAM cover prefetch unavailable (%u bytes)", unsigned(CAPACITY));
  }
  if (!buffer) return Result::Unavailable;
  if (loadingSlot != slot || loadingHeight != height) cancelLoad();
  if (!file.isOpen()) {
    loadingStamp = PreparedCoverCache::sourceStamp();
    if (!Storage.openFileForRead("LIB", path, file)) return Result::Unavailable;
    sourcePath[0] = 0;
    if (std::strlen(path) < sizeof(sourcePath)) std::strcpy(sourcePath, path);
    sourceFileBytes = file.fileSize64();
    PreparedCoverCache::Info info;
    loadingNative = nativeKey && PreparedCoverCache::inspect(file, nativeKey, nativeBytes, info);
    sourceSize = loadingNative ? info.source : PreparedCoverCache::sourceBytes(file);
    const auto size = loadingNative ? info.bytes : sourceSize ? sourceSize : file.fileSize64();
    if ((!loadingNative && size < 54) || !size || size > CAPACITY - used || !file.seek(0)) {
      file.close();
      return Result::Unavailable;
    }
    loadingSlot = slot;
    loadingHeight = height;
    loadingSize = static_cast<size_t>(size);
    loaded = 0;
    sourceInfo = info;
    validatingSource = loadingNative &&
                       !PreparedCoverCache::sourceValidated(sourcePath, nativeKey, sourceFileBytes, info, loadingStamp);
    if (loadingNative)
      performance::count(validatingSource ? performance::Counter::CoverSourceRead
                                          : performance::Counter::CoverSourceReuse);
    sourceRead = 0;
    sourceHash = 2166136261u;
    expectedSourceHash = loadingNative ? info.sourceChecksum : 0;
    expectedPixelHash = loadingNative ? info.checksum : 0;
    loadingNativeKey = loadingNative ? nativeKey : 0;
    if (loadingNative && !validatingSource && !file.seek(sourceSize)) {
      cancelLoad();
      return Result::Unavailable;
    }
  }
  if (validatingSource) {
    const size_t count = std::min({CHUNK_BYTES, size_t(sourceSize - sourceRead), CAPACITY - used});
    if (file.read(buffer.get() + used, count) != static_cast<int>(count)) {
      cancelLoad();
      return Result::Unavailable;
    }
    sourceHash = PreparedCoverCache::updateChecksum(sourceHash, buffer.get() + used, count);
    sourceRead += count;
    if (sourceRead < sourceSize) return Result::Pending;
    validatingSource = false;
    if (sourceHash != expectedSourceHash) {
      loadingNative = false;
      loadingNativeKey = 0;
      loadingSize = sourceSize;
      if (sourceSize > CAPACITY - used || !file.seek(0)) {
        cancelLoad();
        return Result::Unavailable;
      }
    } else if (!file.seek(sourceSize)) {
      cancelLoad();
      return Result::Unavailable;
    }
    return Result::Pending;
  }
  const size_t chunk = std::min(CHUNK_BYTES, loadingSize - loaded);
  if (file.read(buffer.get() + used + loaded, chunk) != static_cast<int>(chunk)) {
    cancelLoad();
    return Result::Unavailable;
  }
  loaded += chunk;
  if (loaded < loadingSize) return Result::Pending;
  if (loadingNative && PreparedCoverCache::checksum(buffer.get() + used, loadingSize) != expectedPixelHash) {
    cancelLoad();
    return Result::Unavailable;
  }
  if (!(loadingStamp == PreparedCoverCache::sourceStamp())) {
    cancelLoad();
    return Result::Unavailable;
  }
  if (loadingNative)
    PreparedCoverCache::rememberSource(sourcePath, loadingNativeKey, sourceFileBytes, sourceInfo, loadingStamp);
  entries[slot] = {used, loadingSize, height, loadingNativeKey};
  used += loadingSize;
  cancelLoad();
  return Result::Ready;
}

CoverPrefetch::Bytes CoverPrefetch::find(uint64_t sceneKey, uint32_t revision, uint32_t sceneRevision, size_t slot,
                                         int height, uint64_t nativeKey) const {
  if (!buffer || !mediaRevision || mediaRevision != Storage.mediaRevision() || !sceneKey || key != sceneKey ||
      contentRevision != revision || artRevision != sceneRevision || slot >= entries.size())
    return {};
  const auto& entry = entries[slot];
  return entry.size && entry.height == height && (!entry.nativeKey || entry.nativeKey == nativeKey)
             ? Bytes{buffer.get() + entry.offset, entry.size, entry.nativeKey != 0}
             : Bytes{};
}

bool CoverPrefetch::promoteLast(size_t slot, int height, uint64_t nativeKey, const uint8_t* pixels, size_t bytes) {
  if (!buffer || loading() || slot >= entries.size() || !nativeKey || !pixels || !bytes) return false;
  auto& entry = entries[slot];
  if (!entry.size || entry.height != height || entry.offset + entry.size != used || bytes > CAPACITY - entry.offset)
    return false;
  // Replace only the tail, before another cover is loaded; no arena growth or source reread.
  std::memcpy(buffer.get() + entry.offset, pixels, bytes);
  entry.size = bytes;
  entry.nativeKey = nativeKey;
  used = entry.offset + bytes;
  return true;
}

void CoverPrefetch::cancelLoad() {
  if (file.isOpen()) file.close();
  loadingSlot = MAX_COVERS;
  loaded = loadingSize = 0;
}

void CoverPrefetch::release() {
  cancelLoad();
  buffer.reset();
  entries = {};
  key = 0;
  used = 0;
  attempted = false;
}
}  // namespace libraryui
