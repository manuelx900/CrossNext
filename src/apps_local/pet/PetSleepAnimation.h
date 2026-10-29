#pragma once

#include "PetNeeds.h"
#include "PetState.h"

namespace petsleep {
inline constexpr uint32_t FRAME_MS = 750;
inline constexpr uint32_t MOOD_FRAME_MS = 3000;
inline uint32_t frameInterval(const pet::Profile& p) {
  return pet::stage(p.readingSeconds) == pet::Stage::Egg ? FRAME_MS : MOOD_FRAME_MS;
}
inline constexpr unsigned POSES = 12, FRAMES = 3, HEAD_OFFSET = POSES * FRAMES * 2,
                          BODY_OFFSET = HEAD_OFFSET + POSES * 2;
enum class Pose : uint8_t { Normal, Happy, Hungry, Tired, Sick };
inline constexpr unsigned BODY_POSES = 5, EGG_OFFSET = BODY_OFFSET + POSES * BODY_POSES * FRAMES * 2;
inline constexpr unsigned ART_COUNT = EGG_OFFSET + 3 * FRAMES * 2;
inline constexpr char ART_PATH[] = "/apps/pet/breathing.bin";

inline bool enabled(const pet::Profile& p, pet::View view) {
  return (view == pet::View::Home || view == pet::View::Dashboard || view == pet::View::Reward) && p.created &&
         p.lifeStatus == pet::LifeStatus::Active && p.resting && p.illness == pet::Illness::Healthy &&
         pet::stage(p.readingSeconds) != pet::Stage::Egg;
}
inline bool breathing(const pet::Profile& p, pet::View view) {
  if (!p.created || p.lifeStatus != pet::LifeStatus::Active) return false;
  switch (view) {
    case pet::View::Dashboard:
    case pet::View::Home:
    case pet::View::Reward:
    case pet::View::Evolution:
      return true;
    default:
      // Menus, historical pictures and reading do not need periodic display updates.
      return false;
  }
}
inline bool moodVisible(const pet::Profile& p, pet::View view) {
  return breathing(p, view) && view != pet::View::Evolution && pet::stage(p.readingSeconds) != pet::Stage::Egg &&
         (!p.resting || p.illness != pet::Illness::Healthy);
}
inline unsigned frame(unsigned step) {
  static constexpr uint8_t frames[] = {0, 1, 2, 1};
  return frames[step % 4];
}
inline unsigned artIndex(const pet::Profile& p, unsigned step) {
  const unsigned age = unsigned(pet::stage(p.readingSeconds));
  return ((unsigned(p.species) * 4 + (age ? age - 1 : 0)) * FRAMES + frame(step)) * 2;
}
inline unsigned bodyIndex(pet::Species species, pet::Stage age, Pose pose, unsigned step) {
  return BODY_OFFSET + ((unsigned(species) * 4 + (unsigned(age) - 1)) * BODY_POSES + unsigned(pose)) * FRAMES * 2 +
         frame(step) * 2;
}
inline unsigned graphicIndex(unsigned id, unsigned step) {
  const unsigned offset = id - 3;
  return bodyIndex(pet::Species(offset / 16), pet::Stage(offset % 16 / 4 + 1), Pose(offset % 4), step);
}
inline unsigned eggIndex(unsigned species, unsigned step) { return EGG_OFFSET + (species * FRAMES + frame(step)) * 2; }
inline Pose bodyPose(const pet::Profile& p) {
  switch (pet::need(p)) {
    case pet::Need::Sick:
      return Pose::Sick;
    case pet::Need::Sleeping:
    case pet::Need::Tired:
      return Pose::Tired;
    case pet::Need::Hungry:
      return Pose::Hungry;
    case pet::Need::Dirty:
    case pet::Need::Company:
      return Pose::Normal;
    case pet::Need::None:
      return p.joy >= 75 ? Pose::Happy : Pose::Normal;
  }
  return Pose::Normal;
}
inline unsigned fullBodyIndex(const pet::Profile& p, unsigned step) {
  if (pet::stage(p.readingSeconds) == pet::Stage::Egg) return eggIndex(unsigned(p.species), step);
  if (p.illness == pet::Illness::Healthy && (p.resting || p.lifeStatus == pet::LifeStatus::Retired))
    return artIndex(p, step);
  return bodyIndex(p.species, pet::stage(p.readingSeconds), bodyPose(p), step);
}
inline unsigned headWidth(pet::Species species) {
  static constexpr uint8_t widths[] = {56, 60, 58};
  return widths[unsigned(species)];
}
inline unsigned headIndex(const pet::Profile& p) {
  const unsigned age = unsigned(pet::stage(p.readingSeconds));
  return HEAD_OFFSET + (unsigned(p.species) * 4 + (age ? age - 1 : 0)) * 2;
}
inline constexpr unsigned HEAD_HEIGHT = 78;

class Clock {
 public:
  void restart(uint32_t now) {
    at = now;
    current = 0;
    running = true;
  }
  bool update(uint32_t now, bool enabled, uint32_t frameMs = FRAME_MS) {
    if (!enabled) {
      running = false;
      current = 0;
      return false;
    }
    if (!running) {
      running = true;
      at = now;
      current = 0;
      return true;
    }
    const uint32_t elapsed = now - at;
    if (elapsed < frameMs) return false;
    const uint32_t frames = elapsed / frameMs;
    at += frames * frameMs;
    const uint8_t next = uint8_t((current + frames % 4) % 4);
    if (next == current) return false;
    current = next;
    return true;
  }
  unsigned step() const { return current; }

 private:
  uint32_t at = 0;
  uint8_t current = 0;
  bool running = false;
};
}  // namespace petsleep
