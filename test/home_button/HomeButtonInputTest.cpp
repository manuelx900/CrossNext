#include <gtest/gtest.h>

#include <cstdint>

#include "util/HomeButtonInput.h"

TEST(HomeButtonInput, RecognizesConfiguredGestures) {
  using A = HomeButtonAction;
  HomeButtonInput input;
  auto tick = [&](uint32_t now, bool tap = false, bool hold = false, bool swipe = false, bool press = false) {
    return input.update(now, tap, hold, swipe, press, A::Home, A::ToggleFrontlight, A::ReaderMenu);
  };
  EXPECT_EQ(tick(0, true), A::Ignore);
  EXPECT_EQ(tick(350), A::Ignore);
  EXPECT_EQ(tick(351), A::Home);
  EXPECT_EQ(tick(352), A::Ignore);
  EXPECT_EQ(tick(1000, true), A::Ignore);
  EXPECT_EQ(tick(1350, true), A::ToggleFrontlight);
  EXPECT_EQ(tick(1701), A::Ignore);
  EXPECT_EQ(tick(2000, false, true), A::ReaderMenu);
  EXPECT_EQ(tick(2001), A::Ignore);
  // Tap then hold: the second contact prevents an early single-tap action.
  EXPECT_EQ(tick(3000, true), A::Ignore);
  EXPECT_EQ(tick(3200, false, false, false, true), A::Ignore);
  EXPECT_EQ(tick(3400), A::Ignore);
  EXPECT_EQ(tick(3900, false, true), A::ReaderMenu);
  EXPECT_EQ(tick(4000), A::Ignore);
  // A bezel swipe cancels the Home-key event and any deferred action.
  EXPECT_EQ(tick(5000, true), A::Ignore);
  EXPECT_EQ(tick(5100, true, false, true), A::Ignore);
  EXPECT_EQ(tick(5500), A::Ignore);
  EXPECT_EQ(tick(6000, true), A::Ignore);
  input.reset();
  EXPECT_EQ(tick(6500), A::Ignore);
  // Unsigned subtraction remains valid over the millisecond clock wrap.
  EXPECT_EQ(tick(UINT32_MAX - 100, true), A::Ignore);
  EXPECT_EQ(tick(100, true), A::ToggleFrontlight);
  EXPECT_EQ(tick(UINT32_MAX - 100, true), A::Ignore);
  EXPECT_EQ(tick(251), A::Home);
  // Two separated taps each produce a single action.
  EXPECT_EQ(tick(7000, true), A::Ignore);
  EXPECT_EQ(tick(7400, true), A::Home);
  EXPECT_EQ(tick(7751), A::Home);
  // Disabling double tap removes the single-tap delay.
  EXPECT_EQ(input.update(8000, true, false, false, false, A::Bookmark, A::Ignore, A::ReaderMenu), A::Bookmark);
  // Every Home gesture can launch clipping selection without changing persisted action IDs.
  static_assert(static_cast<uint8_t>(A::ToggleFrontlight) == 10);
  EXPECT_EQ(input.update(9000, true, false, false, false, A::CreateClipping, A::Ignore, A::Home), A::CreateClipping);
  EXPECT_EQ(input.update(10000, true, false, false, false, A::Home, A::CreateClipping, A::Ignore), A::Ignore);
  EXPECT_EQ(input.update(10100, true, false, false, false, A::Home, A::CreateClipping, A::Ignore), A::CreateClipping);
  EXPECT_EQ(input.update(11000, false, true, false, false, A::Home, A::Ignore, A::CreateClipping), A::CreateClipping);
}

TEST(HomeButtonInput, PendingTapContributesItsDeadlineAcrossClockWrap) {
  using A = HomeButtonAction;
  for (uint32_t start : {0u, 1000u, UINT32_MAX - 100u}) {
    HomeButtonInput input;
    auto tick = [&](uint32_t now, bool tapped = false, bool held = false, bool swiped = false, bool pressed = false) {
      return input.update(now, tapped, held, swiped, pressed, A::Home, A::ToggleFrontlight, A::ReaderMenu);
    };
    EXPECT_EQ(input.nextActionDelayMs(start, 1000), 1000u);
    EXPECT_EQ(tick(start, true), A::Ignore);
    EXPECT_EQ(input.nextActionDelayMs(start, 1000), 351u);
    EXPECT_EQ(input.nextActionDelayMs(start, 10), 10u);
    EXPECT_EQ(input.nextActionDelayMs(start, 0), 0u);
    EXPECT_EQ(input.nextActionDelayMs(start + 350, 1000), 1u);
    EXPECT_EQ(tick(start + 350), A::Ignore);
    EXPECT_EQ(input.nextActionDelayMs(start + 351, 1000), 0u);
    EXPECT_EQ(tick(start + 351), A::Home);
    EXPECT_EQ(input.nextActionDelayMs(start + 351, 1000), 1000u);
    EXPECT_EQ(tick(start + 352), A::Ignore);

    EXPECT_EQ(tick(start + 1000, true), A::Ignore);
    EXPECT_EQ(tick(start + 1100, false, false, false, true), A::Ignore);
    EXPECT_EQ(input.nextActionDelayMs(start + 1400, 1000), 1000u);
    EXPECT_EQ(tick(start + 1400), A::Ignore);
    EXPECT_EQ(tick(start + 1700, false, true), A::ReaderMenu);
    EXPECT_EQ(input.nextActionDelayMs(start + 1700, 1000), 1000u);

    EXPECT_EQ(tick(start + 2000, true), A::Ignore);
    EXPECT_EQ(tick(start + 2200, true), A::ToggleFrontlight);
    EXPECT_EQ(input.nextActionDelayMs(start + 2400, 1000), 1000u);
    EXPECT_EQ(tick(start + 3000, true), A::Ignore);
    EXPECT_EQ(tick(start + 3100, false, false, true), A::Ignore);
    EXPECT_EQ(input.nextActionDelayMs(start + 3400, 1000), 1000u);
    EXPECT_EQ(input.update(start + 4000, true, false, false, false, A::Home, A::Ignore, A::ReaderMenu), A::Home);
    EXPECT_EQ(input.nextActionDelayMs(start + 4000, 1000), 1000u);
  }
}

TEST(HomeButtonInput, ExpiredTapFollowedByAnotherTapGetsANewDeadline) {
  using A = HomeButtonAction;
  HomeButtonInput input;
  EXPECT_EQ(input.update(0, true, false, false, false, A::Home, A::ToggleFrontlight, A::ReaderMenu), A::Ignore);
  EXPECT_EQ(input.update(400, true, false, false, false, A::Home, A::ToggleFrontlight, A::ReaderMenu), A::Home);
  EXPECT_EQ(input.nextActionDelayMs(400, 1000), 351u);
  input.reset();
  EXPECT_EQ(input.nextActionDelayMs(800, 1000), 1000u);
}
