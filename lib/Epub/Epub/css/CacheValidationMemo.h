#pragma once

#include <HalMemory.h>
#include <Logging.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <new>
#include <string_view>

// Access is serialized by the CSS cache caller. A miss always takes the full validator.
class CacheValidationMemo {
 public:
  bool contains(std::string_view path, uint64_t size, uint32_t modified) {
    if (!entries || !modified) return false;
    for (auto& entry : *entries)
      if (entry.valid && path == entry.path && entry.size == size && entry.modified == modified) {
        entry.age = ++age;
        return true;
      }
    return false;
  }
  void clear() {
    if (entries)
      for (auto& entry : *entries) entry.valid = false;
  }
  void forget(std::string_view path) {
    if (entries)
      for (auto& entry : *entries)
        if (entry.valid && path == entry.path) entry.valid = false;
  }
  void remember(std::string_view path, uint64_t size, uint32_t modified) {
    if (!modified || path.size() >= PATH_BYTES) return;
    if (!entries) {
      // Optional, bounded PSRAM only; validation still works without the memo.
      memory = HalMemory::allocatePsram(sizeof(Entries));
      if (!memory) {
        LOG_ERR("CSS", "PSRAM validation memo unavailable (%u bytes)", unsigned(sizeof(Entries)));
        return;
      }
      entries = new (memory.get()) Entries{};
    }
    forget(path);
    auto* oldest = &(*entries)[0];
    for (auto& entry : *entries) {
      if (!entry.valid) {
        oldest = &entry;
        break;
      }
      if (entry.age < oldest->age) oldest = &entry;
    }
    std::memcpy(oldest->path, path.data(), path.size());
    oldest->path[path.size()] = 0;
    oldest->size = size;
    oldest->modified = modified;
    oldest->age = ++age;
    oldest->valid = true;
  }

 private:
  static constexpr size_t PATH_BYTES = 512;
  struct Entry {
    char path[PATH_BYTES];
    uint64_t size;
    uint32_t modified, age;
    bool valid;
  };
  using Entries = std::array<Entry, 4>;
  HalMemory::PsramBuffer memory;
  Entries* entries = nullptr;
  uint32_t age = 0;
};
