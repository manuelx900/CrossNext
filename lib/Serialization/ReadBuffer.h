#pragma once

#include <HalStorage.h>

#include <algorithm>
#include <climits>
#include <cstring>
#include <limits>
#include <string>

namespace serialization {

// Exclusive, borrowed read-ahead storage. The owner supplies reusable working
// memory; a null buffer keeps the direct HAL path.
class ReadBuffer {
 public:
  explicit ReadBuffer(HalFile& file, uint8_t* buffer = nullptr, size_t capacity = 0, size_t maxBytes = SIZE_MAX)
      : file(&file), buffer(buffer), capacity(buffer ? capacity : 0), logical(file.position()), end(file.size()) {
    const size_t remaining = end - std::min(end, logical);
    if (maxBytes < remaining) end = logical + maxBytes;
  }
  explicit ReadBuffer(const uint8_t* data, size_t bytes)
      : file(nullptr), buffer(nullptr), capacity(0), logical(0), end(data ? bytes : 0), memory(data) {}
  ReadBuffer(const ReadBuffer&) = delete;
  ReadBuffer& operator=(const ReadBuffer&) = delete;

  int read(void* destination, size_t count) {
    count = std::min(count, end - std::min(end, logical));
    if (!count) return 0;
    if (memory) {
      std::memcpy(destination, memory + logical, count);
      logical += count;
      return static_cast<int>(count);
    }
    if (!capacity) {
      const int result = file->read(destination, count);
      if (result > 0) logical += result;
      return result;
    }
    auto* out = static_cast<uint8_t*>(destination);
    size_t total = 0;
    while (total < count) {
      if (offset == length) {
        const int result = file->read(buffer, std::min(capacity, end - std::min(end, logical)));
        if (result <= 0) return total ? static_cast<int>(total) : result;
        offset = 0;
        length = static_cast<size_t>(result);
      }
      const size_t take = std::min(count - total, length - offset);
      std::memcpy(out + total, buffer + offset, take);
      offset += take;
      logical += take;
      total += take;
    }
    return static_cast<int>(total);
  }
  int available() const { return static_cast<int>(std::min(end - std::min(end, logical), size_t(INT_MAX))); }
  size_t position() const { return logical; }
  bool finish() { return memory || offset == length || (file && file->seek(logical)); }

 private:
  HalFile* file;
  uint8_t* buffer;
  size_t capacity;
  size_t logical;
  size_t end;
  const uint8_t* memory = nullptr;
  size_t offset = 0;
  size_t length = 0;
};

template <typename T>
void readPod(ReadBuffer& file, T& value) {
  file.read(&value, sizeof(value));
}
template <typename T>
bool tryReadPod(ReadBuffer& file, T& value) {
  return file.read(&value, sizeof(value)) == sizeof(value);
}
inline void readString(ReadBuffer& file, std::string& value) {
  uint32_t length = 0;
  readPod(file, length);
  value.resize(length);
  if (length) file.read(value.data(), length);
}
inline bool tryReadString(ReadBuffer& file, std::string& value, size_t limit) {
  uint32_t length = 0;
  if (!tryReadPod(file, length) || length > limit || length > value.max_size() ||
      length > static_cast<uint32_t>(file.available()))
    return false;
  value.resize(length);
  return !length || file.read(value.data(), length) == static_cast<int>(length);
}
}  // namespace serialization
