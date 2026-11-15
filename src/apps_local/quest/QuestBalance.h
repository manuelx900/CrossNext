#pragma once
#include <cstdint>

namespace questbalance {
inline constexpr uint32_t MAX_LEVEL = 150;
inline constexpr uint32_t XP_PER_MINUTE = 3;
inline constexpr uint32_t XP_CAP = 30, COIN_CAP = 25;
constexpr uint32_t levelPower(uint32_t level) { return 10 + 2 * (level > 0 ? level - 1 : 0); }
constexpr uint32_t recommendedLevel(unsigned path) { return 1 + (path > 119 ? 119 : path) * 99 / 119; }
constexpr uint32_t referencePower(unsigned path) { return 30 + 4 * recommendedLevel(path); }
constexpr uint32_t campaignMinutes(unsigned path) {
  const bool final = path == 119;
  if (final) --path;
  const unsigned region = path / 10, local = path % 10;
  const unsigned minutes = ((6 + region * 11 + local * 3) * 105 + 99) / 100;
  return (final ? 2 : 1) * (local == 9 ? (minutes * 3 + 1) / 2 : minutes);
}
constexpr uint32_t duration(uint32_t minutes, uint32_t reference, uint32_t power, uint32_t floorPercent) {
  const uint64_t scaled = (uint64_t(minutes) * (100 + reference) + 99 + power) / (100 + power);
  const uint32_t floor = (minutes * floorPercent + 99) / 100;
  return uint32_t(scaled < floor ? floor : scaled);
}
constexpr uint32_t campaignDuration(unsigned path, uint32_t power) {
  const unsigned factor = path == 119 ? 2 : 1;
  return duration(campaignMinutes(path) / factor, referencePower(path - (factor - 1)), power, 40) * factor;
}
constexpr uint32_t victoryXp(uint32_t minutes, unsigned bonus = 0) {
  return uint32_t((uint64_t(minutes) * XP_PER_MINUTE * (100 + bonus) + 50) / 100);
}
constexpr uint32_t campaignXp(unsigned path) { return victoryXp(campaignMinutes(path)); }
constexpr uint32_t campaignCoins(unsigned path) { return 12 + path / 10 * 4 + (path % 10 == 9 ? 20 : 0); }
}  // namespace questbalance
