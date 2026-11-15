#include <cassert>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <utility>

#include "src/apps_local/readertext/ReaderMenuTrigger.h"

using namespace readertext;
int main() {
  assert(menuTrigger(255) == MenuTrigger::SwipeUp);
  for (auto trigger : {MenuTrigger::SwipeUp, MenuTrigger::CenterTap, MenuTrigger::HomeHold}) {
    MenuButtonInput input;
    assert(!input.update(true, trigger, 3, 0));
    assert(!input.update(true, trigger, 3, 1000));
  }
  for (auto trigger : {MenuTrigger::LeftHold, MenuTrigger::RightHold, MenuTrigger::EitherHold}) {
    MenuButtonInput input;
    const uint8_t expected = trigger == MenuTrigger::LeftHold ? 1 : trigger == MenuTrigger::RightHold ? 2 : 3;
    assert(!input.update(true, trigger, 3, 100));
    assert(!input.update(true, trigger, 3, 799));
    assert(input.update(true, trigger, 3, 800) == expected);
    assert(!input.update(true, trigger, 3, 2000));
    assert(!input.update(true, trigger, 0, 2001));
    assert(!input.update(true, trigger, 3, UINT32_MAX - 500));
    assert(input.update(true, trigger, 3, 199) == expected);
  }
  static_assert(uint8_t(MenuTrigger::EitherHold) == 5 && uint8_t(MenuTrigger::BottomTap) == 6 &&
                uint8_t(MenuTrigger::TopTap) == 7);
  for (const auto size : {std::pair{480, 800}, std::pair{800, 480}}) {
    const auto [width, height] = size;
    for (int x : {0, width / 2, width - 1}) {
      assert(menuTap(MenuTrigger::TopTap, x, 0, width, height));
      assert(menuTap(MenuTrigger::TopTap, x, 47, width, height));
      assert(!menuTap(MenuTrigger::TopTap, x, 48, width, height));
      assert(menuTap(MenuTrigger::BottomTap, x, height - 1, width, height));
      assert(menuTap(MenuTrigger::BottomTap, x, height - 48, width, height));
      assert(!menuTap(MenuTrigger::BottomTap, x, height - 49, width, height));
      assert(!menuTap(MenuTrigger::SwipeUp, x, 0, width, height));
    }
    for (auto trigger : {MenuTrigger::TopTap, MenuTrigger::BottomTap}) {
      assert(!menuTap(trigger, -1, 0, width, height));
      assert(!menuTap(trigger, width, height - 1, width, height));
      assert(!menuTap(trigger, width / 2, -1, width, height));
      assert(!menuTap(trigger, width / 2, height, width, height));
      assert(!menuTap(trigger, 0, 0, 0, height));
    }
  }
  MenuButtonInput input;
  assert(!input.update(false, MenuTrigger::EitherHold, 1, 0));
  assert(!input.update(true, MenuTrigger::EitherHold, 1, 9999));
  input.update(true, MenuTrigger::EitherHold, 0, 10000);
  input.update(true, MenuTrigger::EitherHold, 1, 10001);
  input.cancel();
  assert(!input.update(true, MenuTrigger::EitherHold, 1, 20000));
  input.update(true, MenuTrigger::EitherHold, 0, 20001);
  input.update(true, MenuTrigger::EitherHold, 1, 20002);
  input.update(true, MenuTrigger::EitherHold, 3, 20400);
  assert(input.update(true, MenuTrigger::EitherHold, 3, 20702) == 1);
  assert(input.update(true, MenuTrigger::EitherHold, 3, 21100) == 2);
  input.update(true, MenuTrigger::EitherHold, 0, 22000);
  input.update(true, MenuTrigger::LeftHold, 1, 23000);
  input.update(true, MenuTrigger::SwipeUp, 1, 23100);
  assert(!input.update(true, MenuTrigger::LeftHold, 1, 25000));
  for (const auto size : {std::pair{480, 800}, std::pair{800, 480}}) {
    assert(centerMenuTap(MenuTrigger::CenterTap, size.first / 2, size.second / 2, size.first, size.second));
    assert(!centerMenuTap(MenuTrigger::SwipeUp, size.first / 2, size.second / 2, size.first, size.second));
    assert(!centerMenuTap(MenuTrigger::CenterTap, 0, 0, size.first, size.second));
    assert(!centerMenuTap(MenuTrigger::CenterTap, size.first * 2 / 3, size.second / 2, size.first, size.second));
  }
  std::puts(
      "Reader panel: default, all side holds, short presses, once per press, ownership, independent clocks, wrap and "
      "center bounds passed");
}
