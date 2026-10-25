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
