#pragma once
#include <ArduinoJson.h>
#include <BufferedFile.h>

#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "ReadingStatsCore.h"
#ifdef ARDUINO
#include <Arduino.h>
#endif
namespace readingstats::cloud {
inline int jsonDay(const char* s) {
  if (!s || std::strlen(s) != 10 || s[4] != '-' || s[7] != '-') return 0;
  for (int n = 0; n < 10; ++n)
    if (n != 4 && n != 7 && (s[n] < '0' || s[n] > '9')) return 0;
  const int y = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 + s[3] - '0';
  if (y < 2000 || y > 2099) return 0;
  return dayIndex({y, (s[5] - '0') * 10 + s[6] - '0', (s[8] - '0') * 10 + s[9] - '0'});
}

// ArduinoJson owns and frees these C allocator blocks. A budget prevents a
// server response from growing beyond 2 MiB; default-capability RAM may use S3 PSRAM.
class JsonBudget final : public ArduinoJson::Allocator {
  struct alignas(std::max_align_t) Block {
    size_t bytes;
  };
  size_t used = 0;
  static constexpr size_t maximum = 2 * 1024 * 1024;

 public:
  void* allocate(size_t size) override {
    if (size > maximum - sizeof(Block) || size + sizeof(Block) > maximum - used) return nullptr;
    const auto total = size + sizeof(Block);
    auto* block = static_cast<Block*>(std::malloc(total));
    if (!block) return nullptr;
    block->bytes = total;
    used += total;
    return block + 1;
  }
  void deallocate(void* pointer) override {
    if (!pointer) return;
    auto* block = static_cast<Block*>(pointer) - 1;
    used -= block->bytes;
    std::free(block);
  }
  void* reallocate(void* pointer, size_t size) override {
    if (!pointer) return allocate(size);
    if (!size) {
      deallocate(pointer);
      return nullptr;
    }
    auto* old = static_cast<Block*>(pointer) - 1;
    if (size > maximum - sizeof(Block) || size + sizeof(Block) > maximum - (used - old->bytes)) return nullptr;
    const auto previous = old->bytes, total = size + sizeof(Block);
    auto* block = static_cast<Block*>(std::realloc(old, total));
    if (!block) return nullptr;
    block->bytes = total;
    used = used - previous + total;
    return block + 1;
  }
};
struct JsonReader {
  serialization::BufferedFileReader file;
  explicit JsonReader(HalFile& input) : file(input, 2048) {}
  int read() {
#ifdef ARDUINO
    if ((file.position() & 8191) == 0) delay(1);
#endif
    uint8_t byte;
    return file.read(&byte, 1) == 1 ? byte : -1;
  }
  size_t readBytes(char* out, size_t size) { return file.read(out, size); }
};
}  // namespace readingstats::cloud
