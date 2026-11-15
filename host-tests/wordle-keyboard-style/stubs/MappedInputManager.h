#pragma once
#include <cstdint>
inline uint32_t millis() {
  static uint32_t n = 0;
  return ++n;
}
class MappedInputManager {
 public:
  enum class Button { Back, NavNext, NavPrevious, Confirm };
  bool tap = false, confirm = false, held = false, ended = false, back = false, backGesture = false;
  int x = 0, y = 0, liveX = -1, liveY = -1;
  bool wasReleased(Button b) const { return (b == Button::Confirm && confirm) || (b == Button::Back && back); }
  bool wasScreenTapped(int& a, int& b) const {
    a = x;
    b = y;
    return tap;
  }
  bool wasBackGesture() const { return backGesture; }
  bool wasScreenTouchDown(int&, int&) const { return false; }
  bool isScreenTouchHeld(int& a, int& b) const {
    a = liveX >= 0 ? liveX : x;
    b = liveY >= 0 ? liveY : y;
    return held;
  }
  bool wasScreenTouchReleased() const { return ended; }
};
