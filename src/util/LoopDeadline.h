#pragma once

#include <algorithm>
#include <cstdint>

namespace loopdeadline {
constexpr uint32_t LEGACY_POLL_MS = 10;
constexpr uint32_t LEGACY_IDLE_POLL_MS = 50;
constexpr uint32_t LEGACY_IDLE_MS = 3000;
constexpr uint32_t HEARTBEAT_MS = 1000;

class Deadline {
 public:
  explicit Deadline(uint32_t now, uint32_t maximum = HEARTBEAT_MS) : now(now), delay(maximum) {}
  void limit(uint32_t maximum) { delay = std::min(delay, maximum); }
  void after(uint32_t startedAt, uint32_t duration) {
    const uint32_t elapsed = now - startedAt;
    limit(elapsed >= duration ? 0 : duration - elapsed);
  }
  uint32_t delayMs() const { return delay; }

 private:
  uint32_t now;
  uint32_t delay;
};
}  // namespace loopdeadline
