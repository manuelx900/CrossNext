#pragma once
#include "CollectionFolder.h"

namespace collections {
// Edit-session memoization only; saved membership remains owned by Selection.
class FolderStates {
 public:
  bool begin();
  void clear();
  void close();
  bool get(const char* path, FolderSummary& summary, bool touch = true);
  void put(const char* path, FolderSummary summary);
  void invalidate(const char* path, bool subtree);
  void changed(const char* path, int64_t delta, bool subtree, bool checked, bool empty);

 private:
  static constexpr unsigned Capacity = 64;
  struct Entry {
    char path[PathBytes];
    FolderSummary summary;
    uint32_t used;
    bool valid;
  };
  HalMemory::PsramBuffer arena_;
  uint32_t clock_ = 0;
  Entry* entries() { return reinterpret_cast<Entry*>(arena_.get()); }
};
}  // namespace collections
