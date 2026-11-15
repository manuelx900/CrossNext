#pragma once

#include "PetFeeding.h"

namespace petcurious {
inline constexpr unsigned OFFSET = petfeed::ART_COUNT, ART_COUNT = OFFSET + 3 * 4 * 2 * 2;
enum class Phase : uint8_t { Idle, Look, Blink };
inline bool eligible(const pet::Profile& p, pet::View view) {
  return view == pet::View::Dashboard && pet::canCare(p) && pet::need(p) == pet::Need::None;
}
inline unsigned artIndex(pet::Species species, pet::Stage age, Phase phase) {
  return OFFSET + ((unsigned(species) * 4 + unsigned(age) - 1) * 2 + unsigned(phase == Phase::Blink)) * 2;
}
class Clock {
 public:
  void start() {
    phase_ = Phase::Look;
    awaitingPaint = true;
  }
  void cancel() {
    phase_ = Phase::Idle;
    awaitingPaint = false;
  }
  bool active() const { return phase_ != Phase::Idle; }
  Phase phase() const { return phase_; }
  void painted(uint32_t now) {
    if (!active() || !awaitingPaint) return;
    paintedAt = now;
    awaitingPaint = false;
  }
  bool update(uint32_t now) {
    if (!active() || awaitingPaint || uint32_t(now - paintedAt) < duration()) return false;
    switch (phase_) {
      case Phase::Look:
        phase_ = Phase::Blink;
        break;
      case Phase::Blink:
        phase_ = Phase::Idle;
        break;
      case Phase::Idle:
        return false;
    }
    awaitingPaint = active();
    return true;
  }

 private:
  uint32_t duration() const {
    return phase_ == Phase::Look ? petdisplay::hold(1800, 500) : petdisplay::hold(1000, 300);
  }
  uint32_t paintedAt = 0;
  Phase phase_ = Phase::Idle;
  bool awaitingPaint = false;
};
}  // namespace petcurious
