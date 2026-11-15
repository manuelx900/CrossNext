#pragma once

#include "PetCuriosity.h"
#include "PetReactionData.h"

namespace petreact {
enum class Kind : uint8_t { None, Petting, Cleaning, Healing, Game, Growth };
enum class Art : uint8_t { None, Legacy, Registered };
inline constexpr unsigned POSE_COUNT = 4 * 3 * 4, LEGACY_OFFSET = petcurious::ART_COUNT;
inline constexpr unsigned LEGACY_ART_COUNT = LEGACY_OFFSET + POSE_COUNT * 2 * 2;
inline constexpr unsigned OFFSET = LEGACY_ART_COUNT, ART_COUNT = OFFSET + POSE_COUNT * 2 * 2;
static_assert(sizeof(REGISTRATIONS) / sizeof(REGISTRATIONS[0]) == POSE_COUNT);
enum class Phase : uint8_t {
  Idle,
  First,
  Second,
  Before,
  BeforeShadow,
  After,
  AfterShadow,
  BeforeAgain,
  AfterAgain,
  Reveal,
  Result
};
inline unsigned poseSlot(Kind kind, pet::Species species, pet::Stage age) {
  unsigned action = 0;
  switch (kind) {
    case Kind::Petting:
      action = 0;
      break;
    case Kind::Cleaning:
      action = 1;
      break;
    case Kind::Healing:
      action = 2;
      break;
    case Kind::Game:
      action = 3;
      break;
    case Kind::None:
    case Kind::Growth:
      return POSE_COUNT;
  }
  if (unsigned(species) >= 3 || age < pet::Stage::Newborn || age > pet::Stage::Adult) return POSE_COUNT;
  return (action * 3 + unsigned(species)) * 4 + unsigned(age) - 1;
}
inline unsigned artIndex(Kind kind, pet::Species species, pet::Stage age, Phase phase, Art art = Art::Registered) {
  const auto slot = poseSlot(kind, species, age);
  if (slot >= POSE_COUNT) return ART_COUNT;
  unsigned offset = 0;
  switch (art) {
    case Art::None:
      return ART_COUNT;
    case Art::Legacy:
      offset = LEGACY_OFFSET;
      break;
    case Art::Registered:
      offset = OFFSET;
      break;
  }
  return offset + (slot * 2 + unsigned(phase == Phase::Second)) * 2;
}
inline bool previous(Phase phase) {
  return phase == Phase::Idle || phase == Phase::Before || phase == Phase::BeforeShadow || phase == Phase::BeforeAgain;
}
inline bool silhouette(Phase phase) {
  switch (phase) {
    case Phase::Before:
    case Phase::BeforeShadow:
    case Phase::After:
    case Phase::AfterShadow:
    case Phase::BeforeAgain:
    case Phase::AfterAgain:
      return true;
    case Phase::Idle:
    case Phase::First:
    case Phase::Second:
    case Phase::Reveal:
    case Phase::Result:
      return false;
  }
  return false;
}
inline pet::Stage growthStage(const pet::Profile& p, Phase phase) {
  const auto current = pet::stage(p.readingSeconds);
  return previous(phase) && p.noticedStage < current ? p.noticedStage : current;
}
class Clock {
 public:
  void start(Kind kind) {
    if (kind == Kind::None) {
      cancel();
      return;
    }
    kind_ = kind;
    phase_ = kind == Kind::Growth ? Phase::Before : Phase::First;
    awaitingPaint = true;
  }
  void cancel() {
    kind_ = Kind::None;
    phase_ = Phase::Idle;
    awaitingPaint = false;
  }
  Kind kind() const { return kind_; }
  Phase phase() const { return phase_; }
  bool active() const { return phase_ != Phase::Idle; }
  void painted(uint32_t now, Kind kind, Phase phase) {
    if (!active() || !awaitingPaint || kind != kind_ || phase != phase_) return;
    paintedAt = now;
    awaitingPaint = false;
  }
  bool update(uint32_t now) {
    if (!active() || awaitingPaint || uint32_t(now - paintedAt) < duration()) return false;
    switch (phase_) {
      case Phase::Idle:
        return false;
      case Phase::First:
        phase_ = Phase::Second;
        break;
      case Phase::Second:
        phase_ = Phase::Result;
        break;
      case Phase::Before:
        phase_ = Phase::BeforeShadow;
        break;
      case Phase::BeforeShadow:
        phase_ = Phase::After;
        break;
      case Phase::After:
        phase_ = Phase::AfterShadow;
        break;
      case Phase::AfterShadow:
        phase_ = Phase::BeforeAgain;
        break;
      case Phase::BeforeAgain:
        phase_ = Phase::AfterAgain;
        break;
      case Phase::AfterAgain:
        phase_ = Phase::Reveal;
        break;
      case Phase::Reveal:
        phase_ = Phase::Result;
        break;
      case Phase::Result:
        cancel();
        return true;
    }
    awaitingPaint = true;
    return true;
  }

 private:
  uint32_t duration() const {
    switch (phase_) {
      case Phase::Idle:
        return 0;
      case Phase::First:
        return petdisplay::hold(1200, 300);
      case Phase::Second:
        return petdisplay::hold(1200, 300);
      case Phase::Before:
      case Phase::After:
        return petdisplay::hold(1000, 400);
      case Phase::BeforeShadow:
      case Phase::AfterShadow:
      case Phase::BeforeAgain:
      case Phase::AfterAgain:
        return petdisplay::hold(800, 200);
      case Phase::Reveal:
        return petdisplay::hold(1600, 600);
      case Phase::Result:
        return petdisplay::hold(2400, 900);
    }
    return 0;
  }
  uint32_t paintedAt = 0;
  Kind kind_ = Kind::None;
  Phase phase_ = Phase::Idle;
  bool awaitingPaint = false;
};
}  // namespace petreact
