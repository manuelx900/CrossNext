#pragma once

#include "PetWidgets.h"

namespace petui::actionsurface {
inline constexpr uint8_t RADIUS = widget::CONTROL_RADIUS;
inline void shadow(toybox::Screen& s, fui::Rect box) { widget::shadow(s, box); }
inline void panel(toybox::Screen& s, fui::Rect box) { widget::panel(s, box); }
inline bool button(toybox::Screen& s, Work& w, fui::Rect box, fui::ActionId action, int value = 0, bool primary = false,
                   bool enabled = true) {
  return widget::button(s, w, box, "", action, value, primary, nullptr, enabled);
}
}  // namespace petui::actionsurface
