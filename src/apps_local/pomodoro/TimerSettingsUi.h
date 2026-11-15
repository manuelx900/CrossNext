#pragma once

#include <cstddef>

#include "../ui/ToyboxScreen.h"
#include "PomodoroCore.h"

namespace timerui {
// Draw after rebuilding the frame with the existing interaction buffer to make the background inert.
void confirmation(toybox::Screen& screen, const char* question, const char* confirmLabel, freeink::ui::ActionId confirm,
                  freeink::ui::ActionId cancel);
void control(toybox::Screen& screen, freeink::ui::Rect box, const char* label, freeink::ui::ActionId action,
             int value = 0, bool selected = false);
freeink::ui::Rect durations(toybox::Screen& screen, freeink::ui::Rect body, const pomodoro::Config& config,
                            freeink::ui::ActionId change, char* valueText, size_t textSize);
}  // namespace timerui
