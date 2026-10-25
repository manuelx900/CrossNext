#pragma once

#include "ChessPuzzleCore.h"

namespace chesspuzzle {
class Store {
 public:
  bool load(Progress& progress);
  bool save(const Progress& progress);
  bool reset(Progress& progress);

 private:
  uint8_t buffer[kProgressBytes]{};
  Progress saved, scratch;
  uint32_t generation = 0;
  int slot = -1;
  bool readOnly = false;
  bool read(int index);
};
}  // namespace chesspuzzle
