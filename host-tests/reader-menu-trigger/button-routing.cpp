#include <cassert>
#include <cstdio>

#include "src/apps_local/readertext/ReaderMenuTrigger.h"
#include "src/util/ButtonReleaseGate.h"
#include "src/util/SideButtonActions.h"
struct HalGPIO {
  static constexpr uint8_t BTN_UP = 0, BTN_DOWN = 1;
  bool down[2]{}, pressed[2]{}, released[2]{};
  bool hasTouch() const { return true; }
  bool isPressed(uint8_t n) const { return down[n]; }
  bool wasPressed(uint8_t n) const { return pressed[n]; }
  bool wasReleased(uint8_t n) const { return released[n]; }
};
struct Settings {
  uint8_t readerTextTrigger = uint8_t(readertext::MenuTrigger::LeftHold);
  uint8_t sideLeftAction = 0, sideRightAction = 0;
} settings;
#define SETTINGS settings
class MappedInputManager {
 public:
  enum class RemoteButton { None, SideLeft, SideRight };
  struct RemoteInput {
    RemoteButton button = RemoteButton::None;
  } remoteInput;
  HalGPIO gpio;
  bool readerTextContext = false;
  uint8_t suppressedNavigationReleases = 0;
  ButtonReleaseGate releaseGate;
  bool readButton(uint8_t, bool (HalGPIO::*)(uint8_t) const) const;
};
#include "button-routing-production.inc"
int main() {
  MappedInputManager input;
  input.gpio.down[0] = input.gpio.pressed[0] = true;
  assert(input.readButton(0, &HalGPIO::wasPressed));
  input.readerTextContext = true;
  assert(!input.readButton(0, &HalGPIO::wasPressed));
  assert(!input.readButton(0, &HalGPIO::isPressed));
  input.gpio.pressed[1] = true;
  assert(input.readButton(1, &HalGPIO::wasPressed));
  input.gpio.down[0] = input.gpio.pressed[0] = false;
  input.gpio.released[0] = true;
  assert(input.readButton(0, &HalGPIO::wasPressed));
  assert(input.readButton(0, &HalGPIO::wasReleased));
  input.suppressedNavigationReleases = 1;
  assert(!input.readButton(0, &HalGPIO::wasPressed));
  input.suppressedNavigationReleases = 0;
  input.releaseGate.arm(1);
  assert(!input.readButton(0, &HalGPIO::wasPressed));
  assert(!input.readButton(0, &HalGPIO::wasReleased));
  std::puts(
      "Production button routing: deferred short navigation, no page/chapter on menu hold, unselected key, "
      "held-release and activity gate passed");
}
