#include <HalGPIO.h>
#include <driver/gpio.h>

#include <cassert>
#include <cstdio>

static void otherOwner(void*) {}
static void reset() {
  BoardConfig::ACTIVE = {};
  mockHandlers = {};
  mockHandlers[48] = {otherOwner, nullptr};
  mockCreateFails = false;
  mockInstallResult = ESP_OK;
  mockFailure = MockFailure::None;
  mockFailedPin = -1;
  mockAdds = mockRemoves = mockInstalls = mockTakes = mockErrors = mockDelay = mockYield = mockIsrYields = 0;
  mockInputLimit = 1000;
}
int main() {
  for (int installed : {ESP_OK, ESP_ERR_INVALID_STATE}) {
    reset();
    mockInstallResult = installed;
    HalGPIO input;
    input.attachActivityInterrupts();
    assert(input.activityInterruptsReady && mockAdds == 5 && mockInstalls == 1);
    assert(input.nextInputServiceDelayMs(1000) == 1000);
    mockHandlers[0].fn(mockHandlers[0].context);  // Edge before the wait must remain pending.
    input.waitForActivity(1000);
    assert(mockTakeSignaled && mockTicks == 100 && mockIsrYields == 1);
    input.signalActivity();
    input.waitForActivity(6);
    assert(mockTakeSignaled && mockTicks == 1);  // Never truncate a positive wait to zero ticks.
    input.waitForActivity(0);
    assert(mockTicks == 0 && mockYield == 1);
    mockInputLimit = 6;
    assert(input.nextInputServiceDelayMs(1000) == 6);
    assert(mockHandlers[48].fn == otherOwner);
  }
  reset();
  BoardConfig::ACTIVE.input.confirm = 3;
  {
    HalGPIO input;
    input.attachActivityInterrupts();
    assert(input.activityInterruptsReady && mockAdds == 5);
  }
  for (bool semaphoreFails : {false, true}) {
    reset();
    mockCreateFails = semaphoreFails;
    mockInstallResult = ESP_ERR_NO_MEM;
    HalGPIO input;
    input.attachActivityInterrupts();
    assert(!input.activityInterruptsReady && mockErrors == 1);
    assert(input.nextInputServiceDelayMs(1000) == 10);
    input.waitForActivity(1000);
    assert(mockTakes == 0 && mockDelay == 10 && mockHandlers[48].fn == otherOwner);
  }
  for (auto failure : {MockFailure::Type, MockFailure::Add, MockFailure::Enable}) {
    reset();
    mockFailure = failure;
    mockFailedPin = 7;
    HalGPIO input;
    input.attachActivityInterrupts();
    assert(!input.activityInterruptsReady && mockErrors == 1);
    assert(!mockHandlers[0].fn && !mockHandlers[7].fn && mockHandlers[48].fn == otherOwner);
    assert(mockRemoves == (failure == MockFailure::Enable ? 2u : 1u));
    input.waitForActivity(1000);
    assert(mockTakes == 0 && mockDelay == 10);
  }
  reset();
  BoardConfig::ACTIVE.input.power = 63;
  {
    HalGPIO input;
    input.attachActivityInterrupts();
    assert(!input.activityInterruptsReady && mockRemoves == 2);
  }
  std::puts("PASS real HAL wake: pending edges, tick rounding, shared ownership and installation-failure rollback");
}
