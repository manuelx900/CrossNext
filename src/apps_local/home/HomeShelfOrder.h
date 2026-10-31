#pragma once
#include <array>
#include <cstdint>
namespace homeui {
enum class ShelfSource : uint8_t { Recent, Index };
struct ShelfEntry {
  uint64_t hash = 0;
  uint16_t index = 0;
  ShelfSource source = ShelfSource::Index;
};
struct ShelfOrder {
  static constexpr int capacity = 60;
  std::array<ShelfEntry, capacity> entries{};
  int count = 0;
  bool add(ShelfSource source, uint16_t index, uint64_t hash) {
    if (count == capacity) return false;
    for (int n = 0; n < count; ++n)
      if (entries[n].hash == hash) return true;
    entries[count++] = {hash, index, source};
    return true;
  }
};
}  // namespace homeui
