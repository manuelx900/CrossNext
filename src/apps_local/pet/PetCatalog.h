#pragma once

#include <cstdint>

namespace pet {
inline constexpr unsigned OBJECT_COUNT = 240, SLOT_COUNT = 4, OBJECTS_PER_SLOT = 60;
// IDs 0..11 retain their PET1/2/3 meanings; each category adds 57 new IDs.
constexpr unsigned objectSlot(unsigned id) { return id < 12 ? id / 3 : (id - 12) / 57; }
constexpr unsigned objectAt(unsigned slot, unsigned index) {
  return index < 3 ? slot * 3 + index : 12 + slot * 57 + index - 3;
}
}  // namespace pet
