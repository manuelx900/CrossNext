#pragma once

#include <cstdint>

namespace pomodoro {
// Main-loop-owned release retained while a timer repaint holds RenderLock.
class PendingTap {
 public:
  void capture(int x, int y, uint32_t shownView) {
    if (pending) return;
    tapX = int16_t(x);
    tapY = int16_t(y);
    view = shownView;
    pending = true;
  }
  bool take(uint32_t currentView, int& x, int& y) {
    if (!pending) return false;
    pending = false;
    if (view != currentView) return false;
    x = tapX;
    y = tapY;
    return true;
  }
  void clear() { pending = false; }
  bool hasPending() const { return pending; }

 private:
  uint32_t view = 0;
  int16_t tapX = 0, tapY = 0;
  bool pending = false;
};
}  // namespace pomodoro
