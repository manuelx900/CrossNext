#include "InterceptCore.h"

#include <cstring>

#include "../reading/ReadingStatsCore.h"
namespace intercept {
namespace {
void put(uint8_t* p, uint32_t v, int n) {
  for (int k = 0; k < n; ++k) p[k] = v >> (8 * k);
}
uint32_t get(const uint8_t* p, int n) {
  uint32_t v = 0;
  for (int k = 0; k < n; ++k) v |= uint32_t(p[k]) << (8 * k);
  return v;
}
}  // namespace
bool allowed(const State& s, unsigned choice) {
  if (s.node >= nodeCount || choice >= nodes[s.node].count) return false;
  const auto& c = nodes[s.node].choices[choice];
  return (s.flags & c.require) == c.require && !(s.flags & c.forbid) && c.next < nodeCount;
}
bool choose(State& s, unsigned index) {
  if (!allowed(s, index)) return false;
  const auto& c = nodes[s.node].choices[index];
  s.flags = (s.flags | c.set) & ~c.clear;
  s.node = c.next;
  if (s.choices < UINT32_MAX) ++s.choices;
  return true;
}
void encode(const State& s, uint8_t* out) {
  std::memset(out, 0, 32);
  std::memcpy(out, "INT1", 4);
  put(out + 4, s.sequence, 4);
  put(out + 8, s.node, 2);
  put(out + 10, s.flags, 2);
  put(out + 12, s.choices, 4);
  put(out + 28, readingstats::checksum(out, 28), 4);
}
bool decode(const uint8_t* b, State& s) {
  if (std::memcmp(b, "INT1", 4) || get(b + 28, 4) != readingstats::checksum(b, 28) || get(b + 8, 2) >= nodeCount ||
      (get(b + 10, 2) & ~63u))
    return false;
  s = {get(b + 4, 4), uint16_t(get(b + 8, 2)), uint16_t(get(b + 10, 2)), get(b + 12, 4)};
  return true;
}
}  // namespace intercept
