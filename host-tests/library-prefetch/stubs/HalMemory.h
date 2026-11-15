#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
class HalMemory {
 public:
  using PsramBuffer = std::unique_ptr<uint8_t[]>;
  static inline bool fail = false;
  static inline unsigned allocations = 0;
  struct HeapStats {
    size_t freeBytes, largestBlockBytes;
  };
  static inline size_t freeBytes = 8 * 1024 * 1024, largestBlockBytes = 8 * 1024 * 1024;
  static HeapStats getPsramHeap() { return {freeBytes, largestBlockBytes}; }
  static PsramBuffer allocatePsram(size_t size) {
    ++allocations;
    return fail ? nullptr : PsramBuffer(new (std::nothrow) uint8_t[size]);
  }
};
