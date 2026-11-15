#include "PreparedCoverCache.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <Logging.h>
#include <Performance.h>

#include <algorithm>
#include <cstring>
#include <iterator>

#include "../home/CoverBitmapPainter.h"

namespace libraryui {
namespace {
constexpr uint8_t Magic[] = {'C', 'N', 'A', 'T', '0', '0', '1', 0};
uint32_t get32(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
uint64_t get64(const uint8_t* p) { return get32(p) | uint64_t(get32(p + 4)) << 32; }
void put32(uint8_t* p, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) p[i] = value >> (8 * i);
}
void put64(uint8_t* p, uint64_t value) {
  put32(p, value);
  put32(p + 4, value >> 32);
}
}  // namespace

namespace {
// 24 exact thumbnail paths, about 3.5 KiB of fixed S3 metadata; no pixel copies or heap churn.
struct VerifiedSource {
  char path[96]{};
  uint64_t key = 0, fileBytes = 0;
  PreparedCoverCache::Info info;
  PreparedCoverCache::SourceStamp stamp;
};
static_assert(sizeof(VerifiedSource) <= 144);
VerifiedSource verifiedSources[24];
size_t nextVerified = 0;
}  // namespace

PreparedCoverCache::SourceStamp PreparedCoverCache::sourceStamp() {
  return {Storage.resourceRevision(), Storage.mediaRevision()};
}

bool PreparedCoverCache::sourceValidated(const char* path, uint64_t key, uint64_t fileBytes, const Info& info,
                                         SourceStamp stamp) {
  if (!path || !*path || !stamp.content || !stamp.media) return false;
  for (const auto& entry : verifiedSources) {
    if (entry.stamp == stamp && entry.key == key && entry.fileBytes == fileBytes && !std::strcmp(entry.path, path) &&
        entry.info.source == info.source && entry.info.bytes == info.bytes && entry.info.checksum == info.checksum &&
        entry.info.sourceChecksum == info.sourceChecksum)
      return true;
  }
  return false;
}

void PreparedCoverCache::rememberSource(const char* path, uint64_t key, uint64_t fileBytes, const Info& info,
                                        SourceStamp stamp) {
  if (!path || !*path || std::strlen(path) >= sizeof(verifiedSources[0].path) || !stamp.content || !stamp.media ||
      !(stamp == sourceStamp()) || sourceValidated(path, key, fileBytes, info, stamp))
    return;
  auto& entry = verifiedSources[nextVerified];
  nextVerified = (nextVerified + 1) % std::size(verifiedSources);
  std::strcpy(entry.path, path);
  entry.key = key;
  entry.fileBytes = fileBytes;
  entry.info = info;
  entry.stamp = stamp;
}

uint32_t PreparedCoverCache::checksum(const uint8_t* data, size_t bytes) {
  return updateChecksum(2166136261u, data, bytes);
}
uint32_t PreparedCoverCache::updateChecksum(uint32_t value, const uint8_t* data, size_t bytes) {
  for (size_t i = 0; i < bytes; ++i) value = (value ^ data[i]) * 16777619u;
  return value;
}
uint64_t PreparedCoverCache::key(const GfxRenderer& renderer, freeink::ui::Rect area) {
  if (renderer.getRenderMode() != GfxRenderer::BW || area.empty() || area.x < 0 || area.y < 0 ||
      area.right() > renderer.getScreenWidth() || area.bottom() > renderer.getScreenHeight())
    return 0;
  uint64_t value = 0x434e4154303031ULL;
  for (const int number : {int(renderer.getOrientation()), renderer.getScreenWidth(), renderer.getScreenHeight(),
                           int(area.width), int(area.height)})
    value = (value ^ uint32_t(number)) * 1099511628211ULL;
  return value;
}
bool PreparedCoverCache::canDraw(const GfxRenderer& renderer, freeink::ui::Rect area) {
  const auto clip = renderer.getClipRect();
  return key(renderer, area) && clip[0] <= area.x && clip[1] <= area.y && clip[0] + clip[2] >= area.right() &&
         clip[1] + clip[3] >= area.bottom();
}
uint32_t PreparedCoverCache::sourceBytes(HalFile& file) {
  uint8_t header[14];
  if (!file.seek(0) || file.read(header, sizeof(header)) != sizeof(header) || header[0] != 'B' || header[1] != 'M')
    return 0;
  const auto size = get32(header + 2);
  return size >= 54 && size <= file.fileSize64() ? size : 0;
}
bool PreparedCoverCache::inspect(HalFile& file, uint64_t expected, size_t bytes, Info& info) {
  const auto size = file.fileSize64();
  if (!expected || !bytes || bytes > Capacity || size < 54 + bytes + FooterBytes) return false;
  uint8_t data[FooterBytes];
  if (!file.seek64(size - FooterBytes) || file.read(data, sizeof(data)) != sizeof(data) ||
      std::memcmp(data, Magic, sizeof(Magic)) || get64(data + 8) != expected || get32(data + 20) != bytes ||
      get32(data + 36) != checksum(data, 36))
    return false;
  info = {get32(data + 16), get32(data + 20), get32(data + 24), get32(data + 28)};
  return info.source <= SourceLimit && uint64_t(info.source) + info.bytes + FooterBytes == size &&
         sourceBytes(file) == info.source;
}
bool PreparedCoverCache::begin() {
  if (arena) return true;
  if (attempted) return false;
  attempted = true;
  const auto heap = HalMemory::getPsramHeap();
  if (heap.freeBytes < 2 * Capacity + 512 * 1024 || heap.largestBlockBytes < 256 * 1024) return false;
  // Two bounded cover regions (pixels + temporary restoration), 32 KiB total.
  // Reused in PSRAM instead of a second framebuffer or repeated per-cover allocation.
  arena = HalMemory::allocatePsram(2 * Capacity);
  if (!arena) LOG_ERR("LIB", "PSRAM native cover workspace unavailable (%u bytes)", unsigned(2 * Capacity));
  return bool(arena);
}
bool PreparedCoverCache::load(HalFile& file, GfxRenderer& renderer, freeink::ui::Rect area, uint64_t expected,
                              bool draw, const char* path, const SourceStamp* beforeOpen) {
  const auto stamp = sourceStamp();
  if (beforeOpen && !(*beforeOpen == stamp)) return false;
  Info info;
  const auto bytes = renderer.getPackedRegionByteSize(area.x, area.y, area.width, area.height);
  if (writing() || (draw && !canDraw(renderer, area)) || !inspect(file, expected, bytes, info) || !begin() ||
      !file.seek(0))
    return false;
  const bool reuse = beforeOpen && sourceValidated(path, expected, file.fileSize64(), info, stamp);
  performance::count(reuse ? performance::Counter::CoverSourceReuse : performance::Counter::CoverSourceRead);
  if (!reuse) {
    uint32_t hash = 2166136261u;
    for (size_t at = 0; at < info.source; at += Chunk) {
      const size_t count = std::min(Chunk, size_t(info.source) - at);
      if (file.read(arena.get() + Capacity, count) != static_cast<int>(count)) return false;
      hash = updateChecksum(hash, arena.get() + Capacity, count);
    }
    if (hash != info.sourceChecksum) return false;
  }
  if (!file.seek(info.source)) return false;
  for (size_t at = 0; at < bytes; at += Chunk) {
    const size_t count = std::min(Chunk, bytes - at);
    if (file.read(arena.get() + at, count) != static_cast<int>(count)) return false;
  }
  if (checksum(arena.get(), bytes) != info.checksum || !(stamp == sourceStamp())) return false;
  if (beforeOpen) rememberSource(path, expected, file.fileSize64(), info, stamp);
  if (!draw) return true;
  if (!renderer.copyPackedBufferToRegion(area.x, area.y, area.width, area.height, arena.get(), bytes)) return false;
  renderer.preserveImagePolarity(area.x, area.y, area.width, area.height);
  return true;
}
bool PreparedCoverCache::prepare(GfxRenderer& renderer, Bitmap& bitmap, freeink::ui::Rect area, uint64_t expected,
                                 const char* path, bool (*continueFn)(void*), void* context) {
  const auto bytes = renderer.getPackedRegionByteSize(area.x, area.y, area.width, area.height);
  if (!expected || writing() || !bytes || bytes > Capacity || !begin() || (continueFn && !continueFn(context)))
    return false;
  auto* backup = arena.get() + Capacity;
  if (!renderer.copyPackedRegionToBuffer(area.x, area.y, area.width, area.height, backup, bytes)) return false;
  const auto clip = renderer.getClipRect();
  renderer.setClipRect(area.x, area.y, area.width, area.height);
  bitmap.enableSourceChecksum(true);
  const bool painted = bitmap.parseHeaders() == BmpReaderError::Ok && bitmap.getBpp() <= 2 &&
                       uint64_t(bitmap.getRowBytes()) * bitmap.getHeight() <= SourceLimit &&
                       homecover::paintBitmap(renderer, bitmap, area, true, 0, continueFn, context, false);
  bitmap.enableSourceChecksum(false);
  renderer.setClipRect(clip[0], clip[1], clip[2], clip[3]);
  const bool copied =
      painted && renderer.copyPackedRegionToBuffer(area.x, area.y, area.width, area.height, arena.get(), bytes);
  const bool restored = renderer.copyPackedBufferToRegion(area.x, area.y, area.width, area.height, backup, bytes);
  if (!restored) LOG_ERR("LIB", "Cannot restore cover preparation region");
  if (!copied || !restored || (continueFn && !continueFn(context))) return false;
  return store(path, expected, bytes, bitmap.sourceChecksum(), bitmap.sourceBytesRead());
}
bool PreparedCoverCache::store(const char* path, uint64_t expected, size_t bytes, uint32_t expectedHash,
                               size_t readBytes) {
  if (!Storage.openDerivedCoverForUpdate("LIB", path, readBytes, writer)) return false;
  const auto source = sourceBytes(writer);
  if (!source || source > SourceLimit || source != readBytes || !writer.seek(0)) {
    cancel();
    return false;
  }
  std::memcpy(footer, Magic, sizeof(Magic));
  put64(footer + 8, expected);
  put32(footer + 16, source);
  put32(footer + 20, bytes);
  put32(footer + 24, checksum(arena.get(), bytes));
  put32(footer + 28, 0);
  put32(footer + 32, 0);
  put32(footer + 36, 0);
  written = 0;
  length = bytes;
  sourceLength = source;
  sourceRead = 0;
  sourceHash = 2166136261u;
  expectedSourceHash = expectedHash;
  phase = Phase::Source;
  return true;
}
void PreparedCoverCache::step() {
  if (!writing()) return;
  switch (phase) {
    case Phase::Source: {
      const size_t count = std::min(Chunk, size_t(sourceLength - sourceRead));
      if (writer.read(arena.get() + Capacity, count) != static_cast<int>(count)) {
        cancel();
        return;
      }
      sourceHash = updateChecksum(sourceHash, arena.get() + Capacity, count);
      sourceRead += count;
      if (sourceRead < sourceLength) return;
      if (sourceHash != expectedSourceHash) {
        cancel();
        return;
      }
      put32(footer + 28, sourceHash);
      put32(footer + 36, checksum(footer, 36));
      if (!writer.truncate(sourceLength) || !writer.seek(sourceLength)) {
        cancel();
        return;
      }
      phase = Phase::Pixels;
      return;
    }
    case Phase::Pixels: {
      const size_t count = std::min(Chunk, length - written);
      if (writer.write(arena.get() + written, count) != count) {
        cancel();
        return;
      }
      written += count;
      if (written == length) phase = Phase::Footer;
      return;
    }
    case Phase::Footer:
      if (writer.write(footer, sizeof(footer)) != sizeof(footer)) {
        cancel();
        return;
      }
      writer.flush();
      cancel();
      return;
  }
}
void PreparedCoverCache::cancel() {
  if (writer.isOpen()) writer.close();
  written = length = 0;
}
void PreparedCoverCache::release() {
  cancel();
  arena.reset();
  attempted = false;
}
}  // namespace libraryui
