#pragma once

#include "PetAnimationSlots.h"
#include "PetDisplayPolicy.h"
#include "PetNeeds.h"

namespace petcompanion {
enum class Kind : uint8_t { None, Sit, Stretch, Greet, Discover, Satisfied, WakeFirst, Hungry, Tired, Dirty, Sick };
enum class Phase : uint8_t { Idle, First, Second };
enum class Emotion : uint8_t { None, Curious, Satisfied, Surprised };
inline bool awake(const pet::Profile& p) { return pet::canCare(p) && !p.resting && p.illness == pet::Illness::Healthy; }
inline bool calm(const pet::Profile& p) { return awake(p) && pet::need(p) == pet::Need::None; }
inline Kind touchKind(const pet::Profile& p, Kind preferred) {
  if (p.resting) return Kind::WakeFirst;
  switch (pet::need(p)) {
    case pet::Need::Sleeping:
      return Kind::WakeFirst;
    case pet::Need::Sick:
      return Kind::Sick;
    case pet::Need::Hungry:
      return Kind::Hungry;
    case pet::Need::Tired:
      return Kind::Tired;
    case pet::Need::Dirty:
      return Kind::Dirty;
    case pet::Need::Company:
      return Kind::Greet;
    case pet::Need::None:
      return preferred;
  }
  return Kind::None;
}
inline unsigned poseSlot(Kind kind, pet::Species species, pet::Stage age) {
  if (age < pet::Stage::Newborn || age > pet::Stage::Adult || unsigned(species) >= 3) return 36;
  unsigned pose;
  switch (kind) {
    case Kind::Sit:
      pose = 0;
      break;
    case Kind::Stretch:
      pose = 1;
      break;
    case Kind::Greet:
      pose = 2;
      break;
    case Kind::None:
    case Kind::Discover:
    case Kind::Satisfied:
    case Kind::WakeFirst:
    case Kind::Hungry:
    case Kind::Tired:
    case Kind::Dirty:
    case Kind::Sick:
      return 36;
  }
  return (pose * 3 + unsigned(species)) * 4 + unsigned(age) - 1;
}
inline unsigned artIndex(unsigned slot, Phase phase) {
  return slot < 36 ? petart::COMPANION_OFFSET + (slot * 2 + unsigned(phase == Phase::Second)) * 2 : petart::ART_COUNT;
}
inline Emotion emotion(Kind kind) {
  switch (kind) {
    case Kind::Sit:
    case Kind::Greet:
      return Emotion::Curious;
    case Kind::Stretch:
    case Kind::Satisfied:
      return Emotion::Satisfied;
    case Kind::Discover:
      return Emotion::Surprised;
    case Kind::None:
    case Kind::WakeFirst:
    case Kind::Hungry:
    case Kind::Tired:
    case Kind::Dirty:
    case Kind::Sick:
      return Emotion::None;
  }
  return Emotion::None;
}
class Clock {
 public:
  void start(Kind value) {
    kind_ = value;
    phase_ = value == Kind::None ? Phase::Idle : Phase::First;
    awaitingPaint = active();
    idleArmed = false;
  }
  void cancel() { start(Kind::None); }
  bool active() const { return phase_ != Phase::Idle; }
  Kind kind() const { return kind_; }
  Phase phase() const { return phase_; }
  void painted(uint32_t now, Kind kind, Phase phase) {
    if (!active() || !awaitingPaint || kind != kind_ || phase != phase_) return;
    paintedAt = now;
    awaitingPaint = false;
  }
  bool update(uint32_t now) {
    const auto duration = kind_ == Kind::Sit ? petdisplay::hold(1800, 700) : petdisplay::hold(1000, 300);
    if (!active() || awaitingPaint || uint32_t(now - paintedAt) < duration) return false;
    if (phase_ == Phase::First) {
      phase_ = Phase::Second;
      awaitingPaint = true;
    } else {
      cancel();
      idleAt = now;
      idleArmed = true;
    }
    return true;
  }
  bool scheduleIdle(uint32_t now, bool eligible) {
    if (!eligible) {
      idleArmed = false;
      return false;
    }
    if (active()) return false;
    if (!idleArmed) {
      idleAt = now;
      idleArmed = true;
      return false;
    }
    if (uint32_t(now - idleAt) < 30000) return false;
    start(Kind::Sit);
    return true;
  }

 private:
  uint32_t paintedAt = 0, idleAt = 0;
  Kind kind_ = Kind::None;
  Phase phase_ = Phase::Idle;
  bool awaitingPaint = false, idleArmed = false;
};
}  // namespace petcompanion
