#pragma once

#include "PetState.h"

namespace petui {
inline uint64_t readingGains(const pet::Profile& p, bool remedy) {
  const auto earned = remedy ? p.earnedMedicine : p.earnedFood;
  const auto noticed = remedy ? p.noticedMedicine : p.noticedFood;
  return earned > noticed ? earned - noticed : 0;
}
}  // namespace petui
