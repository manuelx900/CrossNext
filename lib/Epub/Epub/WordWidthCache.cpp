#include "WordWidthCache.h"

#include <atomic>
namespace {
WordWidthCache readerMetrics;
std::atomic_flag metricsLease = ATOMIC_FLAG_INIT;
std::atomic<bool> retainMetrics{false}, releaseMetrics{false};
}  // namespace
WordWidthCacheLease::~WordWidthCacheLease() { release(); }
void WordWidthCacheLease::retainReader(bool enabled) {
  retainMetrics.store(enabled);
  releaseMetrics.store(!enabled);
  if (enabled || metricsLease.test_and_set(std::memory_order_acquire)) return;
  readerMetrics = WordWidthCache{};
  metricsLease.clear(std::memory_order_release);
}
void WordWidthCacheLease::acquire() {
  if (owned || !retainMetrics.load() || metricsLease.test_and_set(std::memory_order_acquire)) return;
  if (!retainMetrics.load()) {
    metricsLease.clear(std::memory_order_release);
    return;
  }
  if (HalMemory::getPsramHeap().freeBytes < 2 * 1024 * 1024) readerMetrics = WordWidthCache{};
  owned = true;
}
void WordWidthCacheLease::release() {
  if (!owned) return;
  if (releaseMetrics.load()) readerMetrics = WordWidthCache{};
  owned = false;
  metricsLease.clear(std::memory_order_release);
}
WordWidthCache* WordWidthCacheLease::cache() { return owned ? &readerMetrics : &local; }
