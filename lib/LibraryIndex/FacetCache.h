#pragma once

#include <HalMemory.h>

#include <array>
#include <cstdint>

namespace library {
class FacetCache {
 public:
  static constexpr size_t LIMIT = 128 * 1024;
  enum class Result { Hit, Empty, Unavailable };

  Result activate(int kind, size_t bytes) {
    if (kind < 0 || kind >= int(entries.size()) || bytes > LIMIT) return Result::Unavailable;
    active = kind;
    auto& entry = entries[kind];
    entry.age = ++age;
    if (entry.valid && entry.bytes == bytes) return Result::Hit;
    discardActive();
    while (used + bytes > LIMIT && evict()) {
    }
    if (bytes) {
      // At most 128 KiB of ordinals across five facets, activity-owned, PSRAM only.
      entry.data = HalMemory::allocatePsram(bytes);
      if (!entry.data) {
        while (evict()) {
        }
        entry.data = HalMemory::allocatePsram(bytes);
      }
      if (!entry.data) return Result::Unavailable;
    }
    entry.bytes = bytes;
    used += bytes;
    return Result::Empty;
  }
  uint8_t* get() const { return active < 0 ? nullptr : entries[active].data.get(); }
  explicit operator bool() const { return get() != nullptr; }
  uint16_t count() const { return active < 0 ? 0 : entries[active].count; }
  void commit(uint16_t count) {
    if (active < 0) return;
    entries[active].count = count;
    entries[active].valid = true;
  }
  void deactivate() { active = -1; }
  void discardActive() {
    if (active < 0) return;
    auto& entry = entries[active];
    used -= entry.bytes;
    entry = {};
    entry.age = ++age;
  }
  void clear() {
    for (auto& entry : entries) entry = {};
    used = 0;
    active = -1;
  }

 private:
  struct Entry {
    HalMemory::PsramBuffer data;
    size_t bytes = 0;
    uint32_t age = 0;
    uint16_t count = 0;
    bool valid = false;
  };
  std::array<Entry, 5> entries{};
  size_t used = 0;
  uint32_t age = 0;
  int active = -1;
  bool evict() {
    int oldest = -1;
    for (int n = 0; n < int(entries.size()); ++n)
      if (n != active && entries[n].bytes && (oldest < 0 || entries[n].age < entries[oldest].age)) oldest = n;
    if (oldest < 0) return false;
    used -= entries[oldest].bytes;
    entries[oldest] = {};
    return true;
  }
};
}  // namespace library
