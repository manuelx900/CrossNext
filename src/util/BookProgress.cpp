#include "BookProgress.h"

#include <Epub.h>
#include <FsHelpers.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <Xtc.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <functional>

namespace {
uint32_t readLe32(const uint8_t* p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
}  // namespace

static int loadBookProgressUncached(const std::string& path) {
  uint8_t data[10]{};
  if (FsHelpers::hasReflowableBookExtension(path)) {
    // Metadata objects exceed the stack budget; only the featured book is loaded, once per entry.
    auto epub = makeUniqueNoThrow<Epub>(path, "/.crosspoint");
    if (!epub) {
      LOG_ERR("HOME", "OOM: progress metadata");
      return -1;
    }
    if (!epub->load(false, true)) return -1;
    HalFile file;
    if (!Storage.openFileForRead("HOME", epub->getCachePath() + "/progress.bin", file)) return -1;
    const int size = file.read(data, sizeof(data));
    if (size != 4 && size != 6 && size != 10) return -1;
    const int spine = data[0] | (data[1] << 8);
    const int page = data[2] | (data[3] << 8);
    const int total = size >= 6 ? data[4] | (data[5] << 8) : 0;
    if (epub->getSpineItemsCount() <= 0 || epub->getBookSize() == 0) return -1;
    if (spine == epub->getSpineItemsCount()) return 100;
    if (spine > epub->getSpineItemsCount()) return -1;
    const float fraction = total > 0 && page != UINT16_MAX ? std::clamp(float(page) / total, 0.0f, 1.0f) : 0;
    return std::clamp(static_cast<int>(epub->calculateProgress(spine, fraction) * 100 + 0.5f), 0, 100);
  }
  if (FsHelpers::hasXtcExtension(path)) {
    auto xtc = makeUniqueNoThrow<Xtc>(path, "/.crosspoint");
    if (!xtc) {
      LOG_ERR("HOME", "OOM: XTC progress metadata");
      return -1;
    }
    if (!xtc->load()) return -1;
    HalFile file;
    if (!Storage.openFileForRead("HOME", xtc->getCachePath() + "/progress.bin", file) || file.read(data, 4) != 4)
      return -1;
    const uint32_t page = readLe32(data);
    if (xtc->getPageCount() == 0) return -1;
    if (page >= xtc->getPageCount()) return 100;
    return xtc->calculateProgress(page);
  }
  return -1;
}

int loadBookProgress(const std::string& path) {
  // One bounded result survives Home recreation; no parser or book data stays resident.
  static struct {
    char path[512]{};
    uint8_t progress[10]{};
    uint64_t sourceBytes = 0, metadataBytes = 0;
    uint32_t sourceModified = 0, metadataModified = 0;
    int progressBytes = 0, percent = -1;
  } cached;
  const bool epub = FsHelpers::hasReflowableBookExtension(path);
  if ((!epub && !FsHelpers::hasXtcExtension(path)) || path.size() >= sizeof(cached.path))
    return loadBookProgressUncached(path);
  char filePath[96];
  const auto hash = static_cast<unsigned long>(std::hash<std::string>{}(path));
  std::snprintf(filePath, sizeof(filePath), "/.crosspoint/%s_%lu/progress.bin", epub ? "epub" : "xtc", hash);
  uint8_t progress[10]{};
  int length = 0;
  uint64_t sourceBytes = 0, metadataBytes = 0;
  uint32_t sourceModified = 0, metadataModified = 0;
  {
    HalFile file;
    if (!Storage.openFileForRead("HOME", filePath, file)) return -1;
    length = file.read(progress, sizeof(progress));
    if (length != 4 && length != 6 && length != 10) return -1;
  }
  {
    HalFile file;
    if (!Storage.openFileForRead("HOME", path, file)) return -1;
    sourceBytes = file.fileSize64();
    sourceModified = file.modificationTime();
  }
  if (epub) {
    std::snprintf(filePath, sizeof(filePath), "/.crosspoint/epub_%lu/book.bin", hash);
    HalFile file;
    if (!Storage.openFileForRead("HOME", filePath, file)) return -1;
    metadataBytes = file.fileSize64();
    metadataModified = file.modificationTime();
  }
  if (cached.percent >= 0 && !std::strcmp(cached.path, path.c_str()) && cached.progressBytes == length &&
      !std::memcmp(cached.progress, progress, sizeof(progress)) && cached.sourceBytes == sourceBytes &&
      cached.sourceModified == sourceModified && cached.metadataBytes == metadataBytes &&
      cached.metadataModified == metadataModified)
    return cached.percent;
  const int percent = loadBookProgressUncached(path);
  if (percent >= 0) {
    std::strcpy(cached.path, path.c_str());
    std::memcpy(cached.progress, progress, sizeof(progress));
    cached.progressBytes = length;
    cached.sourceBytes = sourceBytes;
    cached.sourceModified = sourceModified;
    cached.metadataBytes = metadataBytes;
    cached.metadataModified = metadataModified;
    cached.percent = percent;
  }
  return percent;
}
