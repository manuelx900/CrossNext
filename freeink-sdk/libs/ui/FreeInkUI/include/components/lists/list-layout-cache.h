#pragma once

#include <cstddef>
#include <cstdint>

namespace freeink::ui {
struct ListRowLayout {
  int16_t height = 0;
  int16_t labelWidth = 0;
  int16_t labelHeight = 0;
  int16_t subtitleHeight = 0;
  int16_t valueWidth = 0;
  uint8_t labelLines = 1;
};

// Caller-owned, bounded geometry only: no text pointers survive a frame.
class ListLayoutCache {
 public:
  struct Key {
    uint64_t first = 14695981039346656037ULL;
    uint64_t second = 5381;
    void add(uint64_t value) {
      for (unsigned n = 0; n < 8; ++n, value >>= 8) {
        first = (first ^ uint8_t(value)) * 1099511628211ULL;
        second = ((second << 5) + second) ^ uint8_t(value);
      }
    }
    void text(const char* value) {
      add(value != nullptr);
      if (value) {
        while (*value) {
          const auto ch = uint8_t(*value++);
          first = (first ^ ch) * 1099511628211ULL;
          second = ((second << 5) + second) ^ ch;
        }
      }
      add(0);
    }
    bool operator==(const Key& other) const { return first == other.first && second == other.second; }
  };
  struct Entry {
    Key key{};
    ListRowLayout layout{};
    uint32_t used = 0;
  };
  void bind(Entry* entries, size_t capacity) {
    entries_ = entries;
    capacity_ = entries ? capacity : 0;
    clear();
  }
  void clear() {
    for (size_t n = 0; n < capacity_; ++n) entries_[n].used = 0;
    clock_ = 0;
  }
  bool find(const Key& key, ListRowLayout& layout) {
    tick();
    for (size_t n = bucket(key), end = n + ways(); n < end; ++n) {
      auto& entry = entries_[n];
      if (entry.used && entry.key == key) {
        entry.used = clock_;
        layout = entry.layout;
        return true;
      }
    }
    return false;
  }
  void store(const Key& key, const ListRowLayout& layout) {
    if (!capacity_) return;
    tick();
    size_t slot = bucket(key);
    for (size_t n = slot, end = n + ways(); n < end; ++n) {
      if (!entries_[n].used || entries_[n].key == key) {
        slot = n;
        break;
      }
      if (entries_[n].used < entries_[slot].used) slot = n;
    }
    entries_[slot] = {key, layout, clock_};
  }

 private:
  Entry* entries_ = nullptr;
  size_t capacity_ = 0;
  uint32_t clock_ = 0;
  size_t ways() const { return capacity_ < 4 ? capacity_ : 4; }
  size_t bucket(const Key& key) const { return capacity_ ? (key.first % (capacity_ / ways())) * ways() : 0; }
  void tick() {
    if (++clock_ == 0) {
      clear();
      ++clock_;
    }
  }
};
}  // namespace freeink::ui
