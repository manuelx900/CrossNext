#pragma once
#include <cstdint>
namespace BoardConfig {
struct Inputs {
  int8_t back = -1, confirm = -1, left = -1, right = -1, up = 0, down = 7, power = 3;
};
struct Touch {
  int8_t irq = 10;
};
struct Profile {
  Inputs input;
  Touch touch;
  int8_t usbDetect = -1, batteryChargeStatus = 21;
};
inline Profile ACTIVE;
}  // namespace BoardConfig
inline uint32_t mockInputLimit = 1000;
class InputManager {
 public:
  uint32_t nextServiceDelayMs(uint32_t maximum) const { return maximum < mockInputLimit ? maximum : mockInputLimit; }
};
