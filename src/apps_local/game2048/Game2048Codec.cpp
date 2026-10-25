#include <algorithm>
#include <cstring>

#include "Game2048Store.h"
namespace game2048 {
namespace {
void put(uint8_t* p, uint64_t v, int count) {
  for (int n = 0; n < count; ++n) p[n] = static_cast<uint8_t>(v >> (n * 8));
}
uint64_t get(const uint8_t* p, int count) {
  uint64_t v = 0;
  for (int n = 0; n < count; ++n) v |= uint64_t(p[n]) << (n * 8);
  return v;
}
uint32_t hash(const uint8_t* bytes) {
  uint32_t v = 2166136261u;
  for (size_t n = 0; n < kSaveBytes - 4; ++n) v = (v ^ bytes[n]) * 16777619u;
  return v;
}
}  // namespace
void encode(const State& state, uint64_t best, uint32_t generation, uint8_t* bytes) {
  std::memset(bytes, 0, kSaveBytes);
  std::memcpy(bytes, "2041", 4);
  put(bytes + 4, generation, 4);
  std::memcpy(bytes + 8, state.cells, 16);
  put(bytes + 24, state.score, 8);
  put(bytes + 32, best, 8);
  put(bytes + 40, state.moves, 4);
  put(bytes + 44, state.random, 4);
  put(bytes + kSaveBytes - 4, hash(bytes), 4);
}
bool decode(const uint8_t* bytes, State& state, uint64_t& best, uint32_t& generation) {
  if (std::memcmp(bytes, "2041", 4) || get(bytes + kSaveBytes - 4, 4) != hash(bytes)) return false;
  for (int n = 0; n < 16; ++n)
    if (bytes[8 + n] > 31) return false;
  if (!get(bytes + 44, 4)) return false;
  std::memcpy(state.cells, bytes + 8, 16);
  state.score = get(bytes + 24, 8);
  best = std::max(state.score, get(bytes + 32, 8));
  state.moves = get(bytes + 40, 4);
  state.random = get(bytes + 44, 4);
  generation = get(bytes + 4, 4);
  return true;
}
}  // namespace game2048
