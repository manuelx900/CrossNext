#pragma once

#include <HalMemory.h>
#include <HalStorage.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "PreparedCoverCache.h"

namespace libraryui {
// Render-owner only. A single next-page snapshot contains BMP bytes or neutral native pixels.
class CoverPrefetch {
 public:
  static constexpr size_t CAPACITY = 128 * 1024;
  static constexpr size_t CHUNK_BYTES = 4096;
  static constexpr size_t MAX_COVERS = 9;
  enum class Result : uint8_t { Pending, Ready, Unavailable };
  struct Bytes {
    const uint8_t* data = nullptr;
    size_t size = 0;
    bool native = false;
    explicit operator bool() const { return data != nullptr; }
  };

  void begin(uint64_t key, uint32_t contentRevision, uint32_t artRevision);
  Result load(size_t slot, int height, const char* path, uint64_t nativeKey = 0, size_t nativeBytes = 0);
  Bytes find(uint64_t key, uint32_t contentRevision, uint32_t artRevision, size_t slot, int height,
             uint64_t nativeKey = 0) const;
  bool promoteLast(size_t slot, int height, uint64_t nativeKey, const uint8_t* pixels, size_t bytes);
  void cancelLoad();
  void release();
  size_t bytesUsed() const { return used; }
  bool enabled() const { return !attempted || buffer != nullptr; }
  bool loading() const { return file.isOpen(); }

 private:
  struct Entry {
    size_t offset = 0, size = 0;
    int height = 0;
    uint64_t nativeKey = 0;
  };
  HalMemory::PsramBuffer buffer;
  HalFile file;
  std::array<Entry, MAX_COVERS> entries{};
  PreparedCoverCache::SourceStamp loadingStamp;
  PreparedCoverCache::Info sourceInfo;
  char sourcePath[96]{};
  uint64_t sourceFileBytes = 0;
  uint32_t mediaRevision = 0;
  uint64_t key = 0;
  uint32_t contentRevision = 0, artRevision = 0;
  size_t used = 0, loaded = 0, loadingSize = 0, loadingSlot = MAX_COVERS;
  int loadingHeight = 0;
  bool attempted = false;
  bool loadingNative = false, validatingSource = false;
  uint64_t loadingNativeKey = 0;
  uint32_t sourceRead = 0, sourceSize = 0, sourceHash = 2166136261u, expectedSourceHash = 0, expectedPixelHash = 0;
};
}  // namespace libraryui
