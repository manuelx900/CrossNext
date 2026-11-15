#pragma once

#include <HalStorage.h>

#include "PomodoroHistory.h"
#include "PomodoroStore.h"

namespace pomodoro {
inline constexpr const char* kHistoryPath = "/.crosspoint/pomodoro-history.bin";
enum class Scan { Record, Finished, Error };
// Timer checkpoint and study deltas share one checked record, preventing
// duplicate or lost historical credit between two independent saves.
class HistoryStore {
 public:
  bool load(State& state, DisplayOptions* options = nullptr);
  bool save(const State& state, Pending& pending, const DisplayOptions& options = {});
  bool beginScan();
  Scan next(Pending& delta);
  void close();
  bool damaged() const { return hadDamage; }

 private:
  Store legacy;
  HalFile scan;
  uint8_t buffer[kHistoryBytes]{};
  // One sector-friendly block reused throughout an incremental history scan.
  uint8_t readAhead[24 * kHistoryBytes]{};
  State scratch;
  Pending decoded;
  DisplayOptions decodedOptions;
  uint64_t end = 16, cursor = 16, scanEnd = 16;
  uint64_t bufferedAt = 0;
  size_t bufferedBytes = 0;
  uint32_t sequence = 0, savedHash = 0;
  bool readOnly = false, hadDamage = false, hasRecord = false;
  bool headerReady = false;
  bool read(HalFile& file, uint64_t offset, uint32_t expected, bool protect = true);
  uint32_t hash(const State& state, const DisplayOptions& options);
};
}  // namespace pomodoro
