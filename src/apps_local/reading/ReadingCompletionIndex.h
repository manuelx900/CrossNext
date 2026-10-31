#pragma once

#include <cstring>

#include "ReadingStatsCore.h"

namespace readingstats {
// Non-owning, bounded index of completion events. The activity owns one PSRAM
// allocation; ordinary reading snapshots need no entry and cause no SD lookup.
class CompletionIndex {
 public:
  struct Entry {
    uint8_t document[16];
    uint64_t completionOffset;
  };
  static constexpr size_t kMaxSlots = 32768;
  static size_t slotsFor(uint64_t records) {
    size_t slots = 16;
    while (slots < kMaxSlots && slots / 2 < records) slots *= 2;
    return slots;
  }
  void reset(Entry* entries = nullptr, size_t capacity = 0) {
    table = entries;
    slots = capacity;
    if (table) std::memset(table, 0, slots * sizeof(Entry));
  }
  bool apply(const Record& record, uint64_t offset) {
    if (!record.firstCompletion && record.kind != RecordKind::Reopen) return true;
    uint8_t key[16];
    if (!keyFor(record.book.document, key) || !slots || offset < 16) return false;
    size_t at = checksum(key, sizeof(key)) & (slots - 1);
    for (size_t n = 0; n < slots; ++n, at = (at + 1) & (slots - 1)) {
      auto& entry = table[at];
      if (entry.completionOffset && std::memcmp(entry.document, key, sizeof(key))) continue;
      std::memcpy(entry.document, key, sizeof(key));
      entry.completionOffset = record.firstCompletion ? offset : 1;
      return true;
    }
    return false;
  }
  bool current(const Record& record, uint64_t offset) const {
    if (!record.firstCompletion || !slots) return false;
    uint8_t key[16];
    if (!keyFor(record.book.document, key)) return false;
    size_t at = checksum(key, sizeof(key)) & (slots - 1);
    for (size_t n = 0; n < slots; ++n, at = (at + 1) & (slots - 1)) {
      const auto& entry = table[at];
      if (!entry.completionOffset) return false;
      if (!std::memcmp(entry.document, key, sizeof(key))) return entry.completionOffset == offset;
    }
    return false;
  }
  bool completed(const char* document) const {
    uint8_t key[16];
    if (!slots || !keyFor(document, key)) return false;
    size_t at = checksum(key, sizeof(key)) & (slots - 1);
    for (size_t n = 0; n < slots; ++n, at = (at + 1) & (slots - 1)) {
      const auto& entry = table[at];
      if (!entry.completionOffset) return false;
      if (!std::memcmp(entry.document, key, sizeof(key))) return entry.completionOffset > 1;
    }
    return false;
  }

 private:
  Entry* table = nullptr;
  size_t slots = 0;
  static bool keyFor(const char* document, uint8_t* key) {
    if (!validDocument(document)) return false;
    const auto nibble = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
    for (int n = 0; n < 16; ++n) key[n] = (nibble(document[2 * n]) << 4) | nibble(document[2 * n + 1]);
    return true;
  }
};
}  // namespace readingstats
