#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "MurdleCampaignSeeds.h"
#include "MurdleCore.h"

namespace murdlesave {
inline constexpr uint8_t kVersion = 7;
inline constexpr uint16_t kNoCampaign = 65535;
// Access packed integers through the compiler, never a cast of a byte buffer.
struct State {
  uint8_t tier, caseTier, hasCase, solved;
  uint32_t seed, fingerprint, struck, record;
  uint16_t caseNumber, solvedCount, wrongCount;
  uint8_t wrongAccusations, face, page;
  uint8_t marks[murdle::kMaxBlocks * murdle::kMaxItems * murdle::kMaxItems / 4];
  uint16_t campaignCase = kNoCampaign;
  uint8_t completed[murdlecampaign::kProgressBytes]{};
  uint32_t peopleSeed = 0;
} __attribute__((packed));
inline constexpr size_t kBytes = 1 + sizeof(State) + 4;
static_assert(kBytes + sizeof(State) < 240, "Keep save workspace below the task-stack budget");
inline uint32_t checksum(const uint8_t* p, size_t n) {
  uint32_t value = 2166136261u;
  for (size_t i = 0; i < n; ++i) value = (value ^ p[i]) * 16777619u;
  return value;
}
inline void encode(const State& state, uint8_t* bytes) {
  bytes[0] = kVersion;
  std::memcpy(bytes + 1, &state, sizeof(state));
  const auto crc = checksum(bytes, kBytes - 4);
  for (int i = 0; i < 4; ++i) bytes[kBytes - 4 + i] = crc >> (i * 8);
}
inline bool decode(const uint8_t* bytes, size_t n, State& state) {
  if (!n) return false;
  const auto version = bytes[0];
  if (version < 2 || version > kVersion) return false;
  constexpr size_t campaignOffset = offsetof(State, campaignCase);
  constexpr size_t legacyProgressOffset = campaignOffset + 1;
  const size_t legacyProgress = version < 6 ? 13 : 25;
  const size_t payload = version == 2  ? campaignOffset
                         : version < 7 ? legacyProgressOffset + legacyProgress + (version >= 5 ? 4 : 0)
                                       : sizeof(State);
  if (n != 1 + payload + (version >= 4 ? 4 : 0)) return false;
  if (version >= 4) {
    uint32_t crc = 0;
    for (int i = 0; i < 4; ++i) crc |= uint32_t(bytes[n - 4 + i]) << (i * 8);
    if (crc != checksum(bytes, n - 4)) return false;
  }
  state = {};
  if (version == kVersion || version == 2) {
    std::memcpy(&state, bytes + 1, payload);
  } else {
    std::memcpy(&state, bytes + 1, campaignOffset);
    const int oldPerTier = version < 6 ? 25 : 50;
    for (int old = 0; old < oldPerTier * murdle::kTierCount; ++old) {
      const int index = (old / oldPerTier) * murdlecampaign::kCasesPerTier + old % oldPerTier;
      if (bytes[1 + legacyProgressOffset + old / 8] & (1u << (old % 8)))
        state.completed[index / 8] |= 1u << (index % 8);
    }
    const int oldCase = bytes[1 + campaignOffset];
    if (oldCase < oldPerTier * murdle::kTierCount) {
      state.campaignCase = (oldCase / oldPerTier) * murdlecampaign::kCasesPerTier + oldCase % oldPerTier;
      state.caseNumber = state.campaignCase + 1;
    }
    if (version >= 5)
      std::memcpy(&state.peopleSeed, bytes + 1 + legacyProgressOffset + legacyProgress, sizeof(state.peopleSeed));
  }
  if (version < 5 && state.wrongAccusations > 2) state.wrongAccusations = 2;
  if (state.wrongAccusations > 3) return false;
  return true;
}
}  // namespace murdlesave
