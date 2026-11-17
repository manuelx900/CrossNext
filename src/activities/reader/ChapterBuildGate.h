#pragma once

#include <BlePageTurner.h>

// PSRAM-backed S3 chapter parsing can coexist with a stable radio. Stack startup
// and teardown remain exclusive; devices without PSRAM keep their stop-before-build policy.
inline bool chapterBuildMayRun(const bleturner::Status& radio) {
#if defined(BOARD_HAS_PSRAM) && defined(FREEINK_BLE_STACK_IN_PSRAM)
  return !radio.stopping && !radio.starting;
#else
  return !radio.running && !radio.stopping && !radio.starting;
#endif
}

inline bool chapterBackgroundBuildMayRun() {
#if defined(BOARD_HAS_PSRAM) && defined(FREEINK_BLE_STACK_IN_PSRAM)
  return chapterBuildMayRun(bleturner::status());
#else
  return !bleturner::holdsHeap();
#endif
}

// Asks `ready` until it says yes, sleeping stepMs between asks, and gives up once waitMs has
// passed after the first ask returned (that ask may itself wait: the module's stop takes up to
// 6 s). now and sleep are the host's clock (millis, delay); tests pass their own.
template <typename Ready, typename Now, typename Sleep>
bool waitForBuildRoom(Ready ready, Now now, Sleep sleep, const uint32_t waitMs, const uint32_t stepMs) {
  if (ready()) return true;
  const uint32_t started = now();
  do {
    if (now() - started >= waitMs) return false;
    sleep(stepMs);
  } while (!ready());
  return true;
}
