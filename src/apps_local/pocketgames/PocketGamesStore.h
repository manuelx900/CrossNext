#pragma once
#include "PocketGamesCore.h"

namespace pocketgames {
class Store {
 public:
  bool load(Kind kind, State& state);
  bool save(const State& state);
  bool hasSave() const { return slot >= 0; }

 private:
  uint8_t buffer[kSaveBytes]{};
  State scratch;
  uint32_t generation = 0, savedHash = 0;
  int slot = -1;
  bool readOnly = false;
  bool read(int slot);
  uint32_t payloadHash(const State& state);
};
}  // namespace pocketgames
