#pragma once

#include "PetState.h"

namespace pet {
enum class Need : uint8_t { None, Sick, Sleeping, Hungry, Tired, Dirty, Company };
inline Need need(const Profile& p) {
  if (p.illness != Illness::Healthy) return Need::Sick;
  if (p.resting) return Need::Sleeping;
  if (p.saciety < 30) return Need::Hungry;
  if (p.energy < 20) return Need::Tired;
  if (p.hygiene <= 20) return Need::Dirty;
  if (p.joy < 60) return Need::Company;
  return Need::None;
}
inline unsigned moodGain(const Profile& p) { return p.joy < 40 ? 15 : 10; }
inline unsigned gameMoodGain(const Profile& p, Game game) {
  static constexpr unsigned base[] = {10, 15, 20};
  return base[unsigned(game)] + (p.joy < 40 ? 5 : 0);
}
enum class PlayBlock : uint8_t { None, Unavailable, Sick, Sleeping, Hungry, Tired };
inline PlayBlock playBlock(const Profile& p) {
  if (!canCare(p)) return PlayBlock::Unavailable;
  if (p.illness != Illness::Healthy) return PlayBlock::Sick;
  if (p.resting) return PlayBlock::Sleeping;
  if (p.saciety < 30) return PlayBlock::Hungry;
  if (p.energy < 20) return PlayBlock::Tired;
  return PlayBlock::None;
}
inline bool gameRewardAvailable(const Profile& p, int day) {
  return playBlock(p) == PlayBlock::None && day > 0 && (day > p.gameDay || (day == p.gameDay && p.gamesToday < 3));
}
bool rewardGame(Profile&, int day, Game game = Game::Pairs);
struct Affection {
  uint8_t before = 0, after = 0;
  Need reaction = Need::None;
  bool first = false;
};
inline Affection affection(Profile& p, int day) {
  const auto before = p.joy;
  const auto reaction = need(p);
  const bool first = petOnce(p, day);
  return {before, p.joy, reaction, first};
}
}  // namespace pet
