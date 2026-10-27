#include <Arduino.h>
#include <BoardConfig.h>
#include <Wire.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>

#define private public
#include <InputManager.h>
#undef private
#include "util/HomeButtonInput.h"
#include "util/LoopDeadline.h"

enum class Irqs { Normal, DropFirstPress, DropAll };
struct Result {
  unsigned toggles = 0, singles = 0, services = 0;
};

// Only the bus and scheduling stimulus are synthetic. This executes the
// production GT911 decoder, input deadlines, Home recognizer and Deadline.
// The mock bus reports the latest physical state at the next service.
// Physical controller buffering is outside this model.
Result run(bool home, Irqs irqs, unsigned phase, unsigned irqLatency, unsigned extraPollCap = 1000, bool wrap = false,
           bool failFirstRead = false) {
  BoardConfig::ACTIVE = BoardConfig::XTEINK_X4_PRO;
  std::fill(std::begin(gpioLevels), std::end(gpioLevels), HIGH);
  Wire = {};
  InputManager input;
  input.gt911Addr = 0x14;
  input.touchDataEnabled = true;
  HomeButtonInput gesture;
  const uint32_t epoch = wrap ? 0xfffffc00u : 10000;
  const unsigned press = 70 + (phase % 4) * 10;
  const unsigned gap = 70 + (phase % 5) * 10;
  const unsigned first = 30 + phase * 5;
  const std::array<unsigned, 4> edges{first, first + press, first + press + gap, first + press + gap + press};
  unsigned edge = 0, nextPoll = 0, wake = 100000;
  int lastActivity = -5000;
  Result result;
  for (unsigned ms = 0; ms <= 1800; ++ms) {
    if (edge < edges.size() && ms == edges[edge]) {
      Wire.gtDown = (edge % 2) == 0;
      Wire.gtFresh = true;
      if (edge == 0 && failFirstRead) Wire.failNextGtStatus = true;
      if (irqs == Irqs::Normal || (irqs == Irqs::DropFirstPress && edge != 0)) wake = std::min(wake, ms + irqLatency);
      ++edge;
    }
    if (ms != nextPoll && ms != wake) continue;
    if (ms == wake) wake = 100000;
    fakeNow = uint32_t(epoch + ms);
    input.update();
    ++result.services;
    const bool pressed = input.wasHomeKeyPressed();
    const bool tapped = input.wasHomeKeyTapped();
    const auto action =
        gesture.update(uint32_t(fakeNow), tapped, input.wasHomeKeyLongPressed(), false, pressed, HomeButtonAction::Home,
                       HomeButtonAction::ToggleFrontlight, HomeButtonAction::ReaderMenu);
    if (action == HomeButtonAction::ToggleFrontlight) ++result.toggles;
    if (action == HomeButtonAction::Home) ++result.singles;
    if (pressed || tapped || action != HomeButtonAction::Ignore) lastActivity = ms;
    const unsigned idle = ms - lastActivity;
    const unsigned activityDelay = home ? 1000 : idle >= 3000 ? 50 : 10;
    loopdeadline::Deadline next{uint32_t(fakeNow)};
    next.limit(activityDelay);
    if (home && idle < 300) next.after(uint32_t(epoch + lastActivity), 300);
    next.limit(extraPollCap);
    next.limit(gesture.nextActionDelayMs(uint32_t(fakeNow), next.delayMs()));
    next.limit(input.nextServiceDelayMs(next.delayMs()));
    const unsigned ticks = next.delayMs() == 0 ? 0 : std::max(1u, next.delayMs() / 10);
    nextPoll = ms + std::max(1u, ticks * 10);
  }
  return result;
}

int main() {
  unsigned checks = 0;
  for (bool home : {false, true})
    for (bool failed : {false, true})
      for (bool wrap : {false, true})
        for (unsigned latency : {0u, 5u, 10u, 20u})
          for (unsigned phase = 0; phase < 100; ++phase) {
            const auto r = run(home, Irqs::Normal, phase, latency, 1000, wrap, failed);
            ++checks;
            assert(r.toggles == 1 && r.singles == 0);
          }
  BoardConfig::ACTIVE = BoardConfig::XTEINK_X4_PRO;
  InputManager input;
  input.gt911Addr = 0x14;
  for (unsigned attempt = 1; attempt <= 300; ++attempt) {
    Wire.failNextGtStatus = true;
    input.pollGt911(attempt * 10);
    assert(input.nextServiceDelayMs(1000) == (attempt <= 3 ? 10 : 1000));
    ++checks;
  }
  input.pollGt911(4000);  // A successful idle read restores the retry budget.
  assert(input.nextServiceDelayMs(1000) == 1000);
  Wire.failNextGtStatus = true;
  input.pollGt911(4010);
  assert(input.nextServiceDelayMs(1000) == 10);
  assert(input.nextServiceDelayMs(1) == 1);
  for (unsigned failures = 1; failures <= 3; ++failures) {
    input.gt911ReadFailures = 0;
    for (unsigned n = 0; n < failures; ++n) {
      Wire.failNextGtStatus = true;
      input.pollGt911(5000 + n * 10);
    }
    Wire.gtDown = Wire.gtFresh = true;
    input.pollGt911(5040);
    assert(input.wasHomeKeyPressed());
    Wire.gtDown = false;
    Wire.gtFresh = true;
    input.pollGt911(5050);
    assert(input.wasHomeKeyTapped());
    ++checks;
  }
  std::printf("GT911 status retry + real Home gesture: %u controlled checks passed\n", checks);
}
