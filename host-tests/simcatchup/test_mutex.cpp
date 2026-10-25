#include <freertos/semphr.h>

#include <chrono>
#include <cstdio>
#include <future>
#include <thread>

namespace {
int checks = 0, failures = 0;
void check(bool condition, const char* label) {
  ++checks;
  if (!condition) {
    ++failures;
    std::printf("FAIL %s\n", label);
  }
}
void contendedTake(SemaphoreHandle_t mutex, uint32_t ticks) {
  check(xSemaphoreTake(mutex, portMAX_DELAY), "owner takes mutex");
  std::promise<bool> result;
  auto answer = result.get_future();
  std::thread contender([&] {
    const bool acquired = xSemaphoreTake(mutex, ticks);
    result.set_value(acquired);
    if (acquired) xSemaphoreGive(mutex);
  });
  const bool completed = answer.wait_for(std::chrono::seconds(1)) == std::future_status::ready;
  check(completed, "contended zero/finite take returns while owner still holds mutex");
  xSemaphoreGive(mutex);
  check(!answer.get(), "contended zero/finite take fails");
  contender.join();
}
}  // namespace

int main() {
  auto mutex = xSemaphoreCreateMutex();
  check(xSemaphoreTake(mutex, 0), "uncontended try succeeds");
  check(xSemaphoreTake(mutex, 0), "recursive try succeeds");
  xSemaphoreGive(mutex);
  xSemaphoreGive(mutex);
  contendedTake(mutex, 0);
  contendedTake(mutex, 2);
  check(xSemaphoreTake(mutex, 0), "try succeeds after release");
  xSemaphoreGive(mutex);
  delete sim_semaphore_detail::asMutex(mutex);

  auto* task = xTaskGetCurrentTaskHandle();
  for (int i = 0; i < 7; ++i) xTaskNotify(task, 1, eIncrement);
  check(ulTaskNotifyTake(pdTRUE, 0) == 7, "clear returns full notification count");
  check(ulTaskNotifyTake(pdTRUE, 0) == 0, "clear leaves no redundant repaint wake");
  for (int i = 0; i < 3; ++i) xTaskNotify(task, 1, eIncrement);
  check(ulTaskNotifyTake(pdFALSE, 0) == 3, "decrement returns previous count");
  check(ulTaskNotifyTake(pdTRUE, 0) == 2, "decrement retained remaining count");
  check(ulTaskNotifyTake(pdTRUE, 1) == 0, "empty finite wait times out");

  auto binary = xSemaphoreCreateBinary();
  check(!xSemaphoreTake(binary, 0), "empty binary try fails");
  xSemaphoreGive(binary);
  check(xSemaphoreTake(binary, 0), "given binary try succeeds");
  check(!xSemaphoreTake(binary, 0), "binary consumed once");
  delete sim_semaphore_detail::asBinary(binary);
  std::printf("%d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}
