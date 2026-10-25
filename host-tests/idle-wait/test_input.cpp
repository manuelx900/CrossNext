#include <BoardConfig.h>
#include <Wire.h>

#include <algorithm>
#include <cassert>
#include <cstdio>
#define private public
#include <InputManager.h>
#undef private

int main() {
  BoardConfig::ACTIVE = BoardConfig::XTEINK_X4_PRO;
  InputManager input;
  assert(input.nextServiceDelayMs(1000) == 1000);
  assert(input.nextServiceDelayMs(0) == 0);
  input.lastState = 1;
  assert(input.nextServiceDelayMs(1000) == 6);
  assert(input.nextServiceDelayMs(2) == 2);
  input.currentState = input.lastState;
  assert(input.nextServiceDelayMs(1000) == 10);
  input.currentState = input.lastState = 0;
  for (bool* held : {&input.confirmBackPhysicalPressed, &input.confirmPowerPhysicalPressed, &input.touchPressed,
                     &input.touchHomeKeyDown}) {
    *held = true;
    assert(input.nextServiceDelayMs(1000) == 10);
    assert(input.nextServiceDelayMs(1) == 1);
    *held = false;
  }
  input.twoButtonPhysicalState = 1;
  assert(input.nextServiceDelayMs(1000) == 10);
  input.twoButtonPhysicalState = 0;
  input.touchSnapshot.reportedCount = 2;
  assert(input.nextServiceDelayMs(1000) == 10);
  input.touchSnapshot.reportedCount = 0;
  BoardConfig::ACTIVE = BoardConfig::STICKY;
  assert(input.nextServiceDelayMs(1000) == 1000);
  BoardConfig::ACTIVE = BoardConfig::METALIO_EINK4;
  assert(input.nextServiceDelayMs(1000) == 10);
  std::puts("PASS real InputManager debounce, holds, multi-touch, Home and controller fallback");
}
