#pragma once

#include <cstdint>
#include <string_view>

// Serialized Section lifecycle only. Hash collisions affect eviction order,
// never profile validation. Eight chapter histories occupy 256 static bytes.
namespace layoutcache {
struct History {
  uint64_t key = 0;
  uint32_t ages[4] = {};
  uint32_t recent = 0;
};
inline History histories[8];
inline uint32_t clock = 0;

inline History& history(std::string_view path) {
  uint64_t key = 14695981039346656037ULL;
  for (const unsigned char byte : path) key = (key ^ byte) * 1099511628211ULL;
  if (!key) key = 1;
  History* oldest = &histories[0];
  for (auto& entry : histories) {
    if (entry.key == key) return entry;
    if (entry.recent < oldest->recent) oldest = &entry;
  }
  *oldest = {};
  oldest->key = key;
  return *oldest;
}
inline void touch(std::string_view path, uint8_t slot) {
  if (++clock == 0) {
    for (auto& entry : histories) entry = {};
    clock = 1;
  }
  auto& entry = history(path);
  entry.ages[slot] = entry.recent = clock;
}
inline uint8_t victim(std::string_view path) {
  auto& entry = history(path);
  uint8_t oldest = 1;
  for (uint8_t slot = 2; slot < 4; ++slot)
    if (entry.ages[slot] < entry.ages[oldest]) oldest = slot;
  return oldest;
}
inline void move(std::string_view path, uint8_t from, uint8_t to) {
  auto& entry = history(path);
  entry.ages[to] = entry.ages[from];
  entry.ages[from] = 0;
}
}  // namespace layoutcache
