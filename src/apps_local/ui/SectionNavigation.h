#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>

namespace sectionnav {
// Main-loop-owned commands retain button gestures while the renderer is busy.
// Fixed storage is reused for the activity lifetime; no hot-path allocation.
class Input {
 public:
  enum class Kind : uint8_t { Step, Section, Back, Confirm };
  struct Event {
    Kind kind;
    int16_t amount;
  };
  void begin(bool held) { inheritedHold = held; }
  bool push(Kind kind, int amount = 1) {
    if (count && (kind == Kind::Step || kind == Kind::Section)) {
      auto& last = events[(first + count - 1) % capacity];
      if (last.kind == kind && ((last.amount > 0) == (amount > 0))) {
        last.amount = int16_t(std::clamp(int(last.amount) + amount, -32767, 32767));
        return true;
      }
    }
    if (count == capacity) return false;
    events[(first + count++) % capacity] = {kind, int16_t(amount)};
    return true;
  }
  bool pop(Event& event, int maxSteps = 32767) {
    if (!count) return false;
    event = events[first];
    if (event.kind == Kind::Step && std::abs(int(event.amount)) > maxSteps) {
      event.amount = int16_t(event.amount > 0 ? maxSteps : -maxSteps);
      events[first].amount -= event.amount;
    } else {
      first = (first + 1) % capacity;
      --count;
    }
    return true;
  }
  bool backPending() const { return count && events[first].kind == Kind::Back; }
  bool hasPending() const { return count != 0; }
  void clear() { first = count = 0; }
  template <typename Buttons>
  bool capture(const Buttons& input, uint32_t now) {
    using Button = typename Buttons::Button;
    if (input.wasReleased(Button::Back)) {
      clear();
      return push(Kind::Back);
    }
    if (input.wasReleased(Button::Confirm)) return push(Kind::Confirm);
    const bool nextHeld = input.isPressed(Button::NavNext), prevHeld = input.isPressed(Button::NavPrevious);
    if (inheritedHold) {
      if (!nextHeld && !prevHeld) inheritedHold = false;
      return true;
    }
    const bool nextLong = input.wasLongPressed(Button::NavNext, 350);
    const bool prevLong = input.wasLongPressed(Button::NavPrevious, 350);
    if (nextLong || prevLong ||
        ((nextHeld || prevHeld) && input.getHeldTime() >= 350 && uint32_t(now - repeatedAt) >= 300)) {
      repeatedAt = now;
      return push(Kind::Section, nextLong || (!prevLong && nextHeld) ? 1 : -1);
    }
    if (input.wasReleased(Button::NavNext)) return push(Kind::Step, 1);
    if (input.wasReleased(Button::NavPrevious)) return push(Kind::Step, -1);
    return true;
  }

 private:
  static constexpr unsigned capacity = 32;
  Event events[capacity]{};
  unsigned first = 0, count = 0;
  uint32_t repeatedAt = 0;
  bool inheritedHold = false;
};
}  // namespace sectionnav
