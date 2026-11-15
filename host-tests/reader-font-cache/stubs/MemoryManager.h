#pragma once
#include <cstddef>
namespace freeink {
class MemoryManager {
 public:
  static MemoryManager& instance() {
    static MemoryManager m;
    return m;
  }
  void ensureFree(size_t) {}
};
}  // namespace freeink
