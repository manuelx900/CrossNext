#pragma once

#include "PetState.h"

namespace petui {
enum class ReadingDisplay : uint8_t { Project, Lifetime };
inline bool hasReadingProject(const pet::Profile& p) {
  return p.created && p.lifeStatus == pet::LifeStatus::Active && pet::homeUnlocked(p) &&
         pet::objectAvailable(p.project) && !pet::owns(p, p.project);
}
}  // namespace petui
