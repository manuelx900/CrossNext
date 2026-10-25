#pragma once

#include <FreeInkUICore.h>

#include "MappedInputManager.h"
#include "activities/Activity.h"

namespace sidebuttons {
inline int shortStep(const MappedInputManager& input) {
  using Button = MappedInputManager::Button;
  return input.wasReleased(Button::NavNext) ? 1 : input.wasReleased(Button::NavPrevious) ? -1 : 0;
}
inline int longStep(const MappedInputManager& input) {
  using Button = MappedInputManager::Button;
  if (input.wasLongPressed(Button::NavNext, 500)) return 1;
  if (input.wasLongPressed(Button::NavPrevious, 500)) return -1;
  return 0;
}
inline int swipeStep(const MappedInputManager& input) {
  switch (input.wasSwipe()) {
    case MappedInputManager::SwipeDir::Left:
    case MappedInputManager::SwipeDir::Up:
      return 1;
    case MappedInputManager::SwipeDir::Right:
    case MappedInputManager::SwipeDir::Down:
      return -1;
    case MappedInputManager::SwipeDir::None:
      return 0;
  }
  return 0;
}
inline int horizontalStep(const MappedInputManager& input) {
  const auto swipe = input.wasSwipe();
  return swipe == MappedInputManager::SwipeDir::Left ? 1 : swipe == MappedInputManager::SwipeDir::Right ? -1 : 0;
}
template <typename Buffer>
freeink::ui::ActionEvent pageSwipe(Buffer& buffer, const MappedInputManager& input) {
  const int direction = horizontalStep(input);
  if (!direction || !buffer.routable()) return {};
  const uint16_t mask = direction > 0 ? freeink::ui::InputNext : freeink::ui::InputPrev;
  for (size_t n = 0; n < buffer.count(); ++n) {
    if (!(buffer.data()[n].inputMask & mask)) continue;
    freeink::ui::InputSnapshot snapshot{};
    snapshot.next = direction > 0;
    snapshot.prev = direction < 0;
    return buffer.route(snapshot);
  }
  return {};
}
inline bool pending(const MappedInputManager& input) {
  using Button = MappedInputManager::Button;
  return input.wasReleased(Button::NavNext) || input.wasReleased(Button::NavPrevious) ||
         ((input.isPressed(Button::NavNext) || input.isPressed(Button::NavPrevious)) && input.getHeldTime() >= 500) ||
         input.wasReleased(Button::Confirm);
}

template <typename Buffer>
bool poll(Buffer& buffer, const MappedInputManager& input, int& x, int& y, bool enabled = true) {
  if (input.wasScreenTapped(x, y)) {
    buffer.setFocusedIndex(-1);
    if (input.wasPressed(MappedInputManager::Button::NavNext))
      input.suppressNextRelease(MappedInputManager::Button::NavNext);
    if (input.wasPressed(MappedInputManager::Button::NavPrevious))
      input.suppressNextRelease(MappedInputManager::Button::NavPrevious);
    return true;
  }
  x = y = -1;
  if (!enabled || !pending(input) || !buffer.routable()) return false;
  freeink::ui::Rect rect;
  if (input.wasReleased(MappedInputManager::Button::Confirm) && buffer.buttonFocusRect(rect)) {
    x = rect.x + rect.width / 2;
    y = rect.y + rect.height / 2;
  }
  return true;
}

// poll() admits navigation and activation. Geometry handlers require the
// coordinates supplied only by a touch or a confirmed coordinate target.
inline bool hasActivation(int x, int y) { return x >= 0 && y >= 0; }

inline void append(const MappedInputManager& input, freeink::ui::InputSnapshot& snapshot) {
  using Button = MappedInputManager::Button;
  if ((snapshot.touchPressed || snapshot.touchReleased) && snapshot.touchX >= 0 && snapshot.touchY >= 0) {
    if (input.wasPressed(Button::NavNext)) input.suppressNextRelease(Button::NavNext);
    if (input.wasPressed(Button::NavPrevious)) input.suppressNextRelease(Button::NavPrevious);
    return;
  }
  if (pending(input)) snapshot.touchReleased = false;
  snapshot.focusNext = input.wasReleased(Button::NavNext);
  snapshot.focusPrev = input.wasReleased(Button::NavPrevious);
  snapshot.next = false;
  snapshot.prev = false;
  snapshot.confirm = input.wasReleased(Button::Confirm);
}

template <typename Buffer>
freeink::ui::ActionEvent route(Buffer& buffer, freeink::ui::InputSnapshot snapshot, const MappedInputManager& input,
                               Activity& owner, bool enabled = true) {
  int touchX = 0, touchY = 0;
  const bool touch = input.wasScreenTapped(touchX, touchY);
  if (enabled && !touch && input.wasReleased(MappedInputManager::Button::Confirm) && buffer.hasButtonFocus()) {
    snapshot.touchReleased = false;
    snapshot.confirm = true;
  }
  if (enabled) append(input, snapshot);
  const auto before = buffer.focusPosition();
  const auto event = buffer.route(snapshot);
  if (before != buffer.focusPosition()) owner.requestUpdate();
  return event;
}
}  // namespace sidebuttons
