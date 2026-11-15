#pragma once

#include <HalMemory.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <new>
#include <string>

namespace homeui {
class ShelfCatalogCache {
 public:
  static constexpr int CAPACITY = 60;
  static constexpr size_t TEXT_BYTES = 64 * 1024;
  bool matches(uint32_t libraryRevision, uint32_t artRevision, bool metadata, int limit) const {
    return data && data->valid && data->libraryRevision == libraryRevision && data->artRevision == artRevision &&
           data->metadata == metadata && data->limit >= limit;
  }
  bool begin(uint32_t libraryRevision, uint32_t artRevision, bool metadata, int limit) {
    if (!data) {
      // One bounded catalog in PSRAM; strings outlive Home without pinning the internal heap.
      memory = HalMemory::allocatePsram(sizeof(Data));
      if (!memory) return false;
      data = new (memory.get()) Data;
    }
    data->valid = false;
    data->count = 0;
    data->used = 0;
    data->libraryRevision = libraryRevision;
    data->artRevision = artRevision;
    data->metadata = metadata;
    data->limit = limit;
    return true;
  }
  bool append(uint16_t ordinal, uint64_t hash) {
    if (!data || data->count == CAPACITY) return false;
    auto& entry = data->entries[data->count++];
    entry = {};
    entry.ordinal = ordinal;
    entry.hash = hash;
    return true;
  }
  bool store(int row, const std::string& path, const std::string& title, const std::string& author) {
    if (!data || row < 0 || row >= data->count) return false;
    auto& entry = data->entries[row];
    if (entry.loaded) return true;
    const size_t bytes = (entry.present ? 0 : path.size() + 1) + title.size() + author.size() + 2;
    if (bytes > TEXT_BYTES - data->used) return false;
    if (!entry.present) entry.path = put(path);
    entry.present = true;
    entry.title = put(title);
    entry.author = put(author);
    entry.loaded = true;
    return true;
  }
  void finish() {
    if (data) data->valid = true;
  }
  void invalidate() {
    if (data) data->valid = false;
  }
  void rememberPresent(uint64_t hash, const std::string& path) {
    if (!data || !data->valid || path.size() + 1 > TEXT_BYTES - data->used) return;
    for (int n = 0; n < data->count; ++n) {
      auto& entry = data->entries[n];
      if (entry.hash == hash) {
        if (!entry.present) {
          entry.path = put(path);
          entry.present = true;
        }
        return;
      }
    }
  }
  bool contains(const std::string& path) const {
    if (!data || !data->valid) return false;
    for (int n = 0; n < data->count; ++n)
      if (data->entries[n].present && path == data->text + data->entries[n].path) return true;
    return false;
  }
  int count() const { return data ? data->count : 0; }
  uint16_t ordinal(int row) const {
    return data && row >= 0 && row < data->count ? data->entries[row].ordinal : UINT16_MAX;
  }
  uint64_t hash(int row) const { return data && row >= 0 && row < data->count ? data->entries[row].hash : 0; }
  bool read(int row, std::string& path, std::string& title, std::string& author) const {
    if (!data || !data->valid || row < 0 || row >= data->count || !data->entries[row].loaded) return false;
    const auto& entry = data->entries[row];
    path.assign(data->text + entry.path);
    title.assign(data->text + entry.title);
    author.assign(data->text + entry.author);
    return true;
  }

 private:
  struct Entry {
    uint64_t hash;
    uint32_t path, title, author;
    uint16_t ordinal;
    bool loaded, present;
  };
  struct Data {
    std::array<Entry, CAPACITY> entries{};
    char text[TEXT_BYTES];
    size_t used = 0;
    uint32_t libraryRevision = 0, artRevision = 0;
    int count = 0, limit = 0;
    bool valid = false, metadata = false;
  };
  HalMemory::PsramBuffer memory;
  Data* data = nullptr;
  uint32_t put(const std::string& value) {
    const auto offset = static_cast<uint32_t>(data->used);
    std::memcpy(data->text + offset, value.c_str(), value.size() + 1);
    data->used += value.size() + 1;
    return offset;
  }
};
}  // namespace homeui
