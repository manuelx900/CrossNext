#pragma once

#include <cstddef>
#include <new>

#ifdef BOARD_HAS_PSRAM
#include <HalMemory.h>
#endif

// Font metadata has the same lifetime as its glyph caches and never runs in an ISR.
class FontAllocation {
 public:
  static void* operator new(const std::size_t bytes, const std::nothrow_t&) noexcept {
#ifdef BOARD_HAS_PSRAM
    return HalMemory::allocatePsram(bytes).release();
#else
    return ::operator new(bytes, std::nothrow);
#endif
  }

  static void operator delete(void* memory) noexcept {
#ifdef BOARD_HAS_PSRAM
    HalMemory::PsramDeleter{}(static_cast<uint8_t*>(memory));
#else
    ::operator delete(memory);
#endif
  }

  static void operator delete(void* memory, const std::nothrow_t&) noexcept { operator delete(memory); }
};
