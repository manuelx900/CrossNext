#pragma once

#include <cstdlib>

#include "../../../lib/hal/HalMemory.h"

namespace sdartMemory {
inline bool available = true;
inline bool denyAllocation = false;
inline size_t liveBytes = 0;
inline size_t peakBytes = 0;
}  // namespace sdartMemory

inline HalMemory::HeapStats HalMemory::getPsramHeap() {
  return sdartMemory::available ? HeapStats{8 * 1024 * 1024, 8 * 1024 * 1024, 8 * 1024 * 1024, 8 * 1024 * 1024}
                                : HeapStats{};
}
inline HalMemory::PsramBuffer HalMemory::allocatePsram(size_t bytes) {
  if (!sdartMemory::available || sdartMemory::denyAllocation) return {};
  auto* data = static_cast<uint8_t*>(std::malloc(bytes));
  if (data) {
    sdartMemory::liveBytes += bytes;
    if (sdartMemory::liveBytes > sdartMemory::peakBytes) sdartMemory::peakBytes = sdartMemory::liveBytes;
  }
  return PsramBuffer(data);
}
inline void HalMemory::PsramDeleter::operator()(uint8_t* data) const {
  // The pack test holds one activity-owned allocation at a time.
  sdartMemory::liveBytes = 0;
  std::free(data);
}
