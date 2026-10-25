#pragma once

#include <algorithm>

#include "PetScreens.h"

namespace petui {
enum class GameOffer : uint8_t { Blocked, Reward, Practice };
inline GameOffer gameOffer(const Model& m) {
  if (pet::playBlock(*m.profile) != pet::PlayBlock::None) return GameOffer::Blocked;
  return m.writable && pet::gameRewardAvailable(*m.profile, m.today) ? GameOffer::Reward : GameOffer::Practice;
}
inline const char* gameCareBlock(const pet::Profile& p) {
  switch (pet::playBlock(p)) {
    case pet::PlayBlock::None:
      return "";
    case pet::PlayBlock::Unavailable:
      return p.lifeStatus != pet::LifeStatus::Active ? tr(STR_PET_LIFE_ENDED) : tr(STR_PET_CARE_AFTER_HATCH);
    case pet::PlayBlock::Sick:
      return tr(STR_PET_NEEDS_REMEDY);
    case pet::PlayBlock::Sleeping:
      return tr(STR_PET_GAME_WAKE_SHORT);
    case pet::PlayBlock::Hungry:
      return tr(STR_PET_GAME_FOOD_SHORT);
    case pet::PlayBlock::Tired:
      return tr(STR_PET_GAME_REST_SHORT);
  }
  return "";
}
inline const char* healthActionHint(const Model& m, bool healing) {
  const auto& p = *m.profile;
  if (!pet::canCare(p))
    return p.lifeStatus != pet::LifeStatus::Active ? tr(STR_PET_LIFE_ENDED) : tr(STR_PET_CARE_AFTER_HATCH);
  if (healing && p.illness == pet::Illness::Healthy) return tr(STR_PET_NO_TREATMENT);
  if (!healing && p.hygiene == 100) return tr(STR_PET_ALREADY_CLEAN);
  if (!m.writable) return tr(STR_PET_CARE_NO_SAVE);
  if (healing) return pet::medicine(p) ? tr(STR_PET_CURE_ONE) : tr(STR_PET_NO_MEDICINE);
  return tr(STR_PET_CLEAN_FREE);
}
inline const char* healthAdvice(const Model& m) {
  const auto& p = *m.profile;
  if (p.lifeStatus != pet::LifeStatus::Active) return tr(STR_PET_LIFE_ENDED);
  if (pet::stage(p.readingSeconds) == pet::Stage::Egg) return tr(STR_PET_EGG_HEALTH);
  if (!m.writable) return tr(STR_PET_CARE_NO_SAVE);
  if (p.illness != pet::Illness::Healthy)
    return pet::medicine(p) ? tr(STR_PET_HEALTH_ADVICE_CURE) : tr(STR_PET_HEALTH_ADVICE_READ);
  if (p.hygiene <= 20) return tr(STR_PET_HEALTH_ADVICE_DIRTY);
  return p.hygiene < 100 ? tr(STR_PET_HEALTH_ADVICE_CLEAN) : tr(STR_PET_HEALTH_ADVICE_OK);
}
inline int64_t restRemaining(const Model& m) {
  const auto& p = *m.profile;
  if (!p.resting || !m.clockKnown || m.now < 1577836800 || p.restAt <= 0 || m.now < p.restAt) return -1;
  return std::max<int64_t>(0, 7200 - (m.now - p.restAt));
}
inline fui::ActionId statusAction(const pet::Profile& p) {
  if (!pet::canCare(p)) return fui::NO_ACTION;
  switch (pet::need(p)) {
    case pet::Need::None:
      return fui::NO_ACTION;
    case pet::Need::Sick:
    case pet::Need::Dirty:
      return OpenHealth;
    case pet::Need::Sleeping:
    case pet::Need::Hungry:
    case pet::Need::Tired:
    case pet::Need::Company:
      return ChangeTab;
  }
  return fui::NO_ACTION;
}
inline const char* statusLink(const pet::Profile& p) {
  return statusAction(p) == OpenHealth ? tr(STR_PET_GO_HEALTH) : tr(STR_PET_GO_CARE);
}
inline const char* statusHint(const pet::Profile& p) {
  switch (pet::need(p)) {
    case pet::Need::Sick:
      return tr(STR_PET_NEEDS_REMEDY);
    case pet::Need::Dirty:
      return tr(STR_PET_NEEDS_CLEANING);
    case pet::Need::None:
    case pet::Need::Sleeping:
    case pet::Need::Hungry:
    case pet::Need::Tired:
    case pet::Need::Company:
      return nullptr;
  }
  return nullptr;
}
inline const char* careAdvice(const pet::Profile& p) {
  if (!pet::canCare(p)) return tr(STR_PET_LIFE_ENDED);
  switch (pet::need(p)) {
    case pet::Need::None:
      return tr(STR_PET_CARE_ADVICE_OK);
    case pet::Need::Sick:
      return tr(STR_PET_CARE_ADVICE_SICK);
    case pet::Need::Sleeping:
      return tr(STR_PET_CARE_ADVICE_SLEEP);
    case pet::Need::Hungry:
      return pet::food(p) ? tr(STR_PET_CARE_ADVICE_HUNGRY) : tr(STR_PET_CARE_ADVICE_FOOD);
    case pet::Need::Tired:
      return tr(STR_PET_CARE_ADVICE_TIRED);
    case pet::Need::Dirty:
      return tr(STR_PET_CARE_ADVICE_DIRTY);
    case pet::Need::Company:
      return tr(STR_PET_CARE_ADVICE_COMPANY);
  }
  return "";
}
inline fui::ActionId advisedCare(const pet::Profile& p, int day) {
  if (!pet::canCare(p)) return fui::NO_ACTION;
  switch (pet::need(p)) {
    case pet::Need::None:
    case pet::Need::Sleeping:
      return fui::NO_ACTION;
    case pet::Need::Sick:
    case pet::Need::Dirty:
      return OpenHealth;
    case pet::Need::Hungry:
      return pet::food(p) ? Feed : fui::NO_ACTION;
    case pet::Need::Tired:
      return Rest;
    case pet::Need::Company:
      return day > 0 && day <= p.lastPetDay && pet::gameRewardAvailable(p, day) ? OpenGames : Stroke;
  }
  return fui::NO_ACTION;
}
}  // namespace petui
