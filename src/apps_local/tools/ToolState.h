#pragma once
#include <cstddef>
#include <cstdint>
namespace tools {
constexpr size_t kStateBytes = 32;
struct State {
  uint32_t sequence = 0;
  int64_t count = 0;
  bool analog = false;
  bool twelveHour = false;
  uint8_t orientation = 0;
};
void encode(const State&, uint8_t*);
bool decode(const uint8_t*, State&);
bool load(State&, bool& writable);
bool save(State&, bool writable);
}  // namespace tools
