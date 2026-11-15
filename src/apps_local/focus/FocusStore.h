#pragma once
#include "FocusCore.h"
namespace focus {
class Repository {
 public:
  bool load(State& state, State& scratch);
  bool save(const State& state);
  bool writable() const { return !readOnly; }

 private:
  uint8_t buffer[kFileBytes]{};
  int slot = -1;
  uint32_t generation = 0, savedHash = 0;
  size_t loadedBytes = 0;
  bool readOnly = false;
  bool read(int index);
};
}  // namespace focus
