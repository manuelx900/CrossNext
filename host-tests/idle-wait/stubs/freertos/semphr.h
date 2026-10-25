#pragma once
#include <cstdint>
using TickType_t = uint32_t;
using BaseType_t = int;
constexpr BaseType_t pdFALSE = 0, pdTRUE = 1;
struct StaticSemaphore_t {
  bool signaled = false;
};
using SemaphoreHandle_t = StaticSemaphore_t*;
inline bool mockCreateFails = false, mockTakeSignaled = false;
inline unsigned mockTakes = 0, mockIsrYields = 0;
inline TickType_t mockTicks = 0;
inline SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t* memory) {
  return mockCreateFails ? nullptr : memory;
}
inline BaseType_t xSemaphoreGive(SemaphoreHandle_t s) {
  s->signaled = true;
  return pdTRUE;
}
inline BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t s, BaseType_t* wake) {
  *wake = pdTRUE;
  return xSemaphoreGive(s);
}
inline BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t ticks) {
  ++mockTakes;
  mockTicks = ticks;
  mockTakeSignaled = s->signaled;
  s->signaled = false;
  return mockTakeSignaled ? pdTRUE : pdFALSE;
}
#define pdMS_TO_TICKS(ms) ((ms) / 10)
#define portYIELD_FROM_ISR() \
  do {                       \
    ++mockIsrYields;         \
  } while (0)
