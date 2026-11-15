#pragma once

#include "PetState.h"

namespace petfarewell {
inline constexpr uint32_t FRAME_MS = 750;
inline bool floating(const pet::Profile& p, pet::View view) {
  return p.created && p.lifeStatus == pet::LifeStatus::Lost &&
         (view == pet::View::Dashboard || view == pet::View::Death);
}
inline int offset(unsigned step) {
  static constexpr int8_t offsets[] = {0, -6, 0, 6};
  return offsets[step % 4];
}
}  // namespace petfarewell
