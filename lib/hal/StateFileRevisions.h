#pragma once

#include <array>

#include "StorageMutation.h"
#include "StorageRevision.h"

// Accessed under storageMutex. Exact canonical identities avoid path/hash collisions.
class StateFileRevisions {
 public:
  uint32_t revision(StorageStateKey key, bool changed = false) {
    if (!key.valid() || !serial.get()) return 0;
    for (auto& entry : entries) {
      if (entry.kind != key.kind || entry.book != key.book) continue;
      if (changed) {
        serial.bump();
        entry.version = serial.get();
      }
      return entry.version;
    }
    serial.bump();
    auto& entry = entries[next++ % entries.size()];
    entry = {key.book, serial.get(), key.kind};
    return entry.version;
  }

 private:
  struct Entry {
    uint64_t book = 0;
    uint32_t version = 0;
    StorageStateKey::Kind kind = StorageStateKey::Kind::None;
  };
  static_assert(sizeof(Entry) <= 16);
  // 1 KiB, present before heap setup; no fallible allocation or SD work for revision queries.
  std::array<Entry, 64> entries{};
  StorageRevision serial;
  uint32_t next = 0;
};
