#include "ToolState.h"

#include <cstring>
namespace tools {
namespace {
uint32_t hash(const uint8_t* p) {
  uint32_t h = 2166136261u;
  for (int n = 0; n < 28; ++n) h = (h ^ p[n]) * 16777619u;
  return h;
}
void put(uint8_t* p, uint64_t v, int size) {
  for (int n = 0; n < size; ++n) p[n] = v >> (8 * n);
}
uint64_t get(const uint8_t* p, int size) {
  uint64_t v = 0;
  for (int n = 0; n < size; ++n) v |= uint64_t(p[n]) << (8 * n);
  return v;
}
}  // namespace
void encode(const State& s, uint8_t* p) {
  std::memset(p, 0, kStateBytes);
  std::memcpy(p, "TLS1", 4);
  put(p + 4, s.sequence, 4);
  put(p + 8, s.count, 8);
  p[16] = s.analog;
  p[17] = s.orientation;
  p[18] = s.twelveHour;
  put(p + 28, hash(p), 4);
}
bool decode(const uint8_t* p, State& s) {
  if (std::memcmp(p, "TLS1", 4) || get(p + 28, 4) != hash(p) || p[16] > 1 || p[17] > 3 || p[18] > 1 ||
      get(p + 8, 8) > 999999999)
    return false;
  s.sequence = get(p + 4, 4);
  s.count = get(p + 8, 8);
  s.analog = p[16];
  s.orientation = p[17];
  s.twelveHour = p[18];
  return true;
}
}  // namespace tools
