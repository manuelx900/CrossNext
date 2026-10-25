#pragma once
#include <cstddef>

#include "Game2048Core.h"

namespace game2048 {
constexpr size_t kSaveBytes = 80;
void encode(const State& state, uint64_t best, uint32_t generation, uint8_t* bytes);
bool decode(const uint8_t* bytes, State& state, uint64_t& best, uint32_t& generation);
class Repository {
 public:
  bool load(State& state, uint64_t& best);
  bool save(const State& state, uint64_t best);

 private:
  uint8_t buffer[kSaveBytes]{};
  uint32_t generation = 0;
  int slot = -1;
  bool writable = false;
};
}  // namespace game2048
