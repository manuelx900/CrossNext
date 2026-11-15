#pragma once

#include <atomic>
#include <cstdint>

// Zero permanently disables memoization if the boot-local counter is exhausted.
class StorageRevision {
  std::atomic<uint32_t> value;

 public:
  explicit StorageRevision(uint32_t initial = 1) : value(initial) {}
  uint32_t get() const { return value.load(); }
  void bump() {
    uint32_t old = value.load();
    while (old && !value.compare_exchange_weak(old, old == UINT32_MAX ? 0 : old + 1)) {
    }
  }
};
