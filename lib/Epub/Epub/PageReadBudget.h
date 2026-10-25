#pragma once

#include <cstddef>

// Optional speculative page loads account for payloads and allocator overhead
// before allocating. Ordinary page loads keep their existing format support.
class PageReadBudget {
 public:
  explicit PageReadBudget(size_t bytes) : remaining(bytes) {}

  bool claim(size_t bytes) {
    if (bytes == 0) return true;
    static constexpr size_t ALLOCATION_OVERHEAD = 32;
    if (remaining < ALLOCATION_OVERHEAD || bytes > remaining - ALLOCATION_OVERHEAD) return false;
    remaining -= bytes + ALLOCATION_OVERHEAD;
    return true;
  }

 private:
  size_t remaining;
};
