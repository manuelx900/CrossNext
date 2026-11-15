#pragma once

#include "PetAnimationSlots.h"
#include "PetDisplayPolicy.h"
#include "PetNeeds.h"

namespace petrest {
inline constexpr unsigned WALK_OFFSET = petart::ART_COUNT, ART_COUNT = WALK_OFFSET + 2 * 3 * 4 * 2 * 2;
enum class Kind : uint8_t { None, Sleep, Wake };
enum class Phase : uint8_t {
  Idle,
  ApproachFirst,
  ApproachSecond,
  Arrive,
  SettleFirst,
  SettleSecond,
  Exit,
  StretchFirst,
  StretchSecond,
  ReturnFirst,
  ReturnSecond,
  ReturnLast
};
inline bool walking(Phase phase) {
  switch (phase) {
    case Phase::ApproachFirst:
    case Phase::ApproachSecond:
    case Phase::Arrive:
    case Phase::ReturnFirst:
    case Phase::ReturnSecond:
    case Phase::ReturnLast:
      return true;
    case Phase::Idle:
    case Phase::SettleFirst:
    case Phase::SettleSecond:
    case Phase::Exit:
    case Phase::StretchFirst:
    case Phase::StretchSecond:
      return false;
  }
  return false;
}
inline bool returning(Phase phase) {
  return phase == Phase::ReturnFirst || phase == Phase::ReturnSecond || phase == Phase::ReturnLast;
}
inline bool second(Phase phase) {
  return phase == Phase::ApproachSecond || phase == Phase::SettleSecond || phase == Phase::StretchSecond ||
         phase == Phase::ReturnSecond;
}
inline unsigned progress(Phase phase) {
  switch (phase) {
    case Phase::ApproachFirst:
      return 1;
    case Phase::ApproachSecond:
      return 2;
    case Phase::Arrive:
    case Phase::SettleFirst:
    case Phase::SettleSecond:
    case Phase::Exit:
    case Phase::StretchFirst:
    case Phase::StretchSecond:
      return 3;
    case Phase::ReturnFirst:
      return 2;
    case Phase::ReturnSecond:
      return 1;
    case Phase::Idle:
    case Phase::ReturnLast:
      return 0;
  }
  return 0;
}
inline bool eligible(const pet::Profile& p, Kind kind) {
  return pet::canCare(p) && p.illness == pet::Illness::Healthy && kind != Kind::None &&
         p.resting == (kind == Kind::Sleep);
}
inline unsigned walkSlot(pet::Species species, pet::Stage age, bool right) {
  return unsigned(species) < 3 && age >= pet::Stage::Newborn && age <= pet::Stage::Adult
             ? unsigned(right) * 12 + unsigned(species) * 4 + unsigned(age) - 1
             : 24;
}
inline unsigned artIndex(unsigned slot, bool secondFrame) {
  return slot < 24 ? WALK_OFFSET + slot * 4 + unsigned(secondFrame) * 2 : ART_COUNT;
}
class Clock {
 public:
  void start(Kind kind, bool hasBed = true) {
    kind_ = kind;
    hasBed_ = hasBed;
    switch (kind) {
      case Kind::Sleep:
        phase_ = hasBed ? Phase::ApproachFirst : Phase::SettleFirst;
        break;
      case Kind::Wake:
        phase_ = hasBed ? Phase::Exit : Phase::StretchFirst;
        break;
      case Kind::None:
        phase_ = Phase::Idle;
        break;
    }
    awaitingPaint = active();
  }
  void cancel() { start(Kind::None); }
  Kind kind() const { return kind_; }
  Phase phase() const { return phase_; }
  bool active() const { return phase_ != Phase::Idle; }
  void painted(uint32_t now, Phase phase) {
    if (!active() || !awaitingPaint || phase != phase_) return;
    paintedAt = now;
    awaitingPaint = false;
  }
  bool update(uint32_t now) {
    const unsigned duration = walking(phase_) ? petdisplay::hold(650, 200) : petdisplay::hold(900, 300);
    if (!active() || awaitingPaint || uint32_t(now - paintedAt) < duration) return false;
    switch (phase_) {
      case Phase::ApproachFirst:
        phase_ = Phase::ApproachSecond;
        break;
      case Phase::ApproachSecond:
        phase_ = Phase::Arrive;
        break;
      case Phase::Arrive:
        phase_ = Phase::SettleFirst;
        break;
      case Phase::SettleFirst:
        phase_ = Phase::SettleSecond;
        break;
      case Phase::Exit:
        phase_ = Phase::StretchFirst;
        break;
      case Phase::StretchFirst:
        phase_ = Phase::StretchSecond;
        break;
      case Phase::StretchSecond:
        if (hasBed_)
          phase_ = Phase::ReturnFirst;
        else
          cancel();
        break;
      case Phase::ReturnFirst:
        phase_ = Phase::ReturnSecond;
        break;
      case Phase::ReturnSecond:
        phase_ = Phase::ReturnLast;
        break;
      case Phase::SettleSecond:
      case Phase::ReturnLast:
        cancel();
        break;
      case Phase::Idle:
        return false;
    }
    awaitingPaint = active();
    return true;
  }

 private:
  uint32_t paintedAt = 0;
  Kind kind_ = Kind::None;
  Phase phase_ = Phase::Idle;
  bool awaitingPaint = false, hasBed_ = true;
};
}  // namespace petrest
