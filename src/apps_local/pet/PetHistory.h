#pragma once

#include "PetState.h"

namespace pet {
inline constexpr unsigned HISTORY_CAPACITY = 12, HISTORY_BYTES = 640, LEGACY_HISTORY_BYTES = 512;
struct LifeRecord {
  uint32_t generation = 0;
  uint64_t readingSeconds = 0;
  int64_t endedAt = 0;
  uint16_t days = 0;
  Species species = Species::Silva;
  LifeStatus reason = LifeStatus::Retired;
  char name[NAME_BYTES]{};
};
struct History {
  uint32_t sequence = 0;
  uint8_t count = 0;
  LifeRecord records[HISTORY_CAPACITY]{};
};
bool addLife(History&, const Profile&);
void encodeHistory(const History&, uint8_t out[HISTORY_BYTES]);
bool decodeHistory(const uint8_t*, History&, size_t bytes = HISTORY_BYTES);
class HistoryStore {
 public:
  bool load(History&, bool& writable);
  bool save(History&, bool writable);

 private:
  uint8_t bytes[HISTORY_BYTES]{};
  History candidate;
};
}  // namespace pet
