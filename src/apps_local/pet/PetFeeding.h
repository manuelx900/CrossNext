#pragma once

#include <cstdint>

#include "PetAnimationSlots.h"
#include "PetDisplayPolicy.h"
#include "PetSleepAnimation.h"

namespace petfeed {
inline constexpr unsigned FOOD_COUNT = pet::FOOD_CHOICE_COUNT, LEGACY_FOOD_COUNT = 6, FOODS_PER_PAGE = 3,
                          BITE_COUNT = 3;
inline constexpr unsigned FOOD_OFFSET = petsleep::ART_COUNT;
inline constexpr unsigned PET_OFFSET = FOOD_OFFSET + 3 * 2 * 2;
inline constexpr unsigned SIMPLE_ART_COUNT = PET_OFFSET + 3 * 4 * 2 * 2;
inline constexpr unsigned ART_COUNT = SIMPLE_ART_COUNT + LEGACY_FOOD_COUNT * (BITE_COUNT + 1) * 2;
enum class Art : uint8_t { Legacy, Simple, Progressive, Extended };
inline unsigned choices(Art art) { return art == Art::Extended ? FOOD_COUNT : LEGACY_FOOD_COUNT; }
enum class Phase : uint8_t { Idle, Bite, Chew, Result };
inline constexpr bool eating(Phase phase) { return phase == Phase::Bite || phase == Phase::Chew; }
inline constexpr unsigned foodIndex(unsigned food, bool bitten = false) {
  return FOOD_OFFSET + (food * 2 + unsigned(bitten)) * 2;
}
inline constexpr unsigned progressiveFoodIndex(unsigned food, unsigned eaten) {
  return food < LEGACY_FOOD_COUNT
             ? SIMPLE_ART_COUNT + (food * (BITE_COUNT + 1) + eaten) * 2
             : petart::EXTRA_FOOD_OFFSET + ((food - LEGACY_FOOD_COUNT) * (BITE_COUNT + 1) + eaten) * 2;
}
inline constexpr unsigned petIndex(unsigned species, unsigned age, Phase phase) {
  return PET_OFFSET + ((species * 4 + age - 1) * 2 + unsigned(phase == Phase::Chew)) * 2;
}
class Clock {
 public:
  void start() {
    phase_ = Phase::Bite;
    bites = 0;
    awaitingPaint = true;
  }
  void cancel() {
    phase_ = Phase::Idle;
    awaitingPaint = false;
  }
  Phase phase() const { return phase_; }
  bool active() const { return phase_ != Phase::Idle; }
  unsigned foodStage() const { return bites + unsigned(phase_ == Phase::Chew); }
  void painted(uint32_t now) {
    if (!active() || !awaitingPaint) return;
    paintedAt = now;
    awaitingPaint = false;
  }
  bool update(uint32_t now) {
    if (!active() || awaitingPaint || uint32_t(now - paintedAt) < duration()) return false;
    switch (phase_) {
      case Phase::Bite:
        phase_ = Phase::Chew;
        break;
      case Phase::Chew:
        if (++bites < BITE_COUNT)
          phase_ = Phase::Bite;
        else
          phase_ = Phase::Result;
        break;
      case Phase::Result:
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
    return phase_ == Phase::Result ? petdisplay::hold(2400, 900) : petdisplay::hold(1200, 300);
  }
  uint32_t paintedAt = 0;
  uint8_t bites = 0;
  Phase phase_ = Phase::Idle;
  bool awaitingPaint = false;
};
}  // namespace petfeed
