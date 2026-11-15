#pragma once
#include <cassert>
using SemaphoreHandle_t = void*;
inline int storageLockDepth = 0;
constexpr unsigned portMAX_DELAY = ~0u;
inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutex() { return reinterpret_cast<void*>(1); }
inline int xSemaphoreTakeRecursive(SemaphoreHandle_t, unsigned) {
  ++storageLockDepth;
  return 1;
}
inline int xSemaphoreGiveRecursive(SemaphoreHandle_t) {
  assert(storageLockDepth > 0);
  --storageLockDepth;
  return 1;
}
