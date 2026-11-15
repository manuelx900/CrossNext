#pragma once

#include <cstddef>
#include <cstdint>

namespace chapterbuild {
// Leave twice the background parser's minimum internal-memory headroom.
constexpr bool reclaimFontCaches(const size_t freeInternal, const size_t largestInternal) {
  return freeInternal < 64 * 1024 || largestInternal < 32 * 1024;
}

constexpr bool showProgress(const uint32_t started, const uint32_t now) {
  return static_cast<uint32_t>(now - started) >= 1000;
}
}  // namespace chapterbuild
