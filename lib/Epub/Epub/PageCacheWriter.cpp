#include "PageCacheWriter.h"

#include <Logging.h>

#include <algorithm>
#include <cstring>

PageCacheWriter::PageCacheWriter(HalFile& file) : file(file), buffer(HalMemory::allocatePsram(BUFFER_SIZE)) {
  // One 4 KiB workspace per build: too large for the stack, with no permanent static reservation.
  if (!buffer) LOG_ERR("SCT", "Page write buffer unavailable (%u bytes); using direct writes", BUFFER_SIZE);
}

size_t PageCacheWriter::write(const uint8_t value) { return write(&value, 1); }

size_t PageCacheWriter::write(const uint8_t* data, const size_t size) {
  if (failed || size == 0) return 0;
  if (deferring && size <= STAGING_BYTES - staged) {
    std::memcpy(staging.get() + staged, data, size);
    staged += size;
    return size;
  }
  // Overflow is a bounded fallback; never drop bytes or change the serialized format.
  if (staging && !flushAll()) return 0;
  if (!buffer) {
    const size_t written = file.write(data, size);
    failed = written != size;
    return written;
  }
  size_t remaining = size;
  while (remaining > 0) {
    const size_t chunk = std::min(remaining, BUFFER_SIZE - used);
    memcpy(buffer.get() + used, data, chunk);
    used += chunk;
    data += chunk;
    remaining -= chunk;
    if (used == BUFFER_SIZE && !drain()) return 0;
  }
  return size;
}

bool PageCacheWriter::drain() {
  if (failed) return false;
  if (used == 0) return true;
  if (file.write(buffer.get(), used) != used) {
    failed = true;
    return false;
  }
  used = 0;
  return true;
}

bool PageCacheWriter::deferPersistence() {
  if (staging || !drain()) return false;
  const auto heap = HalMemory::getPsramHeap();
  // One build-owned spool, too large for stack/internal RAM; released on flush or cancellation.
  if (heap.freeBytes >= STAGING_BYTES + 2 * 1024 * 1024 && heap.largestBlockBytes >= STAGING_BYTES)
    staging = HalMemory::allocatePsram(STAGING_BYTES);
  if (!staging) {
    LOG_ERR("SCT", "Preview spool unavailable (%u bytes); writing directly", unsigned(STAGING_BYTES));
    return false;
  }
  stagingBase = file.position();
  staged = flushed = 0;
  deferring = true;
  return true;
}
void PageCacheWriter::allowPersistence() {
  deferring = false;
  if (!staged) staging.reset();
}
bool PageCacheWriter::flushSome(size_t budget) {
  if (failed) return false;
  if (!staging || deferring) return true;
  const size_t count = std::min({budget, size_t(8192), staged - flushed});
  if (count && file.write(staging.get() + flushed, count) != count) {
    failed = true;
    return false;
  }
  flushed += count;
  if (flushed == staged) {
    staging.reset();
    staged = flushed = 0;
  }
  return true;
}
bool PageCacheWriter::flushAll() {
  allowPersistence();
  while (staging)
    if (!flushSome()) return false;
  return drain();
}
size_t PageCacheWriter::position() const { return staging ? stagingBase + staged : file.position() + used; }
const uint8_t* PageCacheWriter::resident(size_t offset, size_t bytes) const {
  if (!staging || offset < stagingBase || offset - stagingBase > staged || bytes > staged - (offset - stagingBase))
    return nullptr;
  return staging.get() + offset - stagingBase;
}
