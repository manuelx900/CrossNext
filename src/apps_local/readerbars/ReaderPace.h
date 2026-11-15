#pragma once

#include <algorithm>
#include <cstdint>

#include "ReaderBarsConfig.h"

namespace readerbars {
// Reader-owned, fixed scalar state. No timers, allocations or storage access.
class Pace {
 public:
  void pause() {
    anchored = false;
    pending = false;
  }
  void reset() { *this = Pace{}; }
  void turn(bool forward, bool succeeded, uint32_t now = 0) {
    pending = forward && succeeded;
    turnMs = now;
    if (!pending) pause();
  }
  void painted(uint32_t now, uint32_t position, bool readingVisible = true) {
    if (!readingVisible) {
      pause();
      return;
    }
    if (position > UNITS) {
      pause();
      return;
    }
    if (pending && anchored && position > anchorPosition) {
      const uint32_t elapsed = (turnMs ? turnMs : now) - anchorMs;
      const uint32_t advance = position - anchorPosition;
      // Rapid turns, long idle gaps and large jumps do not train the estimate.
      if (elapsed >= 2000 && elapsed <= 300000 && advance <= UNITS / 20) {
        const uint64_t sample = uint64_t(elapsed) * UNITS / advance;
        rateMs = samples ? (rateMs * 3 + sample) / 4 : sample;
        if (samples < 255) ++samples;
      }
    }
    if (!anchored || pending || position != anchorPosition) {
      anchorMs = now;
      anchorPosition = position;
      anchored = true;
    }
    pending = false;
  }
  uint32_t remaining(uint32_t position, uint32_t end = UNITS) const {
    if (position >= end) return 0;
    if (samples < 2) return UNKNOWN_TIME;
    const uint64_t seconds = (rateMs * (end - position) / UNITS + 999) / 1000;
    return static_cast<uint32_t>(std::min<uint64_t>(seconds, UINT32_MAX - 1));
  }
  static constexpr uint32_t UNITS = 1000000;

 private:
  uint64_t rateMs = 0;
  uint32_t anchorMs = 0, anchorPosition = 0, turnMs = 0;
  uint8_t samples = 0;
  bool anchored = false, pending = false;
};
}  // namespace readerbars
