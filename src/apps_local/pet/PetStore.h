#pragma once

#include "PetState.h"

namespace pet {
class Store {
 public:
  bool load(Profile&, bool& writable);
  bool save(Profile&, bool writable);

 private:
  uint8_t bytes[STATE_BYTES]{};
  Profile candidate;
};
}  // namespace pet
