#pragma once

#include "HabitsCore.h"

namespace habits {

enum class LoadResult : uint8_t { Loaded, Empty, Recovered, Unavailable, Damaged };

// Two alternating, checksummed files preserve the previous commit if power
// fails during a write. The fixed buffer belongs to the activity's lifetime.
class Repository {
 public:
  LoadResult load(Store& store, Store& scratch);
  bool save(const Store& store);

 private:
  bool readSlot(int slot);
  uint8_t buffer[kFileSize]{};
  uint32_t generation = 0;
  int activeSlot = -1;
};

}  // namespace habits
