#pragma once

#include "PetScreens.h"

namespace petui {
// Needs retain their existing advice and actions ahead of companion flavour.
inline const char* companionLine(const Model& m) {
  const auto& p = *m.profile;
  if (!p.created || p.lifeStatus != pet::LifeStatus::Active) return nullptr;
  const auto age = pet::stage(p.readingSeconds);
  if (age == pet::Stage::Egg) return tr(STR_PET_COMPANION_EGG);
  if (pet::need(p) != pet::Need::None) return nullptr;
  if (m.clockKnown && m.today > 0 && !m.historyChanged) {
    if (m.todaySeconds >= int(pet::GOALS[p.goalIndex]) * 60) return tr(STR_PET_COMPANION_GOAL);
    if (m.todaySeconds > 0) return tr(STR_PET_COMPANION_READING);
    if (p.lastPetDay == m.today) return tr(STR_PET_COMPANION_PETTED);
  }
  switch (age) {
    case pet::Stage::Newborn:
      return tr(STR_PET_COMPANION_NEWBORN);
    case pet::Stage::Baby:
      return tr(STR_PET_COMPANION_BABY);
    case pet::Stage::Juvenile:
      return tr(STR_PET_COMPANION_JUVENILE);
    case pet::Stage::Adult:
      return tr(STR_PET_COMPANION_ADULT);
    case pet::Stage::Egg:
      return nullptr;
  }
  return nullptr;
}
}  // namespace petui
