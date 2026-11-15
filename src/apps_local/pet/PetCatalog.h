#pragma once

#include <cstdint>

namespace pet {
inline constexpr unsigned OBJECT_COUNT = 240, SLOT_COUNT = 4, OBJECTS_PER_SLOT = 60;
// Retired IDs remain reserved in saves, memory snapshots and SD packs.
constexpr bool objectAvailable(unsigned id) {
  switch (id) {
    case 5:
    case 88:
    case 89:
    case 90:
    case 91:
    case 92:
    case 93:
    case 94:
    case 95:
    case 96:
    case 99:
    case 100:
    case 101:
    case 102:
    case 103:
    case 104:
    case 105:
      return false;
    default:
      return id < OBJECT_COUNT;
  }
}
inline constexpr unsigned CATALOG_COUNT = [] {
  unsigned count = 0;
  for (unsigned id = 0; id < OBJECT_COUNT; ++id) count += objectAvailable(id);
  return count;
}();
// IDs 0..11 retain their PET1/2/3 meanings; each category adds 57 new IDs.
constexpr unsigned objectSlot(unsigned id) { return id < 12 ? id / 3 : (id - 12) / 57; }
constexpr unsigned objectAt(unsigned slot, unsigned index) {
  return index < 3 ? slot * 3 + index : 12 + slot * 57 + index - 3;
}
}  // namespace pet
